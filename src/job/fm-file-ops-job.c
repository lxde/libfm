/*
 *      fm-file-ops-job.c
 *
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
 *      Copyright 2012 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/**
 * SECTION:fm-file-ops-job
 * @short_description: Job to do something with files.
 * @title: FmFileOpsJob
 *
 * @include: libfm/fm-file-ops-job.h
 *
 * The #FmFileOpsJob can be used to do some file operation such as move,
 * copy, delete, change file attributes, etc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-file-ops-job.h"
#include "fm-file-ops-job-xfer.h"
#include "fm-file-ops-job-delete.h"
#include "fm-file-ops-job-change-attr.h"
#include "fm-marshal.h"
#include "fm-file-info-job.h"
#include "glib-compat.h"

enum
{
    PREPARED,
    CUR_FILE,
    PERCENT,
    ASK_RENAME,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static void fm_file_ops_job_finalize              (GObject *object);

static gboolean fm_file_ops_job_run(FmJob* fm_job);
/* static void fm_file_ops_job_cancel(FmJob* job); */

/* funcs for io jobs */
static gboolean _fm_file_ops_job_link_run(FmFileOpsJob* job);


G_DEFINE_TYPE(FmFileOpsJob, fm_file_ops_job, FM_TYPE_JOB);

static void fm_file_ops_job_dispose(GObject *object)
{
    FmFileOpsJob *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FILE_OPS_JOB(object));

    self = (FmFileOpsJob*)object;

    if(self->srcs)
    {
        fm_path_list_unref(self->srcs);
        self->srcs = NULL;
    }
    if(self->dest)
    {
        fm_path_unref(self->dest);
        self->dest = NULL;
    }

    G_OBJECT_CLASS(fm_file_ops_job_parent_class)->dispose(object);
}

static void fm_file_ops_job_class_init(FmFileOpsJobClass *klass)
{
    GObjectClass *g_object_class;
    FmJobClass* job_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_file_ops_job_dispose;
    g_object_class->finalize = fm_file_ops_job_finalize;

    job_class = FM_JOB_CLASS(klass);
    job_class->run = fm_file_ops_job_run;

    /**
     * FmFileOpsJob::prepared:
     * @job: a job object which emitted the signal
     *
     * The #FmFileOpsJob::prepared signal is emitted when preparation
     * of the file operation is done and @job is ready to start
     * copying/deleting...
     *
     * Since: 0.1.10
     */
    signals[PREPARED] =
        g_signal_new( "prepared",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, prepared ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0 );

    /**
     * FmFileOpsJob::cur-file:
     * @job: a job object which emitted the signal
     * @file: (const char *) file which is processing
     *
     * The #FmFileOpsJob::cur-file signal is emitted when @job is about
     * to start operation on the @file.
     *
     * Since: 0.1.0
     */
    signals[CUR_FILE] =
        g_signal_new( "cur-file",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, cur_file ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER );

    /**
     * FmFileOpsJob::percent:
     * @job: a job object which emitted the signal
     * @percent: current ratio of completed job size to full job size
     *
     * The #FmFileOpsJob::percent signal is emitted when one more file
     * operation is completed.
     *
     * Since: 0.1.0
     */
    signals[PERCENT] =
        g_signal_new( "percent",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, percent ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE, 1, G_TYPE_UINT );

    /**
     * FmFileOpsJob::ask-rename:
     * @job: a job object which emitted the signal
     * @src: (#FmFileInfo *) source file
     * @dest: (#FmFileInfo *) destination directory
     * @new_name: (char **) pointer to receive new name
     *
     * The #FmFileOpsJob::ask-rename signal is emitted when file operation
     * raises a conflict because file with the same name already exists
     * in the directory @dest. Signal handler should find a decision how
     * to resolve the situation. If there is more than one handler connected
     * to the signal then only one of them will receive it.
     *
     * Return value: a #FmFileOpOption decision.
     *
     * Since: 0.1.0
     */
    signals[ASK_RENAME] =
        g_signal_new( "ask-rename",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, ask_rename ),
                      g_signal_accumulator_first_wins, NULL,
                      fm_marshal_INT__POINTER_POINTER_POINTER,
                      G_TYPE_INT, 3, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER );

}


