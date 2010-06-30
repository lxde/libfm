#include "fm-file-search-job.h"

#include <gio/gio.h>

#include "fm-folder.h"
#include "fm-list.h"
#include <string.h> /* for strstr */

static void fm_file_search_job_finalize(GObject * object);
gboolean fm_file_search_job_run(FmFileSearchJob* job);

G_DEFINE_TYPE(FmFileSearchJob, fm_file_search_job, FM_TYPE_JOB);

static void for_each_target_folder(GFile * path, FmFileSearchJob * job);
static void for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job);

static void fm_file_search_job_class_init(FmFileSearchJobClass * klass)
{
	GObjectClass * g_object_class;
	FmJobClass * job_class = FM_JOB_CLASS(klass);
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_search_job_finalize;

	job_class->run = fm_file_search_job_run;
}

static void fm_file_search_job_init(FmFileSearchJob * self)
{
    fm_job_init_cancellable(FM_JOB(self));
}

static void fm_file_search_job_finalize(GObject * object)
{
	FmFileSearchJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_FILE_SEARCH_JOB(object));

	self = FM_FILE_SEARCH_JOB(object);

	if(self->files)
		fm_list_unref(self->files);

	if(self->target)
		g_free(self->target);
		
	if(self->target_contains)
		g_free(self->target_contains);

	if(self->target_folders)
		g_slist_free(self->target_folders);

	if(self->target_type)
		fm_mime_type_unref(self->target_type);

	if (G_OBJECT_CLASS(fm_file_search_job_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_file_search_job_parent_class)->finalize)(object);
}

FmJob * fm_file_search_job_new(FmFileSearch * search)
{
	FmFileSearchJob * job = (FmJob*)g_object_new(FM_TYPE_FILE_SEARCH_JOB, NULL);

	job->files = fm_file_info_list_new();
	job->target = g_strdup(search->target);
	job->target_contains = g_strdup(search->target_contains);
	job->target_folders = g_slist_copy(search->target_folders);
	job->target_type = search->target_type; /* does this need to be referenced */

	return (FmJob*)job;
}

gboolean fm_file_search_job_run(FmFileSearchJob * job)
{
	/* TODO: error handling (what sort of errors could occur?) */	

	GSList * folders = job->target_folders;

	while(folders != NULL)
	{
		GFile * path = G_FILE(folders->data); //each one should represent a directory
		/* emit error if one is not a directory */
		for_each_target_folder(path, job);

		folders = folders->next;
	}
	
	return TRUE;
}

static void for_each_target_folder(GFile * path, FmFileSearchJob * job)
{
	/* TODO: handle if the job is canceled since this is called by a for each */

	GFileEnumerator * enumerator = g_file_enumerate_children(path, "standard::*", G_FILE_QUERY_INFO_NONE, NULL, NULL);

	GFileInfo * file_info = g_file_enumerator_next_file(enumerator, NULL, NULL);

	while(file_info != NULL)
	{
		for_each_file_info(file_info, path, job);
		file_info = g_file_enumerator_next_file(enumerator, NULL, NULL);
	}
}

static void for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job)
{
	
	/* TODO: handle if the job is canceled since this is called by a for each */
	
	if(strstr(g_file_info_get_display_name(info), job->target) != NULL)
	{
		GFile * file = g_file_get_child(parent, g_file_info_get_name(info));
		FmFileInfo * file_info = fm_file_info_new_from_gfileinfo(fm_path_new_for_gfile(file), info);

		fm_list_push_tail_noref(job->files, file_info); /* should be referenced because the file list will be freed ? */
	}

	/* TODO: checking that mime matches */
	/* TODO: checking contents of files */
	/* TODO: use mime type when possible to prevent the unneeded checking of file contents */

	/* recurse upon each directory */
	if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
	{
		GFile * file = g_file_get_child(parent, g_file_info_get_name(info));
		for_each_target_folder(file,job);
	}
}

FmFileInfoList* fm_file_search_job_get_files(FmFileSearchJob* job)
{
	return job->files;
}
