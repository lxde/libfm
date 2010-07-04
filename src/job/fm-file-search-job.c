#include "fm-file-search-job.h"

#include <gio/gio.h> /* for GFile, GFileInfo, GFileEnumerator */
#include <string.h> /* for strstr */
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "fm-list.h"

extern const char gfile_info_query_attribs[]; /* defined in fm-file-info-job.c */

static void fm_file_search_job_finalize(GObject * object);
gboolean fm_file_search_job_run(FmFileSearchJob* job);

G_DEFINE_TYPE(FmFileSearchJob, fm_file_search_job, FM_TYPE_JOB);

static void for_each_target_folder(GFile * path, FmFileSearchJob * job);
static void for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job);
static gboolean file_content_search(GFile * file, GFileInfo * info, FmFileSearchJob * job);
static gboolean file_content_search_ginputstream(GFile * file, GFileInfo * file_info, FmFileSearchJob * job);
static gboolean file_content_search_mmap(GFile * file, GFileInfo * file_info, FmFileSearchJob * job);
static gboolean search(char * haystack, char * needle);

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
	job->target_type = search->target_type; /* does this need to be referenced ? */

	return (FmJob*)job;
}

gboolean fm_file_search_job_run(FmFileSearchJob * job)
{
	/* TODO: error handling (what sort of errors could occur?) */	

	GSList * folders = job->target_folders;

	while(folders != NULL && !fm_job_is_cancelled(FM_JOB(job)))
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
	/* FIXME: 	I added error checking.
				I think I freed up the resources as well. */

	GError * error;
	FmJobErrorAction action;
	GFileEnumerator * enumerator = g_file_enumerate_children(path, gfile_info_query_attribs, G_FILE_QUERY_INFO_NONE, fm_job_get_cancellable(job), error);

	if(enumerator == NULL)
		action = fm_job_emit_error(job, error, FM_JOB_ERROR_SEVERE);
	else /* enumerator opened correctly */
	{
		GFileInfo * file_info = g_file_enumerator_next_file(enumerator, fm_job_get_cancellable(job), error);

		while(file_info != NULL && !fm_job_is_cancelled(FM_JOB(job))) /* g_file_enumerator_next_file returns NULL on error but NULL on finished too */
		{
			for_each_file_info(file_info, path, job);
			file_info = g_file_enumerator_next_file(enumerator, fm_job_get_cancellable(job), error);
		}

		if(error != NULL) /* this should catch g_file_enumerator_next_file errors if they pop up */
			action = fm_job_emit_error(job, error, FM_JOB_ERROR_SEVERE);

		if(!g_file_enumerator_close(enumerator, fm_job_get_cancellable(job), error))
			action = fm_job_emit_error(job, error, FM_JOB_ERROR_MILD);
	}

	if(enumerator != NULL)
		g_object_unref(enumerator);

	if(error != NULL)
		g_error_free(error);

	if(action == FM_JOB_ABORT)
		fm_job_cancel(job);
}