static void fm_file_ops_job_finalize(GObject *object)
{
    FmFileOpsJob *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FILE_OPS_JOB(object));

    self = (FmFileOpsJob*)object;

    g_assert(self->src_folder_mon == NULL);
    g_assert(self->dest_folder_mon == NULL);

    G_OBJECT_CLASS(fm_file_ops_job_parent_class)->finalize(object);
}


static void fm_file_ops_job_init(FmFileOpsJob *self)
{
    fm_job_init_cancellable(FM_JOB(self));

    /* for chown */
    self->uid = -1;
    self->gid = -1;
}

/**
 * fm_file_ops_job_new
 * @type: type of file operation the new job will handle
 * @files: list of source files to perform operation
 *
 * Creates new #FmFileOpsJob which can be used in #FmJob API.
 *
 * Returns: a new #FmFileOpsJob object.
 *
 * Since: 0.1.0
 */
FmFileOpsJob *fm_file_ops_job_new(FmFileOpType type, FmPathList* files)
{
    FmFileOpsJob* job = (FmFileOpsJob*)g_object_new(FM_FILE_OPS_JOB_TYPE, NULL);
    job->srcs = fm_path_list_ref(files);
    job->type = type;
    return job;
}


static gboolean fm_file_ops_job_run(FmJob* fm_job)
{
    FmFileOpsJob* job = FM_FILE_OPS_JOB(fm_job);
    switch(job->type)
    {
    case FM_FILE_OP_COPY:
        return _fm_file_ops_job_copy_run(job);
    case FM_FILE_OP_MOVE:
        return _fm_file_ops_job_move_run(job);
    case FM_FILE_OP_TRASH:
        return _fm_file_ops_job_trash_run(job);
    case FM_FILE_OP_UNTRASH:
        return _fm_file_ops_job_untrash_run(job);
    case FM_FILE_OP_DELETE:
        return _fm_file_ops_job_delete_run(job);
    case FM_FILE_OP_LINK:
        return _fm_file_ops_job_link_run(job);
    case FM_FILE_OP_CHANGE_ATTR:
        return _fm_file_ops_job_change_attr_run(job);
    case FM_FILE_OP_NONE: ;
    }
    return FALSE;
}


/**
 * fm_file_ops_job_set_dest
 * @job: a job to set
 * @dest: destination path
 *
 * Sets destination path for operations FM_FILE_OP_MOVE, FM_FILE_OP_COPY,
 * or FM_FILE_OP_LINK.
 *
 * This API may be used only before @job is started.
 *
 * Since: 0.1.0
 */
void fm_file_ops_job_set_dest(FmFileOpsJob* job, FmPath* dest)
{
    job->dest = fm_path_ref(dest);
}

/**
 * fm_file_ops_job_get_dest
 * @job: a job to inspect
 *
 * Retrieves the destination path for operation. If type of operation
 * in not FM_FILE_OP_MOVE, FM_FILE_OP_COPY, or FM_FILE_OP_LINK then
 * result of this call is undefined. The returned value is owned by
 * @job and should be not freed by caller.
 *
 * Returns: (transfer none): the #FmPath which was set by previous call
 * to fm_file_ops_job_set_dest().
 *
 * Since: 0.1.0
 */
FmPath* fm_file_ops_job_get_dest(FmFileOpsJob* job)
{
    return job->dest;
}

/**
 * fm_file_ops_job_set_chmod
 * @job: a job to set
 * @new_mode: which bits of file mode should be set
 * @new_mode_mask: which bits of file mode should be reset
 *
 * Sets that files for file operation FM_FILE_OP_CHANGE_ATTR should have
 * file mode changed according to @new_mode_mask and @new_mode: bits
 * that are present only in @new_mode_mask will be set to 0, and bits
 * that are present in both @new_mode_mask and @new_mode will be set to 1.
 *
 * This API may be used only before @job is started.
 *
 * Since: 0.1.0
 */
void fm_file_ops_job_set_chmod(FmFileOpsJob* job, mode_t new_mode, mode_t new_mode_mask)
{
    job->new_mode = new_mode;
    job->new_mode_mask = new_mode_mask;
}

/**
 * fm_file_ops_job_set_chown
 * @job: a job to set
 * @uid: user id to set as file owner
 * @gid: group id to set as file group
 *
 * Sets that files for file operation FM_FILE_OP_CHANGE_ATTR should have
 * owner or group changed. If @uid >= 0 then @job will try to change
 * owner of files. If @gid >= 0 then @job will try to change group of
 * files.
 *
 * This API may be used only before @job is started.
 *
 * Since: 0.1.0
 */
