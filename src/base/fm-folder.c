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
    START_LOADING,
    FINISH_LOADING,
    UNMOUNT,
    CHANGED,
    REMOVED,
    CONTENT_CHANGED,
    FS_INFO,
    ERROR,
    N_SIGNALS
};

struct _FmFolder
{
    GObject parent;

    FmPath* dir_path;
    GFile* gf;
    GFileMonitor* mon;
    FmDirListJob* dirlist_job;
    FmFileInfo* dir_fi;
    FmFileInfoList* files;

    /* for file monitor */
    guint idle_handler;
    GSList* files_to_add;
    GSList* files_to_update;
    GSList* files_to_del;
    GSList* pending_jobs;
    guint idle_reload_handler;

    /* filesystem info */
    guint64 fs_total_size;
    guint64 fs_free_size;
    GCancellable* fs_size_cancellable;
    gboolean has_fs_info : 1;
    gboolean fs_info_not_avail : 1;
};

static FmFolder* fm_folder_new_internal(FmPath* path, GFile* gf);
static FmFolder* fm_folder_get_internal(FmPath* path, GFile* gf);
static void fm_folder_dispose(GObject *object);
static void fm_folder_finalize(GObject *object);
static void fm_folder_content_changed(FmFolder* folder);

static void on_file_info_job_finished(FmFileInfoJob* job, FmFolder* folder);
static gboolean on_idle(FmFolder* folder);

G_DEFINE_TYPE(FmFolder, fm_folder, G_TYPE_OBJECT);

static GList* _fm_folder_get_file_by_name(FmFolder* folder, const char* name);

static guint signals[N_SIGNALS];
static GHashTable* hash = NULL; /* FIXME: should this be guarded with a mutex? */

static GVolumeMonitor* volume_monitor = NULL;

