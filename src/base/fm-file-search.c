#include "fm-file-search.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#define _GNU_SOURCE 1
#include <fnmatch.h>

static void fm_file_search_finalize(GObject * object);


G_DEFINE_TYPE(FmFileSearch, fm_file_search, FM_TYPE_FOLDER);

static void on_file_search_job_finished(FmFileSearchJob * job, FmFileSearch * search);

static gboolean file_content_search_ginputstream(FmFileSearchFuncData * data);
static gboolean file_content_search_mmap(FmFileSearchFuncData * data);
static gboolean content_search(char * haystack, FmFileSearchFuncData * data);

static void fm_file_search_class_init(FmFileSearchClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_search_finalize;
	fm_file_search_parent_class = g_type_class_peek(FM_TYPE_FOLDER);
}

static void fm_file_search_init(FmFileSearch *self)
{
	self->rules = NULL;
	self->target_folders = NULL;
	self->settings = g_slice_new(FmFileSearchSettings);

	/*set up settings */
	self->settings->target = NULL;
	self->settings->target_contains = NULL;
	self->settings->target_mode = FM_FILE_SEARCH_MODE_EXACT;
	self->settings->content_mode = FM_FILE_SEARCH_MODE_EXACT;
	self->settings->case_sensitive = FALSE;
	self->settings->recursive = TRUE;
	self->settings->show_hidden = FALSE;
}

static void fm_file_search_finalize(GObject * object)
{
	FmFileSearch * self;
	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_FILE_SEARCH(object));

	self = FM_FILE_SEARCH(object);

	if(self->rules)
		g_slist_free(self->rules);

	if(self->target_folders)
		fm_list_unref(self->target_folders);
	
	/* free things in settings */

	if(self->settings->target)
		g_free(self->settings->target);

	if(self->settings->target_contains)
		g_free(self->settings->target_contains);

	if(self->settings)
		g_slice_free(FmFileSearchSettings, self->settings);

	if (G_OBJECT_CLASS(fm_file_search_parent_class)->finalize)
        (* G_OBJECT_CLASS(fm_file_search_parent_class)->finalize)(object);
}

void fm_file_search_add_search_func(FmFileSearch * search, FmFileSearchFunc * func, gpointer user_data)
{
	FmFileSearchRule * rule = g_slice_new(FmFileSearchRule);

	rule->function = func;
	rule->user_data = user_data;

	search->rules = g_slist_append(search->rules, rule);
}

FmFileSearch * fm_file_search_new(char * target , char* target_contains, FmPathList * target_folders)
{
	FmFileSearch * file_search = FM_FILE_SEARCH(g_object_new(FM_TYPE_FILE_SEARCH, NULL));

	FmPath * path;

	FM_FOLDER(file_search)->dir_path = path;

	fm_file_search_set_target(file_search, target);
	fm_file_search_set_target_contains(file_search, target_contains);
	fm_file_search_set_target_folders(file_search, target_folders);

	return file_search;
}

void fm_file_search_run(FmFileSearch * search)
{
	FmJob * file_search_job = fm_file_search_job_new(search->rules, search->target_folders, search->settings);
	g_signal_connect(file_search_job, "finished", on_file_search_job_finished, search);
	fm_job_run_async(file_search_job);	
}

/* Get/Set Methods */

char * fm_file_search_get_target(FmFileSearch * search)
{
	return g_strdup(search->settings->target);
}

void fm_file_search_set_target(FmFileSearch * search, char * target)
{
	if(search->settings->target)
		g_free(search->settings->target);

	search->settings->target = g_strdup(target);
}

char * fm_file_search_get_target_contains(FmFileSearch * search)
{
	return g_strdup(search->settings->target_contains);
}

void fm_file_search_set_target_contains(FmFileSearch * search, char * target_contains)
{
	if(search->settings->target_contains)
		g_free(search->settings->target_contains);

	search->settings->target_contains = g_strdup(target_contains);
}

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

gboolean fm_file_search_get_case_sensitive(FmFileSearch * search)
{
	return search->settings->case_sensitive;
}

void fm_file_search_set_case_sensitive(FmFileSearch * search, gboolean case_sensitive)
{
	search->settings->case_sensitive = case_sensitive;
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

	folder->files = fm_file_search_job_get_files(job);

	g_signal_emit_by_name(folder, "loaded", folder, NULL);
}