void fm_file_ops_job_set_chown(FmFileOpsJob* job, gint uid, gint gid)
{
    job->uid = uid;
    job->gid = gid;
}

/**
 * fm_file_ops_job_set_recursive
 * @job: a job to set
 * @recursive: recursion attribute to set
 *
 * Sets 'recursive' attribute for file operation according to @recursive.
 * If @recursive is %TRUE then file operation @job will try to do all
 * operations recursively.
 *
 * This API may be used only before @job is started.
 *
 * Since: 0.1.0
 */
void fm_file_ops_job_set_recursive(FmFileOpsJob* job, gboolean recursive)
{
    job->recursive = recursive;
}

static gpointer emit_cur_file(FmJob* job, gpointer cur_file)
{
    g_signal_emit(job, signals[CUR_FILE], 0, (const char*)cur_file);
    return NULL;
}

/**
 * fm_file_ops_job_emit_cur_file
 * @job: the job to emit signal
 * @cur_file: the data to emit
 *
 * Emits the #FmFileOpsJob::cur-file signal in main thread.
 *
 * This API is private to #FmFileOpsJob and should not be used outside
 * of libfm implementation.
 *
 * Since: 0.1.0
 */
void fm_file_ops_job_emit_cur_file(FmFileOpsJob* job, const char* cur_file)
{
    fm_job_call_main_thread(FM_JOB(job), emit_cur_file, (gpointer)cur_file);
}

static gpointer emit_percent(FmJob* job, gpointer percent)
{
    g_signal_emit(job, signals[PERCENT], 0, GPOINTER_TO_UINT(percent));
    return NULL;
}

/**
 * fm_file_ops_job_emit_percent
 * @job: the job to emit signal
 *
 * Emits the #FmFileOpsJob::percent signal in main thread.
 *
 * This API is private to #FmFileOpsJob and should not be used outside
 * of libfm implementation.
 *
 * Since: 0.1.0
 */
void fm_file_ops_job_emit_percent(FmFileOpsJob* job)
{
    guint percent;
    if(job->total > 0)
    {
        gdouble dpercent = (gdouble)(job->finished + job->current_file_finished) / job->total;
        percent = (guint)(dpercent * 100);
        if(percent > 100)
            percent = 100;
    }
    else
        percent = 100;

    if( percent > job->percent )
    {
        fm_job_call_main_thread(FM_JOB(job), emit_percent, GUINT_TO_POINTER(percent));
        job->percent = percent;
    }
}

static gpointer emit_prepared(FmJob* job, gpointer user_data)
{
    g_signal_emit(job, signals[PREPARED], 0);
    return NULL;
}

/**
 * fm_file_ops_job_emit_prepared
 * @job: the job to emit signal
 *
 * Emits the #FmFileOpsJob::prepared signal in main thread.
 *
 * This API is private to #FmFileOpsJob and should not be used outside
 * of libfm implementation.
 *
 * Since: 0.1.10
 */
void fm_file_ops_job_emit_prepared(FmFileOpsJob* job)
{
    fm_job_call_main_thread(FM_JOB(job), emit_prepared, NULL);
}

struct AskRename
{
    FmFileInfo* src_fi;
    FmFileInfo* dest_fi;
    char* new_name;
    FmFileOpOption ret;
};

static gpointer emit_ask_rename(FmJob* job, gpointer input_data)
{
#define data ((struct AskRename*)input_data)
    g_signal_emit(job, signals[ASK_RENAME], 0, data->src_fi, data->dest_fi, &data->new_name, &data->ret);
#undef data
    return NULL;
}

/**
 * fm_file_ops_job_ask_rename
 * @job: a job which asked
 * @src: source file descriptor
 * @src_inf: source file information
 * @dest: destination descriptor
 * @new_dest: pointer to get new destination
 *
 * Asks the user in main thread how to resolve conflict if file being
 * copied or moved already exists in destination directory. Ask is done
 * by emitting the #FmFileOpsJob::ask-rename signal.
 *
 * This API is private to #FmFileOpsJob and should not be used outside
 * of libfm implementation.
 *
 * Returns: a decision how to resolve conflict.
 *
 * Since: 0.1.0
 */