static void fm_folder_class_init(FmFolderClass *klass)
{
    GObjectClass *g_object_class;
    FmFolderClass* folder_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_folder_dispose;
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

    /* emitted when the folder is about to be reloaded */
    signals[START_LOADING] =
        g_signal_new ( "start-loading",
                       G_TYPE_FROM_CLASS (klass),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET( FmFolderClass, start_loading),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /* emitted when the content of the folder is loaded.
     * this signal can be emitted for more than once and can be induced
     * by calling fm_folder_reload(). */
    signals[ FINISH_LOADING ] =
        g_signal_new ( "finish-loading",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, finish_loading ),
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

static gboolean on_idle_reload(FmFolder* folder)
{
    fm_folder_reload(folder);
    folder->idle_reload_handler = 0;
    return FALSE;
}

static void queue_reload(FmFolder* folder)
{
    if(folder->idle_reload_handler)
        g_source_remove(folder->idle_reload_handler);
    folder->idle_reload_handler = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_idle_reload, folder, NULL);
}

void on_file_info_job_finished(FmFileInfoJob* job, FmFolder* folder)
{
    GList* l;
    GSList* files_to_add = NULL;
    GSList* files_to_update = NULL;
    gboolean content_changed = FALSE;
    if(!fm_job_is_cancelled(FM_JOB(job)))
    {
        gboolean need_added = g_signal_has_handler_pending(folder, signals[FILES_ADDED], 0, TRUE);
        gboolean need_changed = g_signal_has_handler_pending(folder, signals[FILES_CHANGED], 0, TRUE);

        for(l=fm_list_peek_head_link(job->file_infos);l;l=l->next)
        {
            FmFileInfo* fi = (FmFileInfo*)l->data;
            FmPath* path = fm_file_info_get_path(fi);
            GList* l2 = _fm_folder_get_file_by_name(folder, path->name);
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
                if(need_changed)
                    files_to_update = g_slist_prepend(files_to_update, fi2);
            }
            else
            {
                if(need_added)
                    files_to_add = g_slist_prepend(files_to_add, fi);
                fm_file_info_ref(fi);
                fm_list_push_tail(folder->files, fi);
            }
        }
        if(files_to_add)
        {
            g_signal_emit(folder, signals[FILES_ADDED], 0, files_to_add);
            g_slist_free(files_to_add);
        }
        if(files_to_update)
        {
            g_signal_emit(folder, signals[FILES_CHANGED], 0, files_to_update);
            g_slist_free(files_to_update);
        }
        g_signal_emit(folder, signals[CONTENT_CHANGED], 0);
    }
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
        for(l=folder->files_to_update; l; l = l->next)
        {
            char* name = (char*)l->data;
            path = fm_path_new_child(folder->dir_path, name);
            fm_file_info_job_add(job, path);
            fm_path_unref(path);
            g_free(name);
        }
        g_slist_free(folder->files_to_update);
        folder->files_to_update = NULL;
    }

    if(folder->files_to_add)
    {
        for(l=folder->files_to_add;l;l=l->next)
        {
            char* name = (char*)l->data;
            path = fm_path_new_child(folder->dir_path, name);
            fm_file_info_job_add(job, path);
            fm_path_unref(path);
            g_free(name);
        }
        g_slist_free(folder->files_to_add);
        folder->files_to_add = NULL;
    }

    if(job)
    {
        g_signal_connect(job, "finished", G_CALLBACK(on_file_info_job_finished), folder);
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

    /* const char* names[]={
        "G_FILE_MONITOR_EVENT_CHANGED",
        "G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT",
        "G_FILE_MONITOR_EVENT_DELETED",
        "G_FILE_MONITOR_EVENT_CREATED",
        "G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED",
        "G_FILE_MONITOR_EVENT_PRE_UNMOUNT",
        "G_FILE_MONITOR_EVENT_UNMOUNTED"
    }; */
    name = g_file_get_basename(gf);
    /* g_debug("folder: %p, file %s event: %s", folder, name, names[evt]); */
    g_free(name);

    if(g_file_equal(gf, folder->gf))
    {
        GError* err = NULL;
        /* g_debug("event of the folder itself: %d", evt); */

        /* NOTE: g_object_ref() should be used here.
         * Sometimes the folder will be freed by signal handlers 
         * during emission of the change notifications. */
        g_object_ref(folder);
        switch(evt)
        {
        case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
            /* g_debug("folder is going to be unmounted"); */
            break;
        case G_FILE_MONITOR_EVENT_UNMOUNTED:
            g_signal_emit(folder, signals[UNMOUNT], 0);
            /* g_debug("folder is unmounted"); */
            queue_reload(folder);
            break;
        case G_FILE_MONITOR_EVENT_DELETED:
            g_signal_emit(folder, signals[REMOVED], 0);
            /* g_debug("folder is deleted"); */
            break;
        case G_FILE_MONITOR_EVENT_CREATED:
            queue_reload(folder);
            break;
        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
            g_signal_emit(folder, signals[CHANGED], 0);
            /* update volume info */
            fm_folder_query_filesystem_info(folder);
            /* g_debug("folder is changed"); */
            break;
        }
        g_object_unref(folder);
        return;
    }

    name = g_file_get_basename(gf);

    /* NOTE: sometimes, for unknown reasons, GFileMonitor gives us the
     * same event of the same file for multiple times. So we need to 
     * check for duplications ourselves here. */
    switch(evt)
    {
    case G_FILE_MONITOR_EVENT_CREATED:
    {
        /* make sure that the file is not already queued for addition. */
        if(!g_slist_find_custom(folder->files_to_add, name, (GCompareFunc)strcmp))
        {
            folder->files_to_add = g_slist_append(folder->files_to_add, name);
            /* if we already have the file in FmFolder, update the existing one instead. */
            if(_fm_folder_get_file_by_name(folder, name)) /* we already have it! */
            {
                /* update the existing item. */
                folder->files_to_update = g_slist_append(folder->files_to_update, name);
            }
        }
        else
            g_free(name);
        break;
    }
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    {
        /* make sure that the file is not already queued for changes or
         * it's already queued for addition. */
        if(!g_slist_find_custom(folder->files_to_update, name, (GCompareFunc)strcmp) &&
            !g_slist_find_custom(folder->files_to_add, name, (GCompareFunc)strcmp))
        {
            folder->files_to_update = g_slist_append(folder->files_to_update, name);
        }
        else
            g_free(name);
        break;
    }
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
        folder->idle_handler = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_idle, folder, NULL);
}

