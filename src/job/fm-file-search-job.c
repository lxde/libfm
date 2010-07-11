#include "fm-file-search-job.h"

#include <gio/gio.h> /* for GFile, GFileInfo, GFileEnumerator */
#include <string.h> /* for strstr */
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#define _GNU_SOURCE 1
#include <fnmatch.h>

#include "fm-list.h"

typedef enum _SearchType
{
	SEARCH_TYPE_FILE_NAME,
	SEARCH_TYPE_CONTENT
} SearchType;

extern const char gfile_info_query_attribs[]; /* defined in fm-file-info-job.c */

static void fm_file_search_job_finalize(GObject * object);
gboolean fm_file_search_job_run(FmFileSearchJob* job);

G_DEFINE_TYPE(FmFileSearchJob, fm_file_search_job, FM_TYPE_JOB);

static void for_each_target_folder(GFile * path, FmFileSearchJob * job);
static void for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job);
static gboolean file_content_search(GFile * file, GFileInfo * info, FmFileSearchJob * job);
static gboolean file_content_search_ginputstream(GFile * file, GFileInfo * file_info, FmFileSearchJob * job);
static gboolean file_content_search_mmap(GFile * file, GFileInfo * file_info, FmFileSearchJob * job);
static gboolean search(char * haystack, char * needle, SearchType type, FmFileSearchJob * job);
static void load_target_folders(FmPath * path, gpointer user_data);

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

	if(self->target_regex)
		g_regex_unref(self->target_regex);

	if(self->target_contains_regex)
		g_regex_unref(self->target_contains_regex);

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
	job->target = fm_file_search_get_target(search);
	job->target_mode = search->target_mode;

	job->case_sensitive = search->case_sensitive;

	if(job->target_mode == FM_FILE_SEARCH_MODE_REGEX && job->target != NULL)
	{
		if(job->case_sensitive)
			job->target_regex = g_regex_new(job->target, G_REGEX_OPTIMIZE, 0, NULL);
		else
			job->target_regex = g_regex_new(job->target, G_REGEX_OPTIMIZE | G_REGEX_CASELESS, 0, NULL);
	}

	job->target_contains = fm_file_search_get_target_contains(search);
	job->content_mode = search->content_mode;
	if(job->content_mode == FM_FILE_SEARCH_MODE_REGEX && job->target_contains != NULL)
	{
		if(job->case_sensitive)
			job->target_contains_regex = g_regex_new(job->target_contains, G_REGEX_OPTIMIZE, 0, NULL);
		else
			job->target_contains_regex = g_regex_new(job->target_contains, G_REGEX_OPTIMIZE | G_REGEX_CASELESS, 0, NULL);
	}

	job->target_folders = NULL; /* list of GFile * */
	fm_list_foreach(search->target_folders, load_target_folders, job);
	job->target_type = fm_file_search_get_target_type(search);

	job->show_hidden = search->show_hidden;
	job->recursive = search->recursive;

	job->check_minimum_size = search->check_minimum_size;
	job->check_maximum_size = search->check_maximum_size;
	job->minimum_size = search->minimum_size;
	job->maximum_size = search->maximum_size;

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
		action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);
	else /* enumerator opened correctly */
	{
		GFileInfo * file_info = g_file_enumerator_next_file(enumerator, fm_job_get_cancellable(job), error);

		while(file_info != NULL && !fm_job_is_cancelled(FM_JOB(job))) /* g_file_enumerator_next_file returns NULL on error but NULL on finished too */
		{
			for_each_file_info(file_info, path, job);

			if(file_info != NULL)
				g_object_unref(file_info);

			file_info = g_file_enumerator_next_file(enumerator, fm_job_get_cancellable(job), error);
		}

		if(error != NULL) /* this should catch g_file_enumerator_next_file errors if they pop up */
			action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);

		if(!g_file_enumerator_close(enumerator, fm_job_get_cancellable(FM_JOB(job)), error))
			action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_MILD);
	}

	if(enumerator != NULL)
		g_object_unref(enumerator);

	if(error != NULL)
		g_error_free(error);

	if(action == FM_JOB_ABORT)
		fm_job_cancel(FM_JOB(job));
}