FmFileOpOption fm_file_ops_job_ask_rename(FmFileOpsJob* job, GFile* src, GFileInfo* src_inf, GFile* dest, GFile** new_dest)
{
    struct AskRename data;
    FmFileInfoJob* fijob = fm_file_info_job_new(NULL, 0);
    FmFileInfo *src_fi = NULL, *dest_fi = NULL;
    FmPath *tmp;

    if( !src_inf )
        fm_file_info_job_add_gfile(fijob, src);
    else
    {
        tmp = fm_path_new_for_gfile(src);
        src_fi = fm_file_info_new_from_gfileinfo(tmp, src_inf);
        fm_path_unref(tmp);
    }
    fm_file_info_job_add_gfile(fijob, dest);

    fm_job_set_cancellable(FM_JOB(fijob), fm_job_get_cancellable(FM_JOB(job)));
    fm_job_run_sync(FM_JOB(fijob));

    /* FIXME, handle cancellation correctly */
    if( fm_job_is_cancelled(FM_JOB(fijob)) )
    {
        if(src_fi)
            fm_file_info_unref(src_fi);
        g_object_unref(fijob);
        return 0;
    }

    if(!src_inf)
        src_fi = fm_file_info_list_pop_head(fijob->file_infos);
    dest_fi = fm_file_info_list_pop_head(fijob->file_infos);
    g_object_unref(fijob);

    data.ret = 0;
    data.src_fi = src_fi;
    data.dest_fi = dest_fi;
    data.new_name = NULL;
    fm_job_call_main_thread(FM_JOB(job), emit_ask_rename, (gpointer)&data);

    if(data.ret == FM_FILE_OP_RENAME)
    {
        if(data.new_name)
        {
            GFile* parent = g_file_get_parent(dest);
            *new_dest = g_file_get_child(parent, data.new_name);
            g_object_unref(parent);
            g_free(data.new_name);
        }
    }

    fm_file_info_unref(src_fi);
    fm_file_info_unref(dest_fi);

    return data.ret;
}

static gboolean _fm_file_ops_job_link_run(FmFileOpsJob* job)
{
    gboolean ret = TRUE;
    GFile *dest_dir;
    GList* l;
    FmJob* fmjob = FM_JOB(job);

    dest_dir = fm_path_to_gfile(job->dest);

    /* cannot create links on non-native filesystems */
    if(!g_file_is_native(dest_dir))
    {
        /* FIXME: generate error */
        g_object_unref(dest_dir);
        return FALSE;
    }

    job->total = fm_path_list_get_length(job->srcs);
    g_debug("total files to link: %lu", (gulong)job->total);

    fm_file_ops_job_emit_prepared(job);

    for(l = fm_path_list_peek_head_link(job->srcs);
        !fm_job_is_cancelled(fmjob) && l; l=l->next)
    {
        FmPath* path = FM_PATH(l->data);
        char* src = fm_path_to_str(path);
        GFile* dest = g_file_get_child(dest_dir, fm_path_get_basename(path));
        GError* err = NULL;
        char* dname;

        /* showing currently processed file. */
        dname = fm_path_display_name(path, TRUE);
        fm_file_ops_job_emit_cur_file(job, dname);
        g_free(dname);

        if(!g_file_make_symbolic_link(dest, src, fm_job_get_cancellable(fmjob), &err))
        {
            FmJobErrorAction act = FM_JOB_CONTINUE;
            if(err)
            {
                /* FIXME: ask user to choose another filename for creation */
                act = fm_job_emit_error(fmjob, err, FM_JOB_ERROR_MODERATE);
                g_error_free(err);
                err = NULL;
            }
            if(act == FM_JOB_ABORT)
            {
                g_free(src);
                g_object_unref(dest);
                g_object_unref(dest_dir);
                return FALSE;
            }
            ret = FALSE;
        }
        else if(job->dest_folder_mon)
            g_file_monitor_emit_event(job->dest_folder_mon, dest, NULL, G_FILE_MONITOR_EVENT_CREATED);

        job->finished++;

        /* update progress */
        fm_file_ops_job_emit_percent(job);

        g_free(src);
        g_object_unref(dest);
    }

    /* g_debug("finished: %llu, total: %llu", job->finished, job->total); */
    fm_file_ops_job_emit_percent(job);

    g_object_unref(dest_dir);
    return ret;
}