static void on_dirlist_job_finished(FmDirListJob* job, FmFolder* folder)
{
    GSList* files = NULL;
    /* actually manually disconnecting from 'finished' signal is not
     * needed since the signal is only emit once, and later the job
     * object will be distroyed very soon. */
    /* g_signal_handlers_disconnect_by_func(job, on_dirlist_job_finished, folder); */

    if(!fm_job_is_cancelled(FM_JOB(job)))
    {
        GList* l;
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

        /* Some new files are created while FmDirListJob is loading the folder. */
        if(G_UNLIKELY(folder->files_to_add))
        {
			/* This should be a very rare case. Could this happen? */
            GSList* l;
            g_assert(folder->files_to_add == NULL);
            for(l = folder->files_to_add; l;)
            {
                char* name = (char*)l->data;
                GSList* next = l->next;
                if(_fm_folder_get_file_by_name(folder, name))
                {
					/* we already have the file. remove it from files_to_add, 
					 * and put it in files_to_update instead. .*/
                    folder->files_to_update = g_slist_prepend(folder->files_to_update, name);
                    folder->files_to_add = g_slist_delete_link(folder->files_to_add, l);
                }
                l = next;
            }
        }
    }
    folder->dirlist_job = NULL; /* the job object will be freed in idle handler. */

    g_object_ref(folder);
    g_signal_emit(folder, signals[FINISH_LOADING], 0);
    g_object_unref(folder);
}

static FmJobErrorAction on_dirlist_job_error(FmDirListJob* job, GError* err, FmJobErrorSeverity severity, FmFolder* folder)
{
    FmJobErrorAction ret;
    /* it's possible that some signal handlers tries to free the folder
     * when errors occurs, so let's g_object_ref here. */
    g_object_ref(folder);
    g_signal_emit(folder, signals[ERROR], 0, err, severity, &ret);
    g_object_unref(folder);
    return ret;
}

FmFolder* fm_folder_new_internal(FmPath* path, GFile* gf)
{
    FmFolder* folder = (FmFolder*)g_object_new(FM_TYPE_FOLDER, NULL);
    folder->dir_path = fm_path_ref(path);
    folder->gf = (GFile*)g_object_ref(gf);
    fm_folder_reload(folder);
    return folder;
}

FmFolder* fm_folder_get_internal(FmPath* path, GFile* gf)
{
    FmFolder* folder;
    /* FIXME: should we provide a generic FmPath cache in fm-path.c
     * to associate all kinds of data structures with FmPaths? */

    /* FIXME: should creation of the hash table be moved to fm_init()? */
    folder = (FmFolder*)g_hash_table_lookup(hash, path);

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

static void free_dirlist_job(FmFolder* folder)
{
    g_signal_handlers_disconnect_by_func(folder->dirlist_job, on_dirlist_job_finished, folder);
    g_signal_handlers_disconnect_by_func(folder->dirlist_job, on_dirlist_job_error, folder);
    fm_job_cancel(FM_JOB(folder->dirlist_job)); /* FIXME: is this ok? */
    /* the job will be freed automatically in idle handler. */
    folder->dirlist_job = NULL;
}

static void fm_folder_dispose(GObject *object)
{
    FmFolder *folder;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FOLDER(object));

    g_debug("fm_folder_dispose");
    
    folder = FM_FOLDER(object);

    if(folder->dirlist_job)
        free_dirlist_job(folder);

    if(folder->pending_jobs)
    {
        GSList* l;
        for(l = folder->pending_jobs;l;l=l->next)
        {
            FmJob* job = FM_JOB(l->data);
            g_signal_handlers_disconnect_by_func(job, on_file_info_job_finished, folder);
            fm_job_cancel(job);
            /* the job will be freed automatically in idle handler. */
        }
        g_slist_free(folder->pending_jobs);
        folder->pending_jobs = NULL;
    }

    if(folder->mon)
    {
        g_signal_handlers_disconnect_by_func(folder->mon, on_folder_changed, folder);
        g_object_unref(folder->mon);
        folder->mon = NULL;
    }

    if(folder->idle_reload_handler)
    {
        g_source_remove(folder->idle_reload_handler);
        folder->idle_reload_handler = 0;
    }

    if(folder->idle_handler)
    {
        g_source_remove(folder->idle_handler);
        folder->idle_handler = 0;
        if(folder->files_to_add)
        {
            g_slist_foreach(folder->files_to_add, (GFunc)g_free, NULL);
            g_slist_free(folder->files_to_add);
            folder->files_to_add = NULL;
        }
        if(folder->files_to_update)
        {
            g_slist_foreach(folder->files_to_update, (GFunc)g_free, NULL);
            g_slist_free(folder->files_to_update);
            folder->files_to_update = NULL;
        }
        if(folder->files_to_del)
        {
            // FIXME: is this needed?
            /* g_slist_foreach(folder->files_to_del, (GFunc)g_free, NULL); */
            g_slist_free(folder->files_to_del);
            folder->files_to_del = NULL;
        }
    }

    if(folder->fs_size_cancellable)
    {
        g_cancellable_cancel(folder->fs_size_cancellable);
        g_object_unref(folder->fs_size_cancellable);
        folder->fs_size_cancellable = NULL;
    }

    /* remove from hash table */
    if(folder->dir_path)
    {
        g_hash_table_remove(hash, folder->dir_path);
        fm_path_unref(folder->dir_path);
        folder->dir_path = NULL;
    }

    if(folder->dir_fi)
    {
        fm_file_info_unref(folder->dir_fi);
        folder->dir_fi = NULL;
    }

    if(folder->gf)
    {
        g_object_unref(folder->gf);
        folder->gf = NULL;
    }

    if(folder->files)
    {
        fm_list_unref(folder->files);
        folder->files = NULL;
    }

    (* G_OBJECT_CLASS(fm_folder_parent_class)->dispose)(object);
}

