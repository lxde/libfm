#include "fm-file-search-job.h"

#include <gio/gio.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>

#define _GNU_SOURCE
#include <fnmatch.h>

#include "fm-list.h"

enum {
    FILES_ADDED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];
 
extern const char gfile_info_query_attribs[]; /* defined in fm-file-info-job.c */

static void fm_file_search_job_finalize(GObject * object);
gboolean fm_file_search_job_run(FmFileSearchJob* job);

G_DEFINE_TYPE(FmFileSearchJob, fm_file_search_job, FM_TYPE_JOB);

static void for_each_target_folder(GFile * path, FmFileSearchJob * job);
static gboolean run_rules_for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job);
static void load_target_folders(FmPath * path, gpointer user_data);

static gboolean file_content_search_ginputstream(char * needle, FmFileSearchFuncData * data);
static gboolean file_content_search_mmap(char * needle, FmFileSearchFuncData * data);
static gboolean content_search(char* needle, char * haystack, FmFileSearchFuncData * data);

static void fm_file_search_job_class_init(FmFileSearchJobClass * klass)
{
	GObjectClass * g_object_class;
	FmJobClass * job_class = FM_JOB_CLASS(klass);
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_search_job_finalize;

	job_class->run = fm_file_search_job_run;

	signals[ FILES_ADDED ] =
        g_signal_new ( "files-added",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFileSearchJobClass, files_added ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );
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
	{
		fm_list_unref(self->files);
		self->files = NULL;
	}

	if(self->rules)
	{
		g_slist_free(self->rules);
		self->rules = NULL;
	}

	if(self->target_folders)
	{
		g_slist_free(self->target_folders);
		self->target_folders = NULL;
	}

	if(self->settings)
	{
		g_slice_free(FmFileSearchSettings, self->settings);
		self->settings = NULL;
	}

	if(self->files_to_add)
	{
		g_slist_free(self->files_to_add);
		self->files_to_add = NULL;
	}

	if (G_OBJECT_CLASS(fm_file_search_job_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_file_search_job_parent_class)->finalize)(object);
}

FmJob * fm_file_search_job_new(GSList * rules, FmPathList * target_folders, FmFileSearchSettings * settings)
{
	FmFileSearchJob * job = (FmJob*)g_object_new(FM_TYPE_FILE_SEARCH_JOB, NULL);

	job->files = fm_file_info_list_new();

	job->rules = g_slist_copy(rules);

	job->target_folders = NULL;
	fm_list_foreach(target_folders, load_target_folders, job);

	job->settings = g_slice_dup(FmFileSearchSettings, settings);

	job->files_to_add = NULL;

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

	GError * error = NULL;
	FmJobErrorAction action = FM_JOB_CONTINUE;
	GFileEnumerator * enumerator = g_file_enumerate_children(path, gfile_info_query_attribs, G_FILE_QUERY_INFO_NONE, fm_job_get_cancellable(job), &error);

	if(enumerator == NULL)
		action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);
	else /* enumerator opened correctly */
	{
		GFileInfo * file_info = g_file_enumerator_next_file(enumerator, fm_job_get_cancellable(job), &error);

		while(file_info != NULL && !fm_job_is_cancelled(FM_JOB(job))) /* g_file_enumerator_next_file returns NULL on error but NULL on finished too */
		{
			if(run_rules_for_each_file_info(file_info, path, job))
			{
				const char * name = g_file_info_get_name(file_info); /* does not need to be freed */
				GFile * file = g_file_get_child(path, name);
				FmPath * path = fm_path_new_for_gfile(file);
				if(path != NULL)
				{
					FmFileInfo * info = fm_file_info_new_from_gfileinfo(path, file_info);
					fm_list_push_tail_noref(job->files, info); /* file info is referenced when created */
					job->files_to_add = g_slist_prepend(job->files_to_add, info);
					g_signal_emit(job, signals[FILES_ADDED], 0, job->files_to_add);
        			g_slist_free(job->files_to_add);
					job->files_to_add = NULL;
				}

				if(path != NULL)
				{
					fm_path_unref(path);
					path = NULL;
				}

				if(file != NULL)
				{
					g_object_unref(file);
					file = NULL;
				}
			}

			if(file_info != NULL)
			{
				g_object_unref(file_info);
				file_info = NULL;
			}

			file_info = g_file_enumerator_next_file(enumerator, fm_job_get_cancellable(job), &error);
		}

		if(error != NULL) /* this should catch g_file_enumerator_next_file errors if they pop up */
			action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);

		if(!g_file_enumerator_close(enumerator, fm_job_get_cancellable(FM_JOB(job)), &error))
			action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_MILD);
	}

	if(enumerator != NULL)
	{
		g_object_unref(enumerator);
		enumerator = NULL;
	}

	if(error != NULL)
	{
		g_error_free(error);
		error = NULL;
	}

	if(job != NULL && action == FM_JOB_ABORT )
		fm_job_cancel(FM_JOB(job));
}

