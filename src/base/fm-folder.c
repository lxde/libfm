/*
 *      fm-folder.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
 * SECTION:fm-folder
 * @short_description: Folder loading and monitoring.
 * @title: FmFolder
 *
 * @include: libfm/fm-folder.h
 *
 * The #FmFolder object allows to open and monitor items of some directory
 * (either local or remote), i.e. files and directories, to have fast access
 * to their info and to info of the directory itself as well.
 */

#include "fm-folder.h"
#include "fm-monitor.h"
#include "fm-marshal.h"
#include <string.h>
#include "fm-dummy-monitor.h"

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

    /*<private>*/
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
    gboolean pending_change_notify;
    gboolean filesystem_info_pending;
    guint idle_reload_handler;

    /* filesystem info - set in query thread, read in main */
    guint64 fs_total_size;
    guint64 fs_free_size;
    GCancellable* fs_size_cancellable;
    gboolean has_fs_info : 1;
    gboolean fs_info_not_avail : 1;
};

static FmFolder* fm_folder_new_internal(FmPath* path, GFile* gf);
static FmFolder* fm_folder_get_internal(FmPath* path, GFile* gf);
static void fm_folder_dispose(GObject *object);
static void fm_folder_content_changed(FmFolder* folder);

static void on_file_info_job_finished(FmFileInfoJob* job, FmFolder* folder);
static gboolean on_idle(FmFolder* folder);

G_DEFINE_TYPE(FmFolder, fm_folder, G_TYPE_OBJECT);

static GList* _fm_folder_get_file_by_name(FmFolder* folder, const char* name);

static guint signals[N_SIGNALS];
static GHashTable* hash = NULL; /* FIXME: should this be guarded with a mutex? */

static GVolumeMonitor* volume_monitor = NULL;

/* used for on_query_filesystem_info_finished() to lock folder */
G_LOCK_DEFINE_STATIC(query);