static void fm_folder_finalize(GObject *object)
{
    FmFolder *folder;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FOLDER(object));

    folder = FM_FOLDER(object);
    g_debug("free folder %p", folder);

    if (G_OBJECT_CLASS(fm_folder_parent_class)->finalize)
        (* G_OBJECT_CLASS(fm_folder_parent_class)->finalize)(object);
}

FmFolder* fm_folder_get_for_gfile(GFile* gf)
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
    GError* err = NULL;

    /* Tell the world that we're about to reload the folder.
     * It might be a good idea for users of the folder to disconnect
     * from the folder temporarily and reconnect to it again after
     * the folder complete the loading. This might reduce some
     * unnecessary signal handling and UI updates. */
    g_signal_emit(folder, signals[START_LOADING], 0);

    if(folder->dir_fi)
    {
        /* we need to reload folde info. */
        fm_file_info_unref(folder->dir_fi);
        folder->dir_fi = NULL;
    }

    /* remove all items and re-run a dir list job. */
    GSList* files_to_del = NULL;
    GList* l = fm_list_peek_head_link(folder->files);

    /* cancel running dir listing job if there is any. */
    if(folder->dirlist_job)
        free_dirlist_job(folder);

    /* remove all existing files */
    if(l)
    {
        gboolean need_removed = g_signal_has_handler_pending(folder, signals[FILES_REMOVED], 0, TRUE);
        for(;l;l=l->next)
        {
            FmFileInfo* fi = (FmFileInfo*)l->data;
            if(need_removed)
                files_to_del = g_slist_prepend(files_to_del, fi);
        }
        if(need_removed)
        {
            g_signal_emit(folder, signals[FILES_REMOVED], 0, files_to_del);
            g_slist_free(files_to_del);
        }
        fm_list_clear(folder->files); /* fm_file_info_unref will be invoked. */
    }

    /* also re-create a new file monitor */
    if(folder->mon)
    {
        g_signal_handlers_disconnect_by_func(folder->mon, on_folder_changed, folder);
        g_object_unref(folder->mon);
    }
    folder->mon = fm_monitor_directory(folder->gf, &err);
    if(folder->mon)
    {
        g_signal_connect(folder->mon, "changed", G_CALLBACK(on_folder_changed), folder);
    }
    else
    {
        g_debug("file monitor cannot be created: %s", err->message);
        g_error_free(err);
        folder->mon = NULL;
    }

    g_signal_emit(folder, signals[CONTENT_CHANGED], 0);

    /* run a new dir listing job */
    folder->dirlist_job = fm_dir_list_job_new(folder->dir_path, FALSE);
    g_signal_connect(folder->dirlist_job, "finished", G_CALLBACK(on_dirlist_job_finished), folder);
    g_signal_connect(folder->dirlist_job, "error", G_CALLBACK(on_dirlist_job_error), folder);
    fm_job_run_async(FM_JOB(folder->dirlist_job));

    /* also reload filesystem info.
     * FIXME: is this needed? */
    fm_folder_query_filesystem_info(folder);
}

FmFileInfoList* fm_folder_get_files (FmFolder* folder)
{
    return folder->files;
}

