/*
 *      fm-folder.c
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

#include "fm-folder.h"
#include "fm-monitor.h"
#include "fm-marshal.h"
#include <string.h>

enum {
    FILES_ADDED,
    FILES_REMOVED,
    FILES_CHANGED,
    LOADED,
    UNMOUNT,
    CHANGED,
    REMOVED,
    CONTENT_CHANGED,
    FS_INFO,
    ERROR,
    N_SIGNALS
};

static FmFolder* fm_folder_new_internal(FmPath* path, GFile* gf);
static FmFolder* fm_folder_get_internal(FmPath* path, GFile* gf);
static void fm_folder_finalize(GObject *object);
static void fm_folder_content_changed(FmFolder* folder);

static void on_file_info_finished(FmFileInfoJob* job, FmFolder* folder);
static gboolean on_idle(FmFolder* folder);

G_DEFINE_TYPE(FmFolder, fm_folder, G_TYPE_OBJECT);

static GList* _fm_folder_get_file_by_name(FmFolder* folder, const char* name);

static guint signals[N_SIGNALS];
static GHashTable* hash = NULL; /* FIXME: should this be guarded with a mutex? */

static void fm_folder_class_init(FmFolderClass *klass)
{
    GObjectClass *g_object_class;
    FmFolderClass* folder_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_folder_finalize;
    fm_folder_parent_class = (GObjectClass*)g_type_class_peek(G_TYPE_OBJECT);

    folder_class = FM_FOLDER_CLASS(klass);
    folder_class->content_changed = fm_folder_content_changed;

    /*
    * files-added is emitted when there is a new file created in the dir.
    * The param is GList* of the newly added file.
    */
    signals[ FILES_ADDED ] =
        g_signal_new ( "files-added",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, files_added ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    /*
    * files-removed is emitted when there is a file deleted in the dir.
    * The param is GList* of the removed files.
    */
    signals[ FILES_REMOVED ] =
        g_signal_new ( "files-removed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, files_removed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    /*
    * file-changed is emitted when there is a file changed in the dir.
    * The param is VFSFileInfo of the newly created file.
    */
    signals[ FILES_CHANGED ] =
        g_signal_new ( "files-changed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, files_changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[ LOADED ] =
        g_signal_new ( "loaded",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, loaded ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /* emitted when the folder is unmounted */
    signals[ UNMOUNT ] =
        g_signal_new ( "unmount",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, unmount ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /* emitted when the folder itself is changed */
    signals[ CHANGED ] =
        g_signal_new ( "changed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /* emitted when the folder itself is deleted */
    signals[ REMOVED ] =
        g_signal_new ( "removed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, removed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /* emitted when content of the folder is changed (some files are added, removed, changed) */
    signals[ CONTENT_CHANGED ] =
        g_signal_new ( "content-changed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, content_changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /* emitted when filesystem information is available. */
    signals[ FS_INFO ] =
        g_signal_new ( "fs-info",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, fs_info ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /* emitted when errors happen */
    signals[ERROR] =
        g_signal_new( "error",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET ( FmFolderClass, error ),
                      NULL, NULL,
                      fm_marshal_INT__POINTER_INT,
                      G_TYPE_INT, 2, G_TYPE_POINTER, G_TYPE_INT );
}


static void fm_folder_init(FmFolder *folder)
{
    folder->files = fm_file_info_list_new();
}

void on_file_info_finished(FmFileInfoJob* job, FmFolder* folder)
{
    GList* l;
    GSList* files_to_add = NULL;
    GSList* files_to_update = NULL;
    gboolean content_changed = FALSE;

    for(l=fm_list_peek_head_link(job->file_infos);l;l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        GList* l2 = _fm_folder_get_file_by_name(folder, fi->path->name);
        if(l2) /* the file is already in the folder, update */
        {
            FmFileInfo* fi2 = (FmFileInfo*)l2->data;
            /* FIXME: will fm_file_info_copy here cause problems?
             *        the file info might be referenced by others, too.
             *        we're mofifying an object referenced by others.
             *        we should redesign the API, or document this clearly
             *        in future API doc.
             */
            fm_file_info_copy(fi2, fi);
            files_to_update = g_slist_prepend(files_to_update, fi2);
        }
        else
        {
            files_to_add = g_slist_prepend(files_to_add, fi);
            fm_file_info_ref(fi);
            fm_list_push_tail(folder->files, fi);
        }
    }
    if(files_to_add)
    {
        g_signal_emit(folder, signals[FILES_ADDED], 0, files_to_add);
        g_slist_free(files_to_add);
        content_changed = TRUE;
    }
    if(files_to_update)
    {
        g_signal_emit(folder, signals[FILES_CHANGED], 0, files_to_update);
        g_slist_free(files_to_update);
        content_changed = TRUE;
    }

    if(content_changed)
        g_signal_emit(folder, signals[CONTENT_CHANGED], 0);

    folder->pending_jobs = g_slist_remove(folder->pending_jobs, job);
}

gboolean on_idle(FmFolder* folder)
{
    GSList* l;
    FmFileInfoJob* job = NULL;
    FmPath* path;
    folder->idle_handler = 0;
    if(folder->files_to_update || folder->files_to_add)
        job = (FmFileInfoJob*)fm_file_info_job_new(NULL, 0);

    if(folder->files_to_update)
    {
        GSList* prev = NULL;
        for(l=folder->files_to_update;l;)
        {
            /* if a file is already in files_to_add, remove it. */
            if(g_slist_find_custom(folder->files_to_add, l->data, (GCompareFunc)strcmp))
            {
                GSList* tmp = l;
                l=l->next;
                if(G_LIKELY(prev))
                    prev->next = l;
                g_free(tmp->data);
                g_slist_free_1(tmp);
                if(G_UNLIKELY(tmp == folder->files_to_update))
                    folder->files_to_update = l;
                continue;
            }
            path = fm_path_new_child(folder->dir_path, (char*)l->data);
            fm_file_info_job_add(job, path);
            fm_path_unref(path);
            g_free(l->data);

            prev = l;
            l=l->next;
        }
        g_slist_free(folder->files_to_update);
        folder->files_to_update = NULL;
    }

    if(folder->files_to_add)
    {
        for(l=folder->files_to_add;l;l=l->next)
        {
            path = fm_path_new_child(folder->dir_path, (char*)l->data);
            fm_file_info_job_add(job, path);
            fm_path_unref(path);
            g_free(l->data);
        }
        g_slist_free(folder->files_to_add);
        folder->files_to_add = NULL;
    }

    if(job)
    {
        g_signal_connect(job, "finished", on_file_info_finished, folder);
        folder->pending_jobs = g_slist_prepend(folder->pending_jobs, job);
        fm_job_run_async(FM_JOB(job));
    }

    if(folder->files_to_del)
    {
        GSList* ll;
        for(ll=folder->files_to_del;ll;ll=ll->next)
        {
            GList* l= (GList*)ll->data;
            ll->data = l->data;
            fm_list_delete_link_nounref(folder->files , l);
        }
        g_signal_emit(folder, signals[FILES_REMOVED], 0, folder->files_to_del);
        g_slist_foreach(folder->files_to_del, (GFunc)fm_file_info_unref, NULL);
        g_slist_free(folder->files_to_del);
        folder->files_to_del = NULL;

        g_signal_emit(folder, signals[CONTENT_CHANGED], 0);
    }

    return FALSE;
}

static void on_folder_changed(GFileMonitor* mon, GFile* gf, GFile* other, GFileMonitorEvent evt, FmFolder* folder)
{
    GList* l;
    char* name;
/*
    const char* names[]={
        "G_FILE_MONITOR_EVENT_CHANGED",
        "G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT",
        "G_FILE_MONITOR_EVENT_DELETED",
        "G_FILE_MONITOR_EVENT_CREATED",
        "G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED",
        "G_FILE_MONITOR_EVENT_PRE_UNMOUNT",
        "G_FILE_MONITOR_EVENT_UNMOUNTED"
    };
    name = g_file_get_basename(gf);
    g_debug("folder: %p, file %s event: %s", folder, name, names[evt]);
    g_free(name);
*/

    if(g_file_equal(gf, folder->gf))
    {
        g_debug("event of the folder itself: %d", evt);
        /* FIXME: handle unmount events */
        switch(evt)
        {
        case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
            g_debug("folder is going to be unmounted");
            break;
        case G_FILE_MONITOR_EVENT_UNMOUNTED:
            g_signal_emit(folder, signals[UNMOUNT], 0);
            g_debug("folder is unmounted");
            break;
        case G_FILE_MONITOR_EVENT_DELETED:
            g_signal_emit(folder, signals[REMOVED], 0);
            g_debug("folder is deleted");
            break;
        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
            g_signal_emit(folder, signals[CHANGED], 0);
            /* update volume info */
            fm_folder_query_filesystem_info(folder);
            g_debug("folder is changed");
            break;
        }
        return;
    }

    name = g_file_get_basename(gf);
    switch(evt)
    {
    case G_FILE_MONITOR_EVENT_CREATED:
        folder->files_to_add = g_slist_append(folder->files_to_add, name);
        break;
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        folder->files_to_update = g_slist_append(folder->files_to_update, name);
        break;
    case G_FILE_MONITOR_EVENT_DELETED:
        l = _fm_folder_get_file_by_name(folder, name);
        if(l && !g_slist_find(folder->files_to_del, l) )
            folder->files_to_del = g_slist_prepend(folder->files_to_del, l);
        g_free(name);
        break;
    default:
        /* g_debug("folder %p %s event: %s", folder, name, names[evt]); */
        g_free(name);
        return;
    }
    if(!folder->idle_handler)
        folder->idle_handler = g_idle_add_full(G_PRIORITY_LOW, on_idle, folder, NULL);
}

static void on_job_finished(FmDirListJob* job, FmFolder* folder)
{
    GList* l;
    GSList* files = NULL;
    /* actually manually disconnecting from 'finished' signal is not
     * needed since the signal is only emit once, and later the job
     * object will be distroyed very soon. */
    /* g_signal_handlers_disconnect_by_func(job, on_job_finished, folder); */
    for(l = fm_list_peek_head_link(job->files); l; l=l->next)
    {
        FmFileInfo* inf = (FmFileInfo*)l->data;
        files = g_slist_prepend(files, inf);
        fm_list_push_tail(folder->files, inf);
    }
    if(G_LIKELY(files))
        g_signal_emit(folder, signals[FILES_ADDED], 0, files);

    if(job->dir_fi)
        folder->dir_fi = fm_file_info_ref(job->dir_fi);

    folder->job = NULL; /* the job object will be freed in idle handler. */
    g_signal_emit(folder, signals[LOADED], 0);
}

static FmJobErrorAction on_job_err(FmDirListJob* job, GError* err, FmJobErrorSeverity severity, FmFolder* folder)
{
    FmJobErrorAction ret;
    g_signal_emit(folder, signals[ERROR], 0, err, severity, &ret);
    return ret;
}

FmFolder* fm_folder_new_internal(FmPath* path, GFile* gf)
{
    GError* err = NULL;
    FmFolder* folder = (FmFolder*)g_object_new(FM_TYPE_FOLDER, NULL);
    folder->dir_path = fm_path_ref(path);

    folder->gf = (GFile*)g_object_ref(gf);

    folder->mon = fm_monitor_directory(gf, &err);
    if(folder->mon)
        g_signal_connect(folder->mon, "changed", G_CALLBACK(on_folder_changed), folder );
    else
        g_error_free(err);

    fm_folder_reload(folder);
    return folder;
}

FmFolder* fm_folder_get_internal(FmPath* path, GFile* gf)
{
    FmFolder* folder;
    /* FIXME: should we provide a generic FmPath cache in fm-path.c
     * to associate all kinds of data structures with FmPaths? */

    /* FIXME: should creation of the hash table be moved to fm_init()? */
    if( G_LIKELY(hash) )
        folder = (FmFolder*)g_hash_table_lookup(hash, path);
    else
    {
        hash = g_hash_table_new((GHashFunc)fm_path_hash, (GEqualFunc)fm_path_equal);
        folder = NULL;
    }

    if( G_UNLIKELY(!folder) )
    {
        GFile* _gf = NULL;
        if(!gf)
            _gf = gf = fm_path_to_gfile(path);
        folder = fm_folder_new_internal(path, gf);
        if(_gf)
            g_object_unref(_gf);
        g_hash_table_insert(hash, folder->dir_path, folder);
    }
    else
        return (FmFolder*)g_object_ref(folder);
    return folder;
}

static void fm_folder_finalize(GObject *object)
{
    FmFolder *folder;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FOLDER(object));

    folder = FM_FOLDER(object);

    if(folder->job)
    {
        g_signal_handlers_disconnect_by_func(folder->job, on_job_finished, folder);
        g_signal_handlers_disconnect_by_func(folder->job, on_job_err, folder);
        fm_job_cancel(FM_JOB(folder->job)); /* FIXME: is this ok? */
        /* the job will be freed automatically in idle handler. */
    }

    if(folder->pending_jobs)
    {
        GSList* l;
        for(l = folder->pending_jobs;l;l=l->next)
        {
            FmJob* job = FM_JOB(l->data);
            g_signal_handlers_disconnect_by_func(job, on_job_finished, folder);
            fm_job_cancel(job);
            /* the job will be freed automatically in idle handler. */
        }
    }

    /* remove from hash table */
    g_hash_table_remove(hash, folder->dir_path);
    if(folder->dir_path)
        fm_path_unref(folder->dir_path);

    if(folder->dir_fi)
        fm_file_info_unref(folder->dir_fi);

    if(folder->gf)
        g_object_unref(folder->gf);

    if(folder->mon)
    {
        g_signal_handlers_disconnect_by_func(folder->mon, on_folder_changed, folder);
        g_object_unref(folder->mon);
    }

    if(folder->idle_handler)
    {
        g_source_remove(folder->idle_handler);
        if(folder->files_to_add)
        {
            g_slist_foreach(folder->files_to_add, (GFunc)g_free, NULL);
            g_slist_free(folder->files_to_add);
        }
        if(folder->files_to_update)
        {
            g_slist_foreach(folder->files_to_update, (GFunc)g_free, NULL);
            g_slist_free(folder->files_to_update);
        }
        if(folder->files_to_del)
            g_slist_free(folder->files_to_del);
    }

    fm_list_unref(folder->files);

    if(folder->fs_size_cancellable)
    {
        g_cancellable_cancel(folder->fs_size_cancellable);
        g_object_unref(folder->fs_size_cancellable);
    }

    if (G_OBJECT_CLASS(fm_folder_parent_class)->finalize)
        (* G_OBJECT_CLASS(fm_folder_parent_class)->finalize)(object);
}

FmFolder*    fm_folder_get_for_gfile    (GFile* gf)
{
    FmPath* path = fm_path_new_for_gfile(gf);
    FmFolder* folder = fm_folder_new_internal(path, gf);
    fm_path_unref(path);
    return folder;
}

FmFolder* fm_folder_get_for_path_name(const char* path)
{
    FmPath* fm_path = fm_path_new_for_str(path);
    FmFolder* folder = fm_folder_get_internal(fm_path, NULL);
    fm_path_unref(fm_path);
    return folder;
}

/* FIXME: should we use GFile here? */
FmFolder*    fm_folder_get_for_uri    (const char* uri)
{
    GFile* gf = g_file_new_for_uri(uri);
    FmFolder* folder = fm_folder_get_for_gfile(gf);
    g_object_unref(gf);
    return folder;
}

void fm_folder_reload(FmFolder* folder)
{
    /* FIXME: remove all items and re-run a dir list job. */
    GSList* files_to_del = NULL;
    GList* l = fm_list_peek_head_link(folder->files);
    if(l)
    {
        for(;l;l=l->next)
        {
            FmFileInfo* fi = (FmFileInfo*)l->data;
            files_to_del = g_slist_prepend(files_to_del, fi);
        }
        g_signal_emit(folder, signals[FILES_REMOVED], 0, files_to_del);
        fm_list_clear(folder->files); /* fm_file_info_unref will be invoked. */
        g_slist_free(files_to_del);

        g_signal_emit(folder, signals[CONTENT_CHANGED], 0);
    }

    folder->job = fm_dir_list_job_new(folder->dir_path, FALSE);
    g_signal_connect(folder->job, "finished", G_CALLBACK(on_job_finished), folder);
    g_signal_connect(folder->job, "error", G_CALLBACK(on_job_err), folder);
    fm_job_run_async(FM_JOB(folder->job));
}

FmFileInfoList* fm_folder_get_files (FmFolder* folder)
{
    return folder->files;
}

GList* _fm_folder_get_file_by_name(FmFolder* folder, const char* name)
{
    GList* l = fm_list_peek_head_link(folder->files);
    for(;l;l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        if(strcmp(fi->path->name, name) == 0)
            return l;
    }
    return NULL;
}

FmFileInfo* fm_folder_get_file_by_name(FmFolder* folder, const char* name)
{
    GList* l = _fm_folder_get_file_by_name(folder, name);
    return l ? (FmFileInfo*)l->data : NULL;
}

FmFolder* fm_folder_get(FmPath* path)
{
    return fm_folder_get_internal(path, NULL);
}

gboolean fm_folder_get_is_loaded(FmFolder* folder)
{
    return (folder->job == NULL);
}

gboolean fm_folder_get_filesystem_info(FmFolder* folder, guint64* total_size, guint64* free_size)
{
    if(folder->has_fs_info)
    {
        *total_size = folder->fs_total_size;
        *free_size = folder->fs_free_size;
        return TRUE;
    }
    return FALSE;
}

static void on_query_filesystem_info_finished(GObject *src, GAsyncResult *res, FmFolder* folder)
{
    GFile* gf = G_FILE(src);
    GError* err = NULL;
    GFileInfo* inf = g_file_query_filesystem_info_finish(gf, res, &err);
    if(!inf)
    {
        folder->fs_total_size = folder->fs_free_size = 0;
        folder->has_fs_info = FALSE;
        folder->fs_info_not_avail = TRUE;

        /* FIXME: examine unsupported filesystems */

        g_error_free(err);
        goto _out;
    }
    if(g_file_info_has_attribute(inf, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE))
    {
        folder->fs_total_size = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
        folder->fs_free_size = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
        folder->has_fs_info = TRUE;
    }
    else
    {
        folder->fs_total_size = folder->fs_free_size = 0;
        folder->has_fs_info = FALSE;
        folder->fs_info_not_avail = TRUE;
    }
    g_object_unref(inf);

_out:
    if(folder->fs_size_cancellable)
    {
        g_object_unref(folder->fs_size_cancellable);
        folder->fs_size_cancellable = NULL;
    }

    g_signal_emit(folder, signals[FS_INFO], 0);
    g_object_unref(folder);
}

void fm_folder_query_filesystem_info(FmFolder* folder)
{
    if(!folder->fs_size_cancellable && !folder->fs_info_not_avail)
    {
        folder->fs_size_cancellable = g_cancellable_new();
        g_file_query_filesystem_info_async(folder->gf,
                G_FILE_ATTRIBUTE_FILESYSTEM_SIZE","
                G_FILE_ATTRIBUTE_FILESYSTEM_FREE,
                G_PRIORITY_LOW, folder->fs_size_cancellable,
                on_query_filesystem_info_finished, g_object_ref(folder));
    }
}

static void fm_folder_content_changed(FmFolder* folder)
{
    if(folder->has_fs_info && !folder->fs_info_not_avail)
        fm_folder_query_filesystem_info(folder);
}