static gboolean run_rules_for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job)
{
	gboolean ret = FALSE;

	if(!g_file_info_get_is_hidden(info) || job->settings->show_hidden)
	{
		const char * name = g_file_info_get_name(info); /* does not need to be freed */
		GFile * file = g_file_get_child(parent, name);

		FmFileSearchFuncData * data = g_slice_new(FmFileSearchFuncData);

		data->current_file = file;
		data->current_file_info = info;
		data->settings = job->settings;
		data->job = job;

		GSList * rules = job->rules;

		while(rules != NULL && !fm_job_is_cancelled(FM_JOB(job)))
		{
			FmFileSearchRule * search_rule = rules->data;

			FmFileSearchFunc search_function = (*search_rule->function);

			ret = search_function(data, search_rule->user_data);

			if(!ret)
				break;

			rules = rules->next;
		}

		/* recurse upon each directory */
		if(job->settings->recursive && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
			for_each_target_folder(file,job);

		if(file != NULL)
		{
			g_object_unref(file);
			file = NULL;
		}
		if(data != NULL)
		{
			g_slice_free(FmFileSearchFuncData, data);
			data = NULL;
		}
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

/* functions for content search rule */

static void lowercase(char * s)
{
	int i = 0;
	while(s[i])
	{
		s[i]= tolower(s[i]);
		i++;
	}
}

static gboolean strstr_nocase(char * haystack, char * needle)
{
	gboolean ret = FALSE;
	char * haystack_nocase = g_strdup(haystack);
	char * needle_nocase = g_strdup(needle);

	lowercase(haystack_nocase);
	lowercase(needle_nocase);

	if(strstr(haystack_nocase, needle_nocase) != NULL)
		ret = TRUE;

	g_free(haystack_nocase);
	g_free(needle_nocase);

	return ret;
}

static gboolean file_content_search_ginputstream(char * needle, FmFileSearchFuncData * data)
{
	/* FIXME: 	I added error checking.
				I think I freed up the resources as well. */
	/* TODO: 	Rewrite this to read into a buffer, but check across the break for matches */
	GError * error = NULL;
	FmJobErrorAction action = FM_JOB_CONTINUE;
	gboolean ret = FALSE;
	GFileInputStream * io = g_file_read(data->current_file, fm_job_get_cancellable(data->job), &error);

	if(io == G_IO_ERROR_FAILED)
		action = fm_job_emit_error(FM_JOB(data->job), error, FM_JOB_ERROR_SEVERE);
	else /* input stream successfully opened */
	{
		/* FIXME: should I limit the size to read? */
		goffset size = g_file_info_get_size(data->current_file_info);
		char buffer[size];
	
		if(g_input_stream_read(G_INPUT_STREAM(io), &buffer, size, fm_job_get_cancellable(data->job), error) == -1)
			action = fm_job_emit_error(FM_JOB(data->job), error, FM_JOB_ERROR_SEVERE);
		else /* file successfully read into buffer */
		{
			if(content_search(needle, buffer, data))
				ret = TRUE;
		}
		if(!g_input_stream_close(io, fm_job_get_cancellable(data->job), &error))
				action = fm_job_emit_error(FM_JOB(data->job), error, FM_JOB_ERROR_MILD);
	}

	if (io != NULL)
	{
		g_object_unref(io);
		io = NULL;
	}

	if(error != NULL)
	{
		g_error_free(error);
		error = NULL;
	}

	if(action == FM_JOB_ABORT)
		fm_job_cancel(FM_JOB(data->job));

	return ret;
}

static gboolean file_content_search_mmap(char * needle, FmFileSearchFuncData * data)
{
	/* FIXME: 	I reimplemented the error checking; I think it is more sane now. 
				I think I freed up the resources as well.*/

	GError * error = NULL;
	FmJobErrorAction action = FM_JOB_CONTINUE;
	gboolean ret = FALSE;
	size_t size = (size_t)g_file_info_get_size(data->current_file_info);
	char * path = g_file_get_path(data->current_file);
	char * contents;
	int fd;
	
	fd = open(path, O_RDONLY);
	if(fd == -1)
	{
		error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not open file descriptor for %s\n", path);
		action = fm_job_emit_error(FM_JOB(data->job), error, FM_JOB_ERROR_SEVERE);
	}
	else /* the fd was opened correctly */
	{
		contents = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
		if(contents == MAP_FAILED)
		{
			error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not mmap %s\n", path);
			action = fm_job_emit_error(FM_JOB(data->job), error, FM_JOB_ERROR_SEVERE);
		}
		else /* the file was maped to memory correctly */
		{
			if(content_search(needle, contents, data))
				ret = TRUE;

			if(munmap(contents, size) == -1)
			{
				error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not munmap %s\n", path);
				action = fm_job_emit_error(FM_JOB(data->job), error, FM_JOB_ERROR_MILD);
			}
		}

		if(close(fd) == -1)
		{
			error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not close file descriptor for %s\n", path);
			action = fm_job_emit_error(FM_JOB(data->job), error, FM_JOB_ERROR_MILD);
		}
	}

	if(error != NULL)
	{
		g_error_free(error);
		error = NULL;
	}

	if(path != NULL)
	{
		g_free(path);
		path = NULL;
	}

	if(action == FM_JOB_ABORT)
		fm_job_cancel(FM_JOB(data->job));

	return ret;
}

static gboolean content_search(char* needle, char * haystack, FmFileSearchFuncData * data)
{
	gboolean ret = FALSE;

	if(data->settings->content_mode == FM_FILE_SEARCH_MODE_REGEX)
		ret = g_regex_match_simple(needle, haystack, (!data->settings->case_sensitive_content) ? G_REGEX_CASELESS : 0, 0);

	else if(data->settings->case_sensitive_content)
	{
	 	if(strstr(haystack, needle) != NULL)
			ret = TRUE;
	}

	else if(strstr_nocase(haystack, needle))
		ret = TRUE;

	return ret;
}

/* end of functions for content search rule */

/* rule functions */

gboolean fm_file_search_target_rule(FmFileSearchFuncData * data, gpointer user_data)
{
	gboolean ret = FALSE;
	const char * display_name = g_file_info_get_display_name(data->current_file_info); /* does not need to be freed */

	if(data->settings->target_mode == FM_FILE_SEARCH_MODE_REGEX)
	{
		ret = g_regex_match_simple(user_data, display_name, (!data->settings->case_sensitive_target) ? G_REGEX_CASELESS : 0, 0);
	}
	else
	{
		if(data->settings->case_sensitive_target)
		{
			if(fnmatch(user_data, display_name, 0) == 0)
				ret = TRUE;
		}
		else
		{
			if(fnmatch(user_data, display_name, FNM_CASEFOLD) == 0)
				ret = TRUE;
		}
	}

	return ret;
}

gboolean fm_file_search_target_contains_rule(FmFileSearchFuncData * data, gpointer user_data)
{
	gboolean ret = FALSE;

	if(g_file_info_get_file_type(data->current_file_info) == G_FILE_TYPE_REGULAR && g_file_info_get_size(data->current_file_info) > 0)
	{
		if(g_file_is_native(data->current_file))
			ret = file_content_search_mmap(user_data, data);
		else
			ret = file_content_search_ginputstream(user_data, data);
	}

	return ret;
}

gboolean fm_file_search_target_type_rule(FmFileSearchFuncData * data, gpointer user_data)
{
	gboolean ret = FALSE;

	char * target_type = user_data;

	FmPath * path = fm_path_new_for_gfile(data->current_file);
	FmFileInfo * file_info = fm_file_info_new_from_gfileinfo(path, data->current_file_info);
	FmMimeType * file_mime = fm_file_info_get_mime_type(file_info);
	const char * file_type = fm_mime_type_get_type(file_mime);

	if(g_strcmp0(file_type, target_type) == 0)
		ret = TRUE;

	if(file_mime != NULL)
	{
		fm_mime_type_unref(file_mime);
		file_mime = NULL;
	}

	if(file_info != NULL)
	{
		fm_file_info_unref(file_info);
		file_info = NULL;
	}

	if(path != NULL)
	{
		fm_path_unref(path);
		path = NULL;
	}

	return ret;
}

gboolean fm_file_search_minimum_size_rule(FmFileSearchFuncData * data, gpointer user_data)
{
	gboolean ret = FALSE;

	goffset * minimum_size = user_data;

	if(g_file_info_get_size(data->current_file_info) >= *minimum_size && g_file_info_get_file_type(data->current_file_info) != G_FILE_TYPE_DIRECTORY)
		ret = TRUE;

	return ret;
}

gboolean fm_file_search_maximum_size_rule(FmFileSearchFuncData * data, gpointer user_data)
{
	gboolean ret = FALSE;

	goffset * maximum_size = user_data;

	if(g_file_info_get_size(data->current_file_info) <= *maximum_size && g_file_info_get_file_type(data->current_file_info) != G_FILE_TYPE_DIRECTORY)
		ret = TRUE;

	return ret;
}

gboolean fm_file_search_modified_time_rule(FmFileSearchFuncData * data, gpointer user_data)
{
	gboolean ret = FALSE;

	FmFileSearchModifiedTimeRuleData * time_data = (FmFileSearchModifiedTimeRuleData *)user_data;
	GTimeVal mod_time;
	g_file_info_get_modification_time(data->current_file_info, &mod_time);

	time_t current_time = time(NULL);
	struct tm starting_time;
	struct tm ending_time;

	starting_time.tm_sec = 0;
	starting_time.tm_min = 0;
	starting_time.tm_hour = 0;
	starting_time.tm_mday = time_data->start_d;
	starting_time.tm_mon = time_data->start_m;
	starting_time.tm_year = time_data->start_y - 1900;
	starting_time.tm_isdst = localtime(&current_time)->tm_isdst;

	ending_time.tm_sec = 59;
	ending_time.tm_min = 59;
	ending_time.tm_hour = 23;
	ending_time.tm_mday = time_data->end_d;
	ending_time.tm_mon = time_data->end_m;
	ending_time.tm_year = time_data->end_y - 1900;
	ending_time.tm_isdst = starting_time.tm_isdst;

	time_t start = mktime(&starting_time);
	time_t end = mktime(&ending_time);

	if(mod_time.tv_sec >= start && mod_time.tv_sec <= end)
		ret = TRUE;

	return ret;
}

/* end of rule functions */
