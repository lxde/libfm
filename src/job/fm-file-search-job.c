/*
 * fm-file-search-job.c
 * 
 * Copyright 2010 Shae Smittle <starfall87@gmail.com>
 * Copyright 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include "fm-file-search-job.h"

#include <gio/gio.h>
#include <stdio.h>
/*
#include <fcntl.h>
#include <sys/mman.h>
*/
#include <string.h>
#include <ctype.h>

#define _GNU_SOURCE
#include <fnmatch.h>

#include "fm-list.h"

enum {
    FILES_ADDED,
    N_SIGNALS
};

struct _FmFileSearchJob
{
	FmJob parent;
	FmFileInfoList * files;
	GSList * rules;
	FmPathList * target_folders;
	FmFileSearchSettings * settings;
	GRegex * target_regex;
	GRegex * target_contains_regex;

	GSList * files_to_add;
	guint idle_add_file_handler;
};

static guint signals[N_SIGNALS];
 
extern const char gfile_info_query_attribs[]; /* defined in fm-file-info-job.c */

static void fm_file_search_job_finalize(GObject * object);
static void fm_file_search_job_dispose(GObject * object);
gboolean fm_file_search_job_run(FmFileSearchJob* job);

G_DEFINE_TYPE(FmFileSearchJob, fm_file_search_job, FM_TYPE_JOB);

static void run_rules_for_each_target_folder(GFile * folder_path, FmFileSearchJob * job);
static gboolean run_rules_for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job);

static gboolean file_content_search_ginputstream(char * needle, FmFileSearchFuncData * data);
/*static gboolean file_content_search_mmap(char * needle, FmFileSearchFuncData * data); */
static gboolean content_search(char* needle, char * haystack, FmFileSearchFuncData * data);

static void queue_add_file(FmFileSearchJob* job, FmFileInfo* file);

static void fm_file_search_job_class_init(FmFileSearchJobClass * klass)
{
	GObjectClass * g_object_class;
	FmJobClass * job_class = FM_JOB_CLASS(klass);
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->dispose = fm_file_search_job_dispose;
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

static void fm_file_search_job_init(FmFileSearchJob * job)
{
    fm_job_init_cancellable(FM_JOB(job));
}

static void fm_file_search_job_dispose(GObject * object)
{
	FmFileSearchJob *job = FM_FILE_SEARCH_JOB(object);
	if(job->files)
	{
		fm_list_unref(job->files);
		job->files = NULL;
	}

	if(job->rules)
	{
		g_slist_free(job->rules);
		job->rules = NULL;
	}

	if(job->target_folders)
	{
		g_slist_free(job->target_folders);
		job->target_folders = NULL;
	}

	if(job->settings)
	{
		g_slice_free(FmFileSearchSettings, job->settings);
		job->settings = NULL;
	}

	if(job->files_to_add)
	{
		g_slist_free(job->files_to_add);
		job->files_to_add = NULL;
	}

	if(job->target_regex)
	{
		g_regex_unref(job->target_regex);
		job->target_regex = NULL;
	}

	if(job->target_contains_regex)
	{
		g_regex_unref(job->target_contains_regex);
		job->target_contains_regex = NULL;
	}

	if(job->files_to_add)
	{
		g_slist_foreach(job->files_to_add, (GFunc)fm_file_info_unref, NULL);
		g_slist_free(job->files_to_add);
		job->files_to_add = NULL;
	}

	if(job->idle_add_file_handler)
	{
		g_source_remove(job->idle_add_file_handler);
		job->idle_add_file_handler = 0;
	}
	if (G_OBJECT_CLASS(fm_file_search_job_parent_class)->dispose)
		(* G_OBJECT_CLASS(fm_file_search_job_parent_class)->dispose)(object);
}

static void fm_file_search_job_finalize(GObject * object)
{
	FmFileSearchJob *job;
	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_FILE_SEARCH_JOB(object));
	job = FM_FILE_SEARCH_JOB(object);

	if (G_OBJECT_CLASS(fm_file_search_job_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_file_search_job_parent_class)->finalize)(object);
}

FmJob * fm_file_search_job_new(GSList * rules, FmPathList * target_folders, FmFileSearchSettings * settings)
{
	FmFileSearchJob * job = (FmJob*)g_object_new(FM_TYPE_FILE_SEARCH_JOB, NULL);
	GList* l;

	job->files = fm_file_info_list_new();
	job->rules = g_slist_copy(rules);
	job->target_folders = NULL;
	for(l = fm_path_list_peek_head_link(target_folders); l; l=l->next)
		job->target_folders = g_slist_append(job->target_folders, fm_path_to_gfile(FM_PATH(l->data)));
	job->settings = g_slice_dup(FmFileSearchSettings, settings);
	job->files_to_add = NULL;
	job->target_regex = NULL;
	job->target_contains_regex = NULL;
	return (FmJob*)job;
}