static void for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job)
{	
	/* TODO: error checking ? */

	if(!g_file_info_get_is_hidden(info) || job->show_hidden)
	{
		if((!job->check_minimum_size || g_file_info_get_size(info) >= job->minimum_size) && (!job->check_maximum_size || g_file_info_get_size(info) <= job->maximum_size)) /* file size check */
		{
			const char * display_name = g_file_info_get_display_name(info); /* does not need to be freed */
			const char * name = g_file_info_get_name(info); /* does not need to be freed */
			GFile * file = g_file_get_child(parent, name);

			if(job->target == NULL || search(display_name, job->target, SEARCH_TYPE_FILE_NAME, job)) /* target search */
			{
				FmPath * path = fm_path_new_for_gfile(file);
				FmFileInfo * file_info = fm_file_info_new_from_gfileinfo(path, info);
				FmMimeType * file_mime = fm_file_info_get_mime_type(file_info);
				const char * file_type = fm_mime_type_get_type(file_mime);
				const char * target_file_type = (job->target_type != NULL ? fm_mime_type_get_type(job->target_type) : NULL);

				if(job->target_type == NULL || g_strcmp0(file_type, target_file_type) == 0) /* mime type search */
				{
					if(job->target_contains != NULL && g_file_info_get_file_type(info) == G_FILE_TYPE_REGULAR) /* target content search */
					{
						if(file_content_search(file, info, job))
							fm_list_push_tail_noref(job->files, file_info); /* file info is referenced when created */
						else
							fm_file_info_unref(file_info);
					}
					else
						fm_list_push_tail_noref(job->files, file_info); /* file info is referenced when created */
				}
				else
					fm_file_info_unref(file_info);

				if(path != NULL)
					fm_path_unref(path);
			}


			/* recurse upon each directory */
			if(job->recursive && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
				for_each_target_folder(file,job);

			if(file != NULL)
				g_object_unref(file);
		}
	}	
}

static gboolean file_content_search(GFile * file, GFileInfo * info, FmFileSearchJob * job)
{
	gboolean ret = TRUE;

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
	/* TODO: 	Rewrite this to read into a buffer, but check across the break for matches */
	GError * error;
	FmJobErrorAction action;
	gboolean ret = FALSE;
	GFileInputStream * io = g_file_read(file, fm_job_get_cancellable(job), error);

	if(io == G_IO_ERROR_FAILED)
		action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);
	else /* input stream successfully opened */
	{
		/* FIXME: should I limit the size to read? */
		goffset size = g_file_info_get_size(file_info);
		char buffer[size];
	
		if(g_input_stream_read(G_INPUT_STREAM(io), &buffer, size, fm_job_get_cancellable(job), error) == -1)
			action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);
		else /* file successfully read into buffer */
		{
			if(search(buffer, job->target_contains, SEARCH_TYPE_CONTENT, job))
				ret = TRUE;
		}
		if(!g_input_stream_close(io, fm_job_get_cancellable(job), error))
				action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_MILD);
	}

	if (io != NULL)
		g_object_unref(io);

	if(error != NULL)
		g_error_free(error);

	if(action == FM_JOB_ABORT)
		fm_job_cancel(FM_JOB(job));

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
		action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);
	}
	else /* the fd was opened correctly */
	{
		contents = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
		if(contents == MAP_FAILED)
		{
			error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not mmap %s\n", path);
			action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);
		}
		else /* the file was maped to memory correctly */
		{
			if(search(contents, job->target_contains, SEARCH_TYPE_CONTENT, job))
				ret = TRUE;

			if(munmap(contents, size) == -1)
			{
				error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not munmap %s\n", path);
				action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_MILD);
			}
		}

		if(close(fd) == -1)
		{
			error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not close file descriptor for %s\n", path);
			action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_MILD);
		}
	}

	if(error != NULL)
		g_error_free(error);

	if(path != NULL)
		g_free(path);

	if(action == FM_JOB_ABORT)
		fm_job_cancel(FM_JOB(job));

	return ret;
}

static gboolean search(char * haystack, char * needle, SearchType type, FmFileSearchJob * job)
{
	gboolean ret = FALSE;
	FmFileSearchMode mode = ( type == SEARCH_TYPE_FILE_NAME ? job->target_mode : job->content_mode );

	if(mode == FM_FILE_SEARCH_MODE_REGEX)
	{
		if(type == SEARCH_TYPE_FILE_NAME)
			ret = g_regex_match(job->target_regex, haystack, 0, NULL);
		else
			ret = g_regex_match(job->target_contains_regex, haystack, 0, NULL);
	}
	else if(mode == FM_FILE_SEARCH_MODE_EXACT)
	{
		if(job->case_sensitive)
		{
			if(fnmatch(needle, haystack, 0) == 0)
				ret = TRUE;
		}
		else
		{
			if(fnmatch(needle, haystack, FNM_CASEFOLD) == 0)
				ret = TRUE;
		}
	}
	else
	{
		if(strstr(haystack, needle) != NULL)
			ret = TRUE;
	}
	return ret;
}

static void load_target_folders(FmPath * path, gpointer user_data)
{
	FmFileSearchJob * job = FM_FILE_SEARCH_JOB(user_data);
	job->target_folders = g_slist_append(job->target_folders, fm_path_to_gfile(path));
}

FmFileInfoList * fm_file_search_job_get_files(FmFileSearchJob * job)
{
	return job->files;
}
