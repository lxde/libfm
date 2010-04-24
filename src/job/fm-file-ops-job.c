/*
 *      fm-file-ops-job.c
 *
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
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

#include "fm-file-ops-job.h"
#include "fm-file-ops-job-xfer.h"
#include "fm-file-ops-job-delete.h"
#include "fm-file-ops-job-change-attr.h"
#include "fm-marshal.h"
#include "fm-file-info-job.h"

enum
{
    PREPARED,
	CUR_FILE,
	PERCENT,
    ASK_RENAME,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

static void fm_file_ops_job_finalize  			(GObject *object);

static gboolean fm_file_ops_job_run(FmJob* fm_job);
/* static void fm_file_ops_job_cancel(FmJob* job); */

/* funcs for io jobs */
gboolean _fm_file_ops_job_link_run(FmFileOpsJob* job);


G_DEFINE_TYPE(FmFileOpsJob, fm_file_ops_job, FM_TYPE_JOB);

static void fm_file_ops_job_class_init(FmFileOpsJobClass *klass)
{
	GObjectClass *g_object_class;
	FmJobClass* job_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_ops_job_finalize;

	job_class = FM_JOB_CLASS(klass);
	job_class->run = fm_file_ops_job_run;
	job_class->finished = NULL;

    /* preperation of the file operation is done, ready to start copying/deleting... */
    signals[PREPARED] =
        g_signal_new( "prepared",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, cur_file ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0 );

    signals[CUR_FILE] =
        g_signal_new( "cur-file",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, cur_file ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[PERCENT] =
        g_signal_new( "percent",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, percent ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE, 1, G_TYPE_UINT );

    signals[ASK_RENAME] =
        g_signal_new( "ask-rename",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, ask_rename ),
                      NULL, NULL,
                      fm_marshal_INT__POINTER_POINTER_POINTER,
                      G_TYPE_INT, 3, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER );

}


static void fm_file_ops_job_finalize(GObject *object)
{
	FmFileOpsJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_FILE_OPS_JOB(object));

	self = FM_FILE_OPS_JOB(object);

	if(self->srcs);
		fm_list_unref(self->srcs);
	if(self->dest)
		fm_path_unref(self->dest);

    g_assert(self->src_folder_mon == NULL);
    g_assert(self->dest_folder_mon == NULL);

	G_OBJECT_CLASS(fm_file_ops_job_parent_class)->finalize(object);
}


static void fm_file_ops_job_init(FmFileOpsJob *self)
{
	fm_job_init_cancellable((FmJob*)self);

    /* for chown */
    self->uid = -1;
    self->gid = -1;
}


FmJob *fm_file_ops_job_new(FmFileOpType type, FmPathList* files)
{
	FmFileOpsJob* job = (FmFileOpsJob*)g_object_new(FM_FILE_OPS_JOB_TYPE, NULL);
	job->srcs = fm_list_ref(files);
	job->type = type;
	return (FmJob*)job;
}


gboolean fm_file_ops_job_run(FmJob* fm_job)
{
	FmFileOpsJob* job = (FmFileOpsJob*)fm_job;

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
	}
	return FALSE;
}


void fm_file_ops_job_set_dest(FmFileOpsJob* job, FmPath* dest)
{
	job->dest = fm_path_ref(dest);
}

FmPath* fm_file_ops_job_get_dest(FmFileOpsJob* job)
{
    return job->dest;
}

void fm_file_ops_job_set_chmod(FmFileOpsJob* job, mode_t new_mode, mode_t new_mode_mask)
{
    job->new_mode = new_mode;
    job->new_mode_mask = new_mode_mask;
}

void fm_file_ops_job_set_chown(FmFileOpsJob* job, guint uid, guint gid)
{
    job->uid = uid;
    job->gid = gid;
}

void fm_file_ops_job_set_recursive(FmFileOpsJob* job, gboolean recursive)
{
    job->recursive = recursive;
}

static void emit_cur_file(FmFileOpsJob* job, const char* cur_file)
{
	g_signal_emit(job, signals[CUR_FILE], 0, cur_file);
}

void fm_file_ops_job_emit_cur_file(FmFileOpsJob* job, const char* cur_file)
{
	fm_job_call_main_thread(FM_JOB(job), emit_cur_file, cur_file);
}

static void emit_percent(FmFileOpsJob* job, gpointer percent)
{
	g_signal_emit(job, signals[PERCENT], 0, (guint)percent);
}

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
    	fm_job_call_main_thread(FM_JOB(job), emit_percent, (gpointer)percent);
        job->percent = percent;
    }
}

static void emit_prepared(FmFileOpsJob* job, gpointer user_data)
{
	g_signal_emit(job, signals[PREPARED], 0);
}

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

static void emit_ask_rename(FmFileOpsJob* job, struct AskRename* data)
{
    g_signal_emit(job, signals[ASK_RENAME], 0, data->src_fi, data->dest_fi, &data->new_name, &data->ret);
}

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
        src_fi = fm_list_pop_head(fijob->file_infos);
    dest_fi = fm_list_pop_head(fijob->file_infos);
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

gboolean _fm_file_ops_job_link_run(FmFileOpsJob* job)
{
	GList* l;
    GError* err = NULL;
    FmJob* fmjob = FM_JOB(job);
    job->total = fm_list_get_length(job->srcs);
	l = fm_list_peek_head_link(job->srcs);
	for(; !fm_job_is_cancelled(fmjob) && l;l=l->next)
	{
		GFile* gf = fm_path_to_gfile((FmPath*)l->data);
        gboolean ret = g_file_make_symbolic_link(gf, "", fm_job_get_cancellable(fmjob), &err);
		g_object_unref(gf);
        if(!ret)
        {
            if( err->domain == G_IO_ERROR && err->code == G_IO_ERROR_NOT_SUPPORTED)
            {
//                fm_job_emit_error();
                return FALSE;
            }
        }
        else
            ++job->finished;
        fm_file_ops_job_emit_percent(job);
	}
    return TRUE;
}