gboolean fm_file_search_job_run(FmFileSearchJob * job)
{
	/* TODO: error handling (what sort of errors could occur?) */	
	GSList * folders = job->target_folders;
	while(folders != NULL && !fm_job_is_cancelled(FM_JOB(job)))
	{
		GFile * folder_path = G_FILE(folders->data); //each one should represent a directory
		/* emit error if one is not a directory */
		run_rules_for_each_target_folder(folder_path, job);
		folders = folders->next;
	}
	return TRUE;
}

static void run_rules_for_each_target_folder(GFile * folder_path, FmFileSearchJob * job)
{
	GError * error = NULL;
	FmJobErrorAction action = FM_JOB_CONTINUE;
	GFileEnumerator * enumerator = g_file_enumerate_children(folder_path, gfile_info_query_attribs, G_FILE_QUERY_INFO_NONE, fm_job_get_cancellable(job), &error);
	if(enumerator == NULL)
		action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);
	else /* enumerator opened correctly */
	{
		while(!fm_job_is_cancelled(FM_JOB(job)))
		{
			GFileInfo * file_info = g_file_enumerator_next_file(enumerator, fm_job_get_cancellable(job), &error);
			if(file_info)
			{
				if(run_rules_for_each_file_info(file_info, folder_path, job))
				{
					const char * name = g_file_info_get_name(file_info); /* does not need to be freed */
					GFile *file = g_file_get_child(folder_path, name);
					FmPath * path = fm_path_new_for_gfile(file);
					g_object_unref(file);

					if(path != NULL)
					{
						FmFileInfo * info = fm_file_info_new_from_gfileinfo(path, file_info);
						queue_add_file(job, info);
						fm_file_info_unref(info);
						fm_path_unref(path);
						path = NULL;
					}
				}
				g_object_unref(file_info);
			}
			else /* file_info == NULL */
			{
				if(error != NULL) /* this should catch g_file_enumerator_next_file errors if they pop up */
				{
					action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_SEVERE);
					g_error_free(error);
					error = NULL;
					if(action == FM_JOB_ABORT)
						break;
				}
				else /* end of file list */
					break;
			}
		}
		g_file_enumerator_close(enumerator, fm_job_get_cancellable(FM_JOB(job)), NULL);
		g_object_unref(enumerator);
	}

	if(job != NULL && action == FM_JOB_ABORT )
		fm_job_cancel(FM_JOB(job));
}

static gboolean on_idle_add_files(gpointer user_data)
{
	FmFileSearchJob* job = FM_FILE_SEARCH_JOB(user_data);
	GSList* l;

	for(l = job->files_to_add; l; l = l->next)
	{
		FmFileInfo* file = FM_FILE_INFO(l->data);
		fm_file_info_list_push_tail_noref(job->files, file);
	}
	g_signal_emit(job, signals[FILES_ADDED], 0, job->files_to_add);
	g_slist_free(job->files_to_add);
	job->files_to_add = NULL;
	job->idle_add_file_handler = 0;
	return FALSE;
}

static void queue_add_file(FmFileSearchJob* job, FmFileInfo* file)
{
	/* group the newly founded files and emit files-added signals less frequently */
	job->files_to_add = g_slist_prepend(job->files_to_add, fm_file_info_ref(file));
	if(job->idle_add_file_handler)
		g_source_remove(job->idle_add_file_handler);
	job->idle_add_file_handler = g_idle_add_full(G_PRIORITY_LOW, on_idle_add_files, job, NULL);
}