gboolean fm_folder_is_empty(FmFolder* folder)
{
    return fm_list_is_empty(folder->files);
}

FmFileInfo* fm_folder_get_info(FmFolder* folder)
{
    return folder->dir_fi;
}

FmPath* fm_folder_get_path(FmFolder* folder)
{
    return folder->dir_path;
}

GList* _fm_folder_get_file_by_name(FmFolder* folder, const char* name)
{
    GList* l = fm_list_peek_head_link(folder->files);
    for(;l;l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        FmPath* path = fm_file_info_get_path(fi);
        if(strcmp(path->name, name) == 0)
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

/* is the folder content fully loaded */
gboolean fm_folder_is_loaded(FmFolder* folder)
{
    return (folder->dirlist_job == NULL);
}

/* is this a valid folder */
gboolean fm_folder_is_valid(FmFolder* folder)
{
    return (folder->dir_fi != NULL);
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
                (GAsyncReadyCallback)on_query_filesystem_info_finished,
                g_object_ref(folder));
    }
}

static void fm_folder_content_changed(FmFolder* folder)
{
    if(folder->has_fs_info && !folder->fs_info_not_avail)
        fm_folder_query_filesystem_info(folder);
}

/* NOTE:
 * GFileMonitor has some significant limitations:
 * 1. Currently it can correctly emit unmounted event for a directory.
 * 2. After a directory is unmounted, its content changes.
 *    Inotify does not fire events for this so a forced reload is needed.
 * 3. If a folder is empty, and later a filesystem is mounted to the
 *    folder, its content should reflect the content of the newly mounted
 *    filesystem. However, GFileMonitor and inotify do not emit events
 *    for this case. A forced reload might be needed for this case as well.
 * 4. Some limitations come from Linux/inotify. If FAM/gamin is used,
 *    the condition may be different. More testing is needed.
 */
static void on_mount_added(GVolumeMonitor* vm, GMount* mount, gpointer user_data)
{
    /* If a filesystem is mounted over an existing folder,
     * we need to refresh the content of the folder to reflect
     * the changes. Besides, we need to create a new GFileMonitor
     * for the newly-mounted filesystem as the inode already changed.
     * GFileMonitor cannot detect this kind of changes caused by mounting.
     * So let's do it ourselves. */

    GFile* gfile = g_mount_get_root(mount);
    /* g_debug("FmFolder::mount_added"); */
    if(gfile)
    {
        GHashTableIter it;
        FmPath* path;
        FmFolder* folder;
        FmPath* mounted_path = fm_path_new_for_gfile(gfile);
        g_object_unref(gfile);

        g_hash_table_iter_init(&it, hash);
        while(g_hash_table_iter_next(&it, (gpointer*)&path, (gpointer*)&folder))
        {
            if(fm_path_equal(path, mounted_path))
                queue_reload(folder);
            else if(fm_path_has_prefix(path, mounted_path))
            {
                /* see if currently cached folders are below the mounted path.
                 * Folders below the mounted folder are removed.
                 * FIXME: should we emit "removed" signal for them, or 
                 * keep the folders and only reload them? */
                /* g_signal_emit(folder, signals[REMOVED], 0); */
                queue_reload(folder);
            }
        }
        fm_path_unref(mounted_path);
    }
}

static void on_mount_removed(GVolumeMonitor* vm, GMount* mount, gpointer user_data)
{
    /* g_debug("FmFolder::mount_removed"); */
}

void _fm_folder_init()
{
    hash = g_hash_table_new((GHashFunc)fm_path_hash, (GEqualFunc)fm_path_equal);
    volume_monitor = g_volume_monitor_get();
    if(G_LIKELY(volume_monitor))
    {
        g_signal_connect(volume_monitor, "mount-added", G_CALLBACK(on_mount_added), NULL);
        g_signal_connect(volume_monitor, "mount-removed", G_CALLBACK(on_mount_removed), NULL);
    }
}

void _fm_folder_finalize()
{
    g_hash_table_destroy(hash);
    hash = NULL;
    if(volume_monitor)
    {
        g_signal_handlers_disconnect_by_func(volume_monitor, on_mount_added, NULL);
        g_signal_handlers_disconnect_by_func(volume_monitor, on_mount_removed, NULL);
        g_object_unref(volume_monitor);
        volume_monitor = NULL;
    }
}
