/*
 *      fm-folder.c
 *      
 *      Copyright 2009 PCMan <pcman@thinkpad>
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
#include <string.h>


#define MONITOR_RATE_LIMIT 5000

enum {
	FILES_ADDED,
	FILES_REMOVED,
	FILES_CHANGED,
	N_SIGNALS
};

static FmFolder* fm_folder_new_internal(FmPath* path, GFile* gf);
static void fm_folder_finalize  			(GObject *object);
G_DEFINE_TYPE(FmFolder, fm_folder, G_TYPE_OBJECT);

static guint signals[N_SIGNALS];

static GList* _fm_folder_get_file_by_name(FmFolder* folder, const char* name);


static void fm_folder_class_init(FmFolderClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_folder_finalize;
	fm_folder_parent_class = (GObjectClass*)g_type_class_peek(G_TYPE_OBJECT);

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
}


static void fm_folder_init(FmFolder *self)
{
	self->files = fm_file_info_list_new();
}

static gboolean on_idle(FmFolder* folder)
{
    GSList* l;
    if(folder->files_to_add)
    {
        g_signal_emit(folder, signals[FILES_ADDED], 0, folder->files_to_add);
        g_slist_free(folder->files_to_add);
        folder->files_to_add = NULL;
    }
    if(folder->files_to_update)
    {
        g_signal_emit(folder, signals[FILES_CHANGED], 0, folder->files_to_update);
        g_slist_free(folder->files_to_update);
        folder->files_to_update = NULL;
    }
    if(folder->files_to_del)
    {
        g_signal_emit(folder, signals[FILES_REMOVED], 0, folder->files_to_del);
        g_slist_free(folder->files_to_del);
        folder->files_to_del = NULL;
    }
    folder->idle_handler = 0;
    return FALSE;
}

static void on_folder_changed(GFileMonitor* mon, GFile* gf, GFile* other, GFileMonitorEvent evt, FmFolder* folder)
{
    GList* l;
    FmFileInfo* fi;
    char* name;
	const char* names[]={
		"G_FILE_MONITOR_EVENT_CHANGED",
		"G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT",
		"G_FILE_MONITOR_EVENT_DELETED",
		"G_FILE_MONITOR_EVENT_CREATED",
		"G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED",
		"G_FILE_MONITOR_EVENT_PRE_UNMOUNT",
		"G_FILE_MONITOR_EVENT_UNMOUNTED"
	};
	char* file = g_file_get_basename(gf);
	g_debug("folder %p %s event: %s", folder, file, names[evt]);
	g_free(file);
	/* FIXME: need to query file info asynchronously and add them the the list. */

    switch(evt)
    {
    case G_FILE_MONITOR_EVENT_CREATED:
        break;
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        break;
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        break;
    case G_FILE_MONITOR_EVENT_DELETED:
        name = g_file_get_basename(gf);
        l = _fm_folder_get_file_by_name(folder, name);
        if(l)
        {
            folder->files_to_del = g_slist_prepend(folder->files_to_del, l->data);
            fm_list_delete_link_nounref(folder->files , l);
        }
        g_free(name);
        break;
    default:
        return;
    }
    if(!folder->idle_handler)
        folder->idle_handler = g_idle_add(on_idle, folder);
}

/* FIXME: use our own implementation for local files. */
static void on_job_finished(FmDirListJob* job, FmFolder* folder)
{
	GList* l;
	GSList* files = NULL;
	/* actually manually disconnecting from 'finished' signal is not 
	 * needed since the signal is only emit once, and later the job 
	 * object will be distroyed very soon. */
	/* g_signal_handlers_disconnect_by_func(job, on_job_finished, folder); */
	g_debug("%d files are listed", fm_list_get_length(job->files) );
	for(l = fm_list_peek_head_link(job->files); l; l=l->next )
	{
		FmFileInfo* inf = (FmFileInfo*)l->data;
		files = g_slist_prepend(files, inf);
		fm_list_push_tail(folder->files, inf);
	}
	g_signal_emit(folder, signals[FILES_ADDED], 0, files);

	folder->job = NULL; /* the job object will be freed in idle handler. */
}

FmFolder* fm_folder_new_internal(FmPath* path, GFile* gf)
{
	GError* err = NULL;
	FmFolder* folder = (FmFolder*)g_object_new(FM_TYPE_FOLDER, NULL);
	folder->dir_path = fm_path_ref(path);

	folder->gf = (GFile*)g_object_ref(gf);
	folder->mon = g_file_monitor_directory(gf, G_FILE_MONITOR_WATCH_MOUNTS, NULL, &err);
	if(folder->mon)
	{
		g_file_monitor_set_rate_limit(folder->mon, MONITOR_RATE_LIMIT);
		g_signal_connect(folder->mon, "changed", G_CALLBACK(on_folder_changed), folder );
	}
	else
	{
		if(err)
		{
			g_debug(err->message);
			g_error_free(err);
		}
	}

	folder->job = fm_dir_list_job_new(path);
	g_signal_connect(folder->job, "finished", G_CALLBACK(on_job_finished), folder);
	fm_job_run_async(folder->job);
	return folder;
}

FmFolder* fm_folder_new(FmPath* path)
{
	GFile* gf = fm_path_to_gfile(path);
	FmFolder* folder = fm_folder_new_internal(path, gf);
	g_object_unref(gf);
	return folder;
}


static void fm_folder_finalize(GObject *object)
{
	FmFolder *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_FOLDER(object));

	self = FM_FOLDER(object);

	if(self->job)
	{
		g_signal_handlers_disconnect_by_func(self->job, on_job_finished, self);
		fm_job_cancel(self->job); /* FIXME: is this ok? */
		/* the job will be freed in idle handler. */
	}

	if(self->dir_path)
		fm_path_unref(self->dir_path);

	if(self->gf)
		g_object_unref(self->gf);

	if(self->mon)
	{
		g_signal_handlers_disconnect_by_func(self->mon, on_folder_changed, self);
		g_object_unref(self->mon);
	}

	fm_list_unref(self->files);

	if (G_OBJECT_CLASS(fm_folder_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_folder_parent_class)->finalize)(object);
}

FmFolder*	fm_folder_new_for_gfile	(GFile* gf)
{
	FmPath* path = fm_path_new_for_gfile(gf);
	FmFolder* folder = fm_folder_new_internal(path, gf);
	fm_path_unref(path);
	return folder;
}

FmFolder*	fm_folder_new_for_path	(const char* path)
{
	FmPath* fm_path = fm_path_new(path);
	FmFolder* folder = fm_folder_new(fm_path);
	fm_path_unref(fm_path);
	return folder;
}

/* FIXME: should we use GFile here? */
FmFolder*	fm_folder_new_for_uri	(const char* uri)
{
	GFile* gf = g_file_new_for_uri(uri);
	FmFolder* folder = fm_folder_new_for_gfile(gf);
	g_object_unref(gf);
	return folder;	
}

void fm_folder_reload(FmFolder* folder)
{
	/* FIXME: remove all items and re-run a dir list job. */
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