static gboolean run_rules_for_each_file_info(GFileInfo * info, GFile * parent, FmFileSearchJob * job)
{
	gboolean ret = FALSE;

	if(job->settings->show_hidden || !g_file_info_get_is_hidden(info))
	{
		const char * name = g_file_info_get_name(info);
		GFile * file = g_file_get_child(parent, name);
		FmFileSearchFuncData data = {0};
		GSList * rules = job->rules;

		data.current_file = file;
		data.current_file_info = info;
		data.settings = job->settings;
		data.job = job;

		while(rules != NULL && !fm_job_is_cancelled(FM_JOB(job)))
		{
			FmFileSearchRule * search_rule = rules->data;
			FmFileSearchFunc search_function = (*search_rule->function);
			ret = search_function(&data, search_rule->user_data);
			if(!ret)
				break;
			rules = rules->next;
		}

		/* recurse upon each directory */
		if(job->settings->recursive && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
			run_rules_for_each_target_folder(file,job);

		g_object_unref(file);
	}

	return ret;
}

FmFileInfoList * fm_file_search_job_get_files(FmFileSearchJob * job)
{
	return job->files;
}

/* functions for content search rule */

static gboolean strstr_nocase(char * haystack, char * needle)
{
	gboolean ret = FALSE;

	char * haystack_nocase = g_utf8_strdown(haystack, -1);
	char * needle_nocase = g_utf8_strdown(needle, -1);

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
		goffset size = 512 + sizeof(needle);
		goffset to_be_read = size;
		char buffer[size];
	
		goffset read = g_input_stream_read(G_INPUT_STREAM(io), &buffer, to_be_read, fm_job_get_cancellable(data->job), error);

		if(read == -1)
			action = fm_job_emit_error(FM_JOB(data->job), error, FM_JOB_ERROR_SEVERE);
		else
		{
			if(content_search(needle, buffer, data))
				ret = TRUE;
			else
			{
				while(read == to_be_read)
				{
					to_be_read = 512;

					int i;
					for(i = 0; i < sizeof(needle); i++)
					{
						buffer[i] = buffer[i+512];
					}

					read = g_input_stream_read(G_INPUT_STREAM(io), &buffer[sizeof(needle)], to_be_read, fm_job_get_cancellable(data->job), error);

					if(content_search(needle, buffer, data))
					{
						ret = TRUE;
						break;
					}
				}
			}
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

#if 0
/* NOTE: I disabled mmap-based search since this could cause
 * unexpected crashes sometimes if the mapped files are
 * removed or changed during the search. */
static gboolean file_content_search_mmap(char * needle, FmFileSearchFuncData * data)
{
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
#endif

static gboolean content_search(char* needle, char * haystack, FmFileSearchFuncData * data)
{
	gboolean ret = FALSE;

	if(fm_job_is_cancelled(FM_JOB(data->job)))
		return ret;

	else if(data->settings->content_mode == FM_FILE_SEARCH_MODE_REGEX)
	{
		if(data->job->target_contains_regex == NULL)
			data->job->target_contains_regex = g_regex_new(needle, (!data->settings->case_sensitive_content) ? G_REGEX_CASELESS : 0, 0, NULL);


		ret = g_regex_match(data->job->target_contains_regex, haystack, 0, NULL);
	}

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
		if(data->job->target_regex == NULL)
			data->job->target_regex = g_regex_new(user_data, (!data->settings->case_sensitive_target) ? G_REGEX_CASELESS : 0, 0, NULL);

		ret = g_regex_match(data->job->target_regex, display_name, 0, NULL);
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
		/* NOTE: I disabled mmap-based search since this could cause
		 * unexpected crashes sometimes if the mapped files are
		 * removed or changed during the search. */
/*
		if(g_file_is_native(data->current_file))
			ret = file_content_search_mmap(user_data, data);
		else
*/
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

	if(g_content_type_is_a(file_type, target_type))
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

gboolean fm_file_search_target_type_list_rule(FmFileSearchFuncData * data, gpointer user_data)
{
	gboolean ret = FALSE;

	char ** target_type_list = user_data;

	FmPath * path = fm_path_new_for_gfile(data->current_file);
	FmFileInfo * file_info = fm_file_info_new_from_gfileinfo(path, data->current_file_info);
	FmMimeType * file_mime = fm_file_info_get_mime_type(file_info);
	const char * file_type = fm_mime_type_get_type(file_mime);

	int i;
	for(i = 0; target_type_list[i]; ++i)
	{
		if(g_content_type_is_a(file_type, target_type_list[i]))
			ret = TRUE;		
	}

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

gboolean fm_file_search_target_type_generic_rule(FmFileSearchFuncData * data, gpointer user_data)
{
	gboolean ret = FALSE;

	char * target_type = user_data;

	FmPath * path = fm_path_new_for_gfile(data->current_file);
	FmFileInfo * file_info = fm_file_info_new_from_gfileinfo(path, data->current_file_info);
	FmMimeType * file_mime = fm_file_info_get_mime_type(file_info);
	const char * file_type = fm_mime_type_get_type(file_mime);

	if(g_str_has_prefix(file_type, target_type))
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
