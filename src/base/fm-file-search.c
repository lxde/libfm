/*
 * fm-file-search.c
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

#include "fm-file-search.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#define _GNU_SOURCE 1
#include <fnmatch.h>

static void fm_file_search_finalize(GObject * object);
static void fm_file_search_dispose(GObject * object);

G_DEFINE_TYPE(FmFileSearch, fm_file_search, FM_TYPE_FOLDER);

static void on_file_search_job_finished(FmFileSearchJob * job, FmFileSearch * search);
static void on_job_files_added(FmFileSearchJob * job, GSList * files_added, FmFileSearch * search);

static void fm_file_search_class_init(FmFileSearchClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_search_finalize;
	fm_file_search_parent_class = g_type_class_peek(FM_TYPE_FOLDER);
}

static void fm_file_search_init(FmFileSearch *search)
{
	FM_FOLDER(search)->files = fm_file_info_list_new();

	FM_FOLDER(search)->dir_path = NULL;

	search->rules = NULL;
	search->target_folders = NULL;
	search->settings = g_slice_new(FmFileSearchSettings);

	/*set up settings */
	search->settings->target_mode = FM_FILE_SEARCH_MODE_EXACT;
	search->settings->content_mode = FM_FILE_SEARCH_MODE_EXACT;
	search->settings->case_sensitive_target = FALSE;
	search->settings->case_sensitive_content = FALSE;
	search->settings->recursive = TRUE;
	search->settings->show_hidden = FALSE;
}

static void fm_file_search_finalize(GObject * object)
{
	FmFileSearch * search;
	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_FILE_SEARCH(object));
	search = FM_FILE_SEARCH(object);

	if (G_OBJECT_CLASS(fm_file_search_parent_class)->finalize)
        (* G_OBJECT_CLASS(fm_file_search_parent_class)->finalize)(object);
}

static void fm_file_search_dispose(GObject * object)
{
	FmFileSearch * search;
	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_FILE_SEARCH(object));

	search = FM_FILE_SEARCH(object);

	if(search->rules)
	{
		g_slist_free(search->rules);
		search->rules = NULL;
	}

	if(search->target_folders)
	{
		fm_list_unref(search->target_folders);
		search->target_folders = NULL;
	}

	if(search->search_job)
		fm_file_search_cancel(search);

	if(search->settings)
	{
		g_slice_free(FmFileSearchSettings, search->settings);
		search->settings = NULL;
	}

	if (G_OBJECT_CLASS(fm_file_search_parent_class)->dispose)
        (* G_OBJECT_CLASS(fm_file_search_parent_class)->dispose)(object);
}

void fm_file_search_add_search_func(FmFileSearch * search, FmFileSearchFunc * func, gpointer user_data)
{
	FmFileSearchRule * rule = g_slice_new(FmFileSearchRule);

	rule->function = func;
	rule->user_data = user_data;

	search->rules = g_slist_append(search->rules, rule);
}

FmFileSearch * fm_file_search_new(FmPathList * target_folders)
{
	FmFileSearch * file_search = FM_FILE_SEARCH(g_object_new(FM_TYPE_FILE_SEARCH, NULL));

	fm_file_search_set_target_folders(file_search, target_folders);

	return file_search;
}

void fm_file_search_run(FmFileSearch * search)
{
	/* reload */
	FmJob * file_search_job = fm_file_search_job_new(search->rules, search->target_folders, search->settings);
	g_signal_emit_by_name(search, "start-loading");
	g_signal_connect(file_search_job, "files-added", on_job_files_added, search);
	g_signal_connect(file_search_job, "finished", on_file_search_job_finished, search);
	fm_job_run_async(file_search_job);
	search->search_job = file_search_job;
}

void fm_file_search_cancel(FmFileSearch * search)
{
	if(FM_IS_FILE_SEARCH(search) && FM_IS_FILE_SEARCH_JOB(search->search_job))
	{
		g_signal_handlers_disconnect_by_func(search->search_job, on_job_files_added, search);
		g_signal_handlers_disconnect_by_func(search->search_job, on_file_search_job_finished, search);
		fm_job_cancel(search->search_job);
		search->search_job = NULL;
	}
}

/* Get/Set Methods */

FmFileSearchMode fm_file_search_get_target_mode(FmFileSearch * search)
{
	return search->settings->target_mode;
}

void fm_file_search_set_target_mode(FmFileSearch * search, FmFileSearchMode target_mode)
{
	search->settings->target_mode = target_mode;
}

FmFileSearchMode fm_file_search_get_content_mode(FmFileSearch * search)
{
	return search->settings->content_mode;
}

void fm_file_search_set_content_mode(FmFileSearch * search, FmFileSearchMode content_mode)
{
	search->settings->content_mode = content_mode;
}

FmPathList * fm_file_search_get_target_folders(FmFileSearch * search)
{
	return fm_list_ref(search->target_folders);
}

void fm_file_search_set_target_folders(FmFileSearch * search, FmPathList * target_folders)
{
	if(search->target_folders != NULL)
		fm_list_unref(search->target_folders);

	search->target_folders = fm_list_ref(target_folders);
}

gboolean fm_file_search_get_case_sensitive_target(FmFileSearch * search)
{
	return search->settings->case_sensitive_target;
}

void fm_file_search_set_case_sensitive_target(FmFileSearch * search, gboolean case_sensitive_target)
{
	search->settings->case_sensitive_target = case_sensitive_target;
}

gboolean fm_file_search_get_case_sensitive_content(FmFileSearch * search)
{
	return search->settings->case_sensitive_content;
}

void fm_file_search_set_case_sensitive_content(FmFileSearch * search, gboolean case_sensitive_content)
{
	search->settings->case_sensitive_content = case_sensitive_content;
}

gboolean fm_file_search_get_recursive(FmFileSearch * search)
{
	return search->settings->recursive;
}

void fm_file_search_set_recursive(FmFileSearch * search, gboolean recursive)
{
	search->settings->recursive = recursive;
}

gboolean fm_file_search_get_show_hidden(FmFileSearch * search)
{
	return search->settings->show_hidden;
}

void fm_file_search_set_show_hidden(FmFileSearch * search, gboolean show_hidden)
{
	search->settings->show_hidden = show_hidden;
}

/* utility functions */

static void on_file_search_job_finished(FmFileSearchJob * job, FmFileSearch * search)
{
	FmFolder * folder = FM_FOLDER(search);
	g_print("search-finished\n");
	folder->files = fm_file_search_job_get_files(job);
	g_signal_emit_by_name(folder, "finish-loading", folder, NULL);
	search->search_job = NULL;
}

static void on_job_files_added(FmFileSearchJob * job, GSList * files_added, FmFileSearch * search)
{
	FM_FOLDER(search)->files_to_add = g_slist_copy(files_added);
	g_signal_emit_by_name(FM_FOLDER(search), "files-added", FM_FOLDER(search)->files_to_add);
	g_slist_free(FM_FOLDER(search)->files_to_add);
	FM_FOLDER(search)->files_to_add = NULL;
}