static void fm_folder_class_init(FmFolderClass *klass)
{
    GObjectClass *g_object_class;
    FmFolderClass* folder_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_folder_dispose;
    fm_folder_parent_class = (GObjectClass*)g_type_class_peek(G_TYPE_OBJECT);

    folder_class = FM_FOLDER_CLASS(klass);
    folder_class->content_changed = fm_folder_content_changed;

    /**
     * FmFolder::files-added:
     * @folder: the monitored directory
     * @list: #GList of newly added #FmFileInfo
     *
     * The #FmFolder::files-added signal is emitted when there is some
     * new file created in the directory.
     *
     * Since: 0.1.0
     */
    signals[ FILES_ADDED ] =
        g_signal_new ( "files-added",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, files_added ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    /**
     * FmFolder::files-removed:
     * @folder: the monitored directory
     * @list: #GList of #FmFileInfo that were deleted
     *
     * The #FmFolder::files-removed signal is emitted when some file was
     * deleted from the directory.
     *
     * Since: 0.1.0
     */
    signals[ FILES_REMOVED ] =
        g_signal_new ( "files-removed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, files_removed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    /**
     * FmFolder::files-changed:
     * @folder: the monitored directory
     * @list: #GList of #FmFileInfo that were changed
     *
     * The #FmFolder::files-changed signal is emitted when some file in
     * the directory was changed.
     *
     * Since: 0.1.0
     */
    signals[ FILES_CHANGED ] =
        g_signal_new ( "files-changed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, files_changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    /**
     * FmFolder::start-loading:
     * @folder: the monitored directory
     *
     * The #FmFolder::start-loading signal is emitted when the folder is
     * about to be reloaded.
     *
     * Since: 1.0.0
     */
    signals[START_LOADING] =
        g_signal_new ( "start-loading",
                       G_TYPE_FROM_CLASS (klass),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET( FmFolderClass, start_loading),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * FmFolder::finish-loading:
     * @folder: the monitored directory
     *
     * The #FmFolder::finish-loading signal is emitted when the content
     * of the folder is loaded.
     * This signal may be emitted more than once and can be induced
     * by calling fm_folder_reload().
     *
     * Since: 1.0.0
     */
    signals[ FINISH_LOADING ] =
        g_signal_new ( "finish-loading",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, finish_loading ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * FmFolder::unmount:
     * @folder: the monitored directory
     *
     * The #FmFolder::unmount signal is emitted when the folder was unmounted.
     *
     * Since: 0.1.1
     */
    signals[ UNMOUNT ] =
        g_signal_new ( "unmount",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, unmount ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * FmFolder::changed:
     * @folder: the monitored directory
     *
     * The #FmFolder::changed signal is emitted when the folder itself
     * was changed.
     *
     * Since: 0.1.16
     */
    signals[ CHANGED ] =
        g_signal_new ( "changed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * FmFolder::removed:
     * @folder: the monitored directory
     *
     * The #FmFolder::removed signal is emitted when the folder itself
     * was deleted.
     *
     * Since: 0.1.16
     */
    signals[ REMOVED ] =
        g_signal_new ( "removed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, removed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * FmFolder::content-changed:
     * @folder: the monitored directory
     *
     * The #FmFolder::content-changed signal is emitted when content
     * of the folder is changed (some files are added, removed, or changed).
     *
     * Since: 0.1.16
     */
    signals[ CONTENT_CHANGED ] =
        g_signal_new ( "content-changed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, content_changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * FmFolder::fs-info:
     * @folder: the monitored directory
     *
     * The #FmFolder::fs-info signal is emitted when filesystem
     * information is available.
     *
     * Since: 0.1.16
     */
    signals[ FS_INFO ] =
        g_signal_new ( "fs-info",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderClass, fs_info ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * FmFolder::error:
     * @folder: the monitored directory
     * @error: error descriptor
     * @severity: #FmJobErrorSeverity of the error
     *
     * The #FmFolder::error signal is emitted when some error happens.
     *
     * Return value: #FmJobErrorAction that should be performed on that error.
     *
     * Since: 0.1.1
     */
    signals[ERROR] =
        g_signal_new( "error",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET ( FmFolderClass, error ),
                      NULL, NULL,
                      fm_marshal_UINT__BOXED_UINT,
#if GLIB_CHECK_VERSION(2,26,0)
                      G_TYPE_UINT, 2, G_TYPE_ERROR, G_TYPE_UINT );
#else
                      G_TYPE_UINT, 2, G_TYPE_BOXED, G_TYPE_UINT );
#endif
}


static void fm_folder_init(FmFolder *folder)
{
    folder->files = fm_file_info_list_new();
}

static gboolean on_idle_reload(FmFolder* folder)
{
    fm_folder_reload(folder);
    folder->idle_reload_handler = 0;
    g_object_unref(folder);
    return FALSE;
}

static void queue_reload(FmFolder* folder)
{
    if(folder->idle_reload_handler)
        g_source_remove(folder->idle_reload_handler);
    folder->idle_reload_handler = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_idle_reload, g_object_ref(folder), NULL);
}

static void on_file_info_job_finished(FmFileInfoJob* job, FmFolder* folder)
{
    GList* l;
    GSList* files_to_add = NULL;
    GSList* files_to_update = NULL;
    if(!fm_job_is_cancelled(FM_JOB(job)))
    {
        gboolean need_added = g_signal_has_handler_pending(folder, signals[FILES_ADDED], 0, TRUE);
        gboolean need_changed = g_signal_has_handler_pending(folder, signals[FILES_CHANGED], 0, TRUE);

        for(l=fm_file_info_list_peek_head_link(job->file_infos);l;l=l->next)
        {
            FmFileInfo* fi = (FmFileInfo*)l->data;
            FmPath* path = fm_file_info_get_path(fi);
            GList* l2 = _fm_folder_get_file_by_name(folder, path->name);
            if(l2) /* the file is already in the folder, update */
            {
                FmFileInfo* fi2 = (FmFileInfo*)l2->data;
                /* FIXME: will fm_file_info_update here cause problems?
                 *        the file info might be referenced by others, too.
                 *        we're mofifying an object referenced by others.
                 *        we should redesign the API, or document this clearly
                 *        in future API doc.
                 */
                fm_file_info_update(fi2, fi);
                if(need_changed)
                    files_to_update = g_slist_prepend(files_to_update, fi2);
            }
            else
            {
                if(need_added)
                    files_to_add = g_slist_prepend(files_to_add, fi);
                fm_file_info_ref(fi);
                fm_file_info_list_push_tail(folder->files, fi);
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
    g_object_unref(job);
}

static gboolean on_idle(FmFolder* folder)
{
    GSList* l;
    FmFileInfoJob* job = NULL;
    FmPath* path;
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
        /* FIXME: free job if error */
        /* the job will be freed automatically in on_file_info_job_finished() */
    }

    if(folder->files_to_del)
    {
        GSList* ll;
        for(ll=folder->files_to_del;ll;ll=ll->next)
        {
            GList* l= (GList*)ll->data;
            ll->data = l->data;
            fm_file_info_list_delete_link_nounref(folder->files , l);
        }
        g_signal_emit(folder, signals[FILES_REMOVED], 0, folder->files_to_del);
        g_slist_foreach(folder->files_to_del, (GFunc)fm_file_info_unref, NULL);
        g_slist_free(folder->files_to_del);
        folder->files_to_del = NULL;

        g_signal_emit(folder, signals[CONTENT_CHANGED], 0);
    }

    if(folder->pending_change_notify)
    {
        g_signal_emit(folder, signals[CHANGED], 0);
        /* update volume info */
        fm_folder_query_filesystem_info(folder);
        folder->pending_change_notify = FALSE;
    }

    G_LOCK(query);
    folder->idle_handler = 0;
    if(folder->filesystem_info_pending)
    {
        g_signal_emit(folder, signals[FS_INFO], 0);
        folder->filesystem_info_pending = FALSE;
    }
    G_UNLOCK(query);
    g_object_unref(folder); /* it was borrowed by query */

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
    
    /*
    name = g_file_get_basename(gf);
    g_debug("folder: %p, file %s event: %s", folder, name, names[evt]);
    g_free(name);
    */

    if(g_file_equal(gf, folder->gf))
    {
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
            folder->pending_change_notify = TRUE;
            G_LOCK(query);
            if(!folder->idle_handler)
                folder->idle_handler = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_idle, g_object_ref(folder), NULL);
            G_UNLOCK(query);
            /* g_debug("folder is changed"); */
            break;
        case G_FILE_MONITOR_EVENT_MOVED:
        case G_FILE_MONITOR_EVENT_CHANGED:
            ;
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
            if(!_fm_folder_get_file_by_name(folder, name)) /* it's new file */
            {
                /* add the file name to queue for addition. */
                folder->files_to_add = g_slist_append(folder->files_to_add, name);
            }
            else if(!g_slist_find_custom(folder->files_to_update, name, (GCompareFunc)strcmp))
            {
                /* file already queued for update, don't duplicate */
                g_free(name);
            }
            /* if we already have the file in FmFolder, update the existing one instead. */
            else
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
    G_LOCK(query);
    if(!folder->idle_handler)
        folder->idle_handler = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_idle, g_object_ref(folder), NULL);
    G_UNLOCK(query);
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
        for(l = fm_file_info_list_peek_head_link(job->files); l; l=l->next)
        {
            FmFileInfo* inf = (FmFileInfo*)l->data;
            files = g_slist_prepend(files, inf);
            fm_file_info_list_push_tail(folder->files, inf);
        }
        if(G_LIKELY(files))
        {
            g_signal_emit(folder, signals[FILES_ADDED], 0, files);
            g_slist_free(files);
        }

        if(job->dir_fi)
            folder->dir_fi = fm_file_info_ref(job->dir_fi);

        /* Some new files are created while FmDirListJob is loading the folder. */
        if(G_UNLIKELY(folder->files_to_add))
        {
            /* This should be a very rare case. Could this happen? */
            GSList* l;
            for(l = folder->files_to_add; l;)
            {
                char* name = (char*)l->data;
                GSList* next = l->next;
                if(_fm_folder_get_file_by_name(folder, name))
                {
                    /* we already have the file. remove it from files_to_add, 
                     * and put it in files_to_update instead.
                     * No strdup for name is needed here. We steal
                     * the string from files_to_add.*/
                    folder->files_to_update = g_slist_prepend(folder->files_to_update, name);
                    folder->files_to_add = g_slist_delete_link(folder->files_to_add, l);
                }
                l = next;
            }
        }
    }
    g_object_unref(folder->dirlist_job);
    folder->dirlist_job = NULL;

    g_object_ref(folder);
    g_signal_emit(folder, signals[FINISH_LOADING], 0);
    g_object_unref(folder);
}

static FmJobErrorAction on_dirlist_job_error(FmDirListJob* job, GError* err, FmJobErrorSeverity severity, FmFolder* folder)
{
    guint ret;
    /* it's possible that some signal handlers tries to free the folder
     * when errors occurs, so let's g_object_ref here. */
    g_object_ref(folder);
    g_signal_emit(folder, signals[ERROR], 0, err, (guint)severity, &ret);
    g_object_unref(folder);
    return ret;
}

static FmFolder* fm_folder_new_internal(FmPath* path, GFile* gf)
{
    FmFolder* folder = (FmFolder*)g_object_new(FM_TYPE_FOLDER, NULL);
    folder->dir_path = fm_path_ref(path);
    folder->gf = (GFile*)g_object_ref(gf);
    fm_folder_reload(folder);
    return folder;
}

/* NB: increases reference on returned object */
static FmFolder* fm_folder_get_internal(FmPath* path, GFile* gf)
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
    g_object_unref(folder->dirlist_job);
    folder->dirlist_job = NULL;
}

static void fm_folder_dispose(GObject *object)
{
    FmFolder *folder;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FOLDER(object));

    /* g_debug("fm_folder_dispose"); */

    folder = (FmFolder*)object;

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
            g_object_unref(job);
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

    G_LOCK(query);
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
    G_UNLOCK(query);

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
        fm_file_info_list_unref(folder->files);
        folder->files = NULL;
    }

    (* G_OBJECT_CLASS(fm_folder_parent_class)->dispose)(object);
}

/**
 * fm_folder_from_gfile
 * @gf: #GFile file descriptor
 *
 * Retrieves a folder corresponding to @gf. Returned data may be freshly
 * created or already loaded. Caller should call g_object_unref() on the
 * returned data after usage.
 *
 * Before 1.0.0 this call had name fm_folder_get_for_gfile.
 *
 * Returns: (transfer full): #FmFolder corresponding to @gf.
 *
 * Since: 0.1.1
 */
FmFolder* fm_folder_from_gfile(GFile* gf)
{
    FmPath* path = fm_path_new_for_gfile(gf);
    FmFolder* folder = fm_folder_get_internal(path, gf);
    fm_path_unref(path);
    return folder;
}

/**
 * fm_folder_from_path_name
 * @path: POSIX path to the folder
 *
 * Retrieves a folder corresponding to @path. Returned data may be freshly
 * created or already loaded. Caller should call g_object_unref() on the
 * returned data after usage.
 *
 * Before 1.0.0 this call had name fm_folder_get_for_path_name.
 *
 * Returns: (transfer full): #FmFolder corresponding to @path.
 *
 * Since: 0.1.0
 */
FmFolder* fm_folder_from_path_name(const char* path)
{
    FmPath* fm_path = fm_path_new_for_str(path);
    FmFolder* folder = fm_folder_get_internal(fm_path, NULL);
    fm_path_unref(fm_path);
    return folder;
}

/**
 * fm_folder_from_uri
 * @uri: URI for the folder
 *
 * Retrieves a folder corresponding to @uri. Returned data may be freshly
 * created or already loaded. Caller should call g_object_unref() on the
 * returned data after usage.
 *
 * Before 1.0.0 this call had name fm_folder_get_for_uri.
 *
 * Returns: (transfer full): #FmFolder corresponding to @uri.
 *
 * Since: 0.1.0
 */
/* FIXME: should we use GFile here? */
FmFolder*    fm_folder_from_uri    (const char* uri)
{
    GFile* gf = g_file_new_for_uri(uri);
    FmFolder* folder = fm_folder_from_gfile(gf);
    g_object_unref(gf);
    return folder;
}

/**
 * fm_folder_reload
 * @folder: folder to be reloaded
 *
 * Causes to retrieve all data for the @folder as if folder was freshly
 * opened.
 *
 * Since: 0.1.1
 */
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
    GList* l = fm_file_info_list_peek_head_link(folder->files);

    /* cancel running dir listing job if there is any. */
    if(folder->dirlist_job)
        free_dirlist_job(folder);

    /* remove all existing files */
    if(l)
    {
        if(g_signal_has_handler_pending(folder, signals[FILES_REMOVED], 0, TRUE))
        {
            /* need to emit signal of removal */
            GSList* files_to_del = NULL;
            for(;l;l=l->next)
                files_to_del = g_slist_prepend(files_to_del, (FmFileInfo*)l->data);
            g_signal_emit(folder, signals[FILES_REMOVED], 0, files_to_del);
            g_slist_free(files_to_del);
        }
        fm_file_info_list_clear(folder->files); /* fm_file_info_unref will be invoked. */
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
    /* FIXME: free job if error */

    /* also reload filesystem info.
     * FIXME: is this needed? */
    fm_folder_query_filesystem_info(folder);
}

/**
 * fm_folder_get_files
 * @folder: folder to retrieve file list
 *
 * Retrieves list of currently known files and subdirectories in the
 * @folder. Returned list is owned by #FmFolder and should be not modified
 * by caller. If caller wants to keep a reference to the returned list it
 * should do fm_file_info_list_ref() on the returned data.
 *
 * Before 1.0.0 this call had name fm_folder_get.
 *
 * Returns: (transfer none): list of items that @folder currently contains.
 *
 * Since: 0.1.1
 */
FmFileInfoList* fm_folder_get_files (FmFolder* folder)
{
    return folder->files;
}

/**
 * fm_folder_is_empty
 * @folder: folder to test
 *
 * Checks if folder has no files or subdirectories.
 *
 * Returns: %TRUE if folder is empty.
 *
 * Since: 1.0.0
 */
gboolean fm_folder_is_empty(FmFolder* folder)
{
    return fm_file_info_list_is_empty(folder->files);
}

/**
 * fm_folder_get_info
 * @folder: folder to retrieve info
 *
 * Retrieves #FmFileInfo data about the folder itself. Returned data is
 * owned by #FmFolder and should be not modified or freed by caller.
 *
 * Returns: (transfer none): info descriptor of the @folder.
 *
 * Since: 1.0.0
 */
FmFileInfo* fm_folder_get_info(FmFolder* folder)
{
    return folder->dir_fi;
}

/**
 * fm_folder_get_path
 * @folder: folder to retrieve path
 *
 * Retrieves path of the folder. Returned data is owned by #FmFolder and
 * should be not modified or freed by caller.
 *
 * Returns: (transfer none): path of the folder.
 *
 * Since: 1.0.0
 */
FmPath* fm_folder_get_path(FmFolder* folder)
{
    return folder->dir_path;
}

static GList* _fm_folder_get_file_by_name(FmFolder* folder, const char* name)
{
    GList* l = fm_file_info_list_peek_head_link(folder->files);
    for(;l;l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        FmPath* path = fm_file_info_get_path(fi);
        if(strcmp(path->name, name) == 0)
            return l;
    }
    return NULL;
}

/**
 * fm_folder_get_file_by_name
 * @folder: folder to search
 * @name: basename of file in @folder
 *
 * Tries to find a file with basename @name in the @folder. Returned data
 * is owned by #FmFolder and should be not freed by caller.
 *
 * Returns: (transfer none): info descriptor of file or %NULL if no file was found.
 *
 * Since: 0.1.16
 */
FmFileInfo* fm_folder_get_file_by_name(FmFolder* folder, const char* name)
{
    GList* l = _fm_folder_get_file_by_name(folder, name);
    return l ? (FmFileInfo*)l->data : NULL;
}

/**
 * fm_folder_from_path
 * @path: path descriptor for the folder
 *
 * Retrieves a folder corresponding to @path. Returned data may be freshly
 * created or already loaded. Caller should call g_object_unref() on the
 * returned data after usage.
 *
 * Before 1.0.0 this call had name fm_folder_get.
 *
 * Returns: (transfer full): #FmFolder corresponding to @path.
 *
 * Since: 0.1.1
 */
FmFolder* fm_folder_from_path(FmPath* path)
{
    return fm_folder_get_internal(path, NULL);
}

/**
 * fm_folder_is_loaded
 * @folder: folder to test
 *
 * Checks if all data for @folder is completely loaded.
 *
 * Before 1.0.0 this call had name fm_folder_get_is_loaded.
 *
 * Returns: %TRUE is loading of folder is already completed.
 *
 * Since: 0.1.16
 */
gboolean fm_folder_is_loaded(FmFolder* folder)
{
    return (folder->dirlist_job == NULL);
}

/**
 * fm_folder_is_valid
 * @folder: folder to test
 *
 * Checks if directory described by @folder exists.
 *
 * Returns: %TRUE if @folder describes a valid existing directory.
 *
 * Since: 1.0.0
 */
gboolean fm_folder_is_valid(FmFolder* folder)
{
    return (folder->dir_fi != NULL);
}

/**
 * fm_folder_get_filesystem_info
 * @folder: folder to retrieve info
 * @total_size: pointer to counter of total size of the filesystem
 * @free_size: pointer to counter of free space on the filesystem
 *
 * Retrieves info about total and free space on the filesystem which
 * contains the @folder.
 *
 * Returns: %TRUE if information can be retrieved.
 *
 * Since: 0.1.16
 */
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

/* this function is run in GIO thread! */
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
    G_LOCK(query);
    if(folder->fs_size_cancellable)
    {
        g_object_unref(folder->fs_size_cancellable);
        folder->fs_size_cancellable = NULL;
    }

    folder->filesystem_info_pending = TRUE;
    /* we have a reference borrowed by async query still */
    if(!folder->idle_handler)
        folder->idle_handler = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_idle, folder, NULL);
    else
        g_object_unref(folder);
    G_UNLOCK(query);
}

/**
 * fm_folder_query_filesystem_info
 * @folder: folder to retrieve info
 *
 * Queries to retrieve info about filesystem which contains the @folder if
 * the filesystem supports such query.
 *
 * Since: 0.1.16
 */
void fm_folder_query_filesystem_info(FmFolder* folder)
{
    G_LOCK(query);
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
    G_UNLOCK(query);
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

    /* NOTE: gvfs does not emit unmount signals for remote folders since
     * GFileMonitor does not support remote filesystems at all. We do fake
     * file monitoring with FmDummyMonitor dirty hack.
     * So here is the side effect, no unmount notifications.
     * We need to generate the signal ourselves. */

    GFile* gfile = g_mount_get_root(mount);
    if(gfile)
    {
        GSList* dummy_monitor_folders = NULL, *l;
        GHashTableIter it;
        FmPath* path;
        FmFolder* folder;
        FmPath* mounted_path = fm_path_new_for_gfile(gfile);
        g_object_unref(gfile);

        g_hash_table_iter_init(&it, hash);
        while(g_hash_table_iter_next(&it, (gpointer*)&path, (gpointer*)&folder))
        {
            if(fm_path_has_prefix(path, mounted_path))
            {
                /* see if currently cached folders are below the mounted path.
                 * Folders below the mounted folder are removed. */
                if(FM_IS_DUMMY_MONITOR(folder->mon))
                    dummy_monitor_folders = g_slist_prepend(dummy_monitor_folders, folder);
            }
        }
        fm_path_unref(mounted_path);

        for(l = dummy_monitor_folders; l; l = l->next)
        {
            folder = FM_FOLDER(l->data);
            g_object_ref(folder);
            g_signal_emit_by_name(folder->mon, "changed", folder->gf, NULL, G_FILE_MONITOR_EVENT_UNMOUNTED);
            /* FIXME: should we emit a fake deleted event here? */
            /* g_signal_emit_by_name(folder->mon, "changed", folder->gf, NULL, G_FILE_MONITOR_EVENT_DELETED); */
            g_object_unref(folder);
        }
        g_slist_free(dummy_monitor_folders);
    }
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