static void for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job)
{	
	/* TODO: error checking ? */

	char * display_name = g_file_info_get_display_name(info); /* does not need to be freed */
	char * name = g_file_info_get_name(info); /* does not need to be freed */
	GFile * file = g_file_get_child(parent, name);

	if(search(display_name, job->target))
	{
		FmPath * path = fm_path_new_for_gfile(file);
		FmFileInfo * file_info = fm_file_info_new_from_gfileinfo(path, info);

		if(job->target_contains != NULL && g_file_info_get_file_type(info) == G_FILE_TYPE_REGULAR)
		{
			if(file_content_search(file, info, job))
				fm_list_push_tail_noref(job->files, file_info); /* file info is referenced when created */
			else
				fm_file_info_unref(file_info);
		}
		else
			fm_list_push_tail_noref(job->files, file_info); /* file info is referenced when created */
		
		fm_path_unref(path);
	}

	/*	TODO: 	checking that mime matches
				use mime type when possible to prevent the unneeded checking of file contents */

	/* recurse upon each directory */
	if(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
		for_each_target_folder(file,job);

	if(file != NULL)
		g_object_unref(file);

	if(info != NULL)
		g_object_unref(info);
}

static gboolean file_content_search(GFile * file, GFileInfo * info, FmFileSearchJob * job)
{
	gboolean ret = FALSE;

	if(g_file_is_native(file))
		ret = file_content_search_mmap(file, info, job);
	else
		ret = file_content_search_ginputstream(file, info, job);

	return ret;
}

static gboolean file_content_search_ginputstream(GFile * file, GFileInfo * file_info, FmFileSearchJob * job)
{
	/* FIXME: 	I added error checking.
				I think I freed up the resources as well. */

	GError * error;
	FmJobErrorAction action;
	gboolean ret = FALSE;
	GFileInputStream * io = g_file_read(file, fm_job_get_cancellable(job), error);

	if(io == G_IO_ERROR_FAILED)
		action = fm_job_emit_error(job, error, FM_JOB_ERROR_SEVERE);
	else /* input stream successfully opened */
	{
		/* FIXME: should I limit the size to read? */
		goffset size = g_file_info_get_size(file_info);
		char buffer[size];
	
		if(g_input_stream_read(io, &buffer, size, fm_job_get_cancellable(job), error) == -1)
			action = fm_job_emit_error(job, error, FM_JOB_ERROR_SEVERE);
		else /* file successfully read into buffer */
		{
			if(search(buffer, job->target_contains))
				ret = TRUE;
		}
		if(!g_input_stream_close(io, fm_job_get_cancellable(job), error))
				action = fm_job_emit_error(job, error, FM_JOB_ERROR_MILD);
	}

	if (io != NULL)
		g_object_unref(io);

	if(error != NULL)
		g_error_free(error);

	if(action == FM_JOB_ABORT)
		fm_job_cancel(job);

	return ret;
}

static gboolean file_content_search_mmap(GFile * file, GFileInfo * file_info, FmFileSearchJob * job)
{
	/* FIXME: 	I reimplemented the error checking; I think it is more sane now. 
				I think I freed up the resources as well.*/

	GError * error;
	FmJobErrorAction action;
	gboolean ret = FALSE;
	size_t size = (size_t)g_file_info_get_size(file_info);
	char * path = g_file_get_path(file);
	char * contents;
	int fd;
	
	fd = open(path, O_RDONLY);
	if(fd == -1)
	{
		error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not open file descriptor for %s\n", path);
		action = fm_job_emit_error(job, error, FM_JOB_ERROR_SEVERE);
	}
	else /* the fd was opened correctly */
	{
		contents = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
		if(contents == MAP_FAILED)
		{
			error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not mmap %s\n", path);
			action = fm_job_emit_error(job, error, FM_JOB_ERROR_SEVERE);
		}
		else /* the file was maped to memory correctly */
		{
			if(search(contents, job->target_contains))
				ret = TRUE;

			if(munmap(contents, size) == -1)
			{
				error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not munmap %s\n", path);
				action = fm_job_emit_error(job, error, FM_JOB_ERROR_MILD);
			}
		}

		if(close(fd) == -1)
		{
			error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not close file descriptor for %s\n", path);
			action = fm_job_emit_error(job, error, FM_JOB_ERROR_MILD);
		}
	}

	if(error != NULL)
		g_error_free(error);

	if(path != NULL)
		g_free(path);

	if(action == FM_JOB_ABORT)
		fm_job_cancel(job);

	return ret;
}

static gboolean search(char * haystack, char * needle)
{
	/* TODO: replace this with a fuzzy search */

	gboolean ret = FALSE;

	if(strstr(haystack, needle) != NULL)
		ret = TRUE;

	return ret;
}

FmFileInfoList * fm_file_search_job_get_files(FmFileSearchJob * job)
{
	return job->files;
}
