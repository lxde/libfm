#include <stdio.h>
#include <gio/gio.h>
#include <glib.h>

#include "fm-file-search.h"
#include "fm-folder.h"
#include "fm-file-info.h"
#include "fm-path.h"
#include "fm.h"

#include <string.h>

static char * target = NULL;
static char * target_contains = NULL;
static char * path_list = NULL;
static char * target_type = NULL;
static gboolean not_recursive = FALSE;
static gboolean show_hidden = FALSE;
static gboolean regex_target = FALSE;
static gboolean regex_content = FALSE;
static gboolean exact_target = FALSE;
static gboolean exact_content = FALSE;
static char * type = NULL;
static gboolean case_sensitive = FALSE;
static gint64 minimum_size = -1;
static gint64 maximum_size = -1;

static GOptionEntry entries[] =
{
	{"target", 't', 0, G_OPTION_ARG_STRING, &target, "phrase to search for in file names", NULL },
	{"contains", 'c', 0, G_OPTION_ARG_STRING, &target_contains, "phrase to search for in file contents", NULL},
	{"paths", 'p', 0, G_OPTION_ARG_STRING, &path_list, "paths to search through i.e. /usr/share/local:/usr/share", NULL},
	{"type", 'y', 0, G_OPTION_ARG_STRING, &target_type, "system string representation of type of files to search", NULL},
	{"norecurse", 'r', 0, G_OPTION_ARG_NONE, &not_recursive, "disables recursively searching directories", NULL},
	{"showhidden", 's', 0, G_OPTION_ARG_NONE, &show_hidden, "enables searching hidden files", NULL},
	{"regextarget", 'e', 0, G_OPTION_ARG_NONE, &regex_target, "enables regex target searching", NULL},
	{"regexcontent", 'g', 0, G_OPTION_ARG_NONE, &regex_content, "enables regex target searching", NULL},
	{"exacttarget", 'x', 0, G_OPTION_ARG_NONE, &exact_target, "enables regex target searching", NULL},
	{"exactcontent", 'a', 0, G_OPTION_ARG_NONE, &exact_content, "enables regex target searching", NULL},
	{"type", 'z', 0, G_OPTION_ARG_STRING, &type, "type of file to search for", NULL},
	{"casesensitive", 'n', 0, G_OPTION_ARG_NONE, &case_sensitive, "enables case sensitive searching", NULL},
	{"minimumsize", 'u', 0,G_OPTION_ARG_INT64, &minimum_size, "minimum size of file that is a match", NULL},
	{"maximumsize", 'w', 0, G_OPTION_ARG_INT64, &maximum_size, "maximum size of file taht is a match", NULL},
	{NULL}
};

static void print_files(gpointer data, gpointer user_data)
{
	FmFileInfo * info = FM_FILE_INFO(data);
	printf("%s\n", fm_file_info_get_disp_name(info));
}

static void on_search_loaded(gpointer data, gpointer user_data)
{
	FmFileInfoList * info_list = fm_folder_get_files(FM_FOLDER(data));
	fm_list_foreach(info_list, print_files, NULL);
	g_main_loop_quit((GMainLoop*)user_data);
}

int main(int argc, char** argv)
{
	g_type_init();
	fm_init(NULL);

	GMainLoop * loop = g_main_loop_new(NULL, FALSE);
	
	GOptionContext * context;

	context = g_option_context_new(" - test for libfm file search");
	g_option_context_add_main_entries(context, entries, NULL);
	g_option_context_parse(context, &argc, &argv, NULL);
	
	FmFileSearch * search;

	char * path_list_token = strtok(path_list, ":");
	FmPathList * target_folders = fm_path_list_new();

	while(path_list_token != NULL)
	{
		FmPath * path = fm_path_new(path_list_token);
		fm_list_push_tail(target_folders, path);
		path_list_token = strtok(NULL, ":");
	}
	search = fm_file_search_new(target, target_contains, target_folders);

	fm_file_search_set_show_hidden(search, show_hidden);
	fm_file_search_set_recursive(search, !not_recursive);

	if(regex_target)
		fm_file_search_set_target_mode(search, FM_FILE_SEARCH_MODE_REGEX);
	else if(exact_target)
		fm_file_search_set_target_mode(search, FM_FILE_SEARCH_MODE_EXACT);

	if(regex_content)
		fm_file_search_set_content_mode(search, FM_FILE_SEARCH_MODE_REGEX);
	else if(exact_content)
		fm_file_search_set_content_mode(search, FM_FILE_SEARCH_MODE_EXACT);

	if(case_sensitive)
		fm_file_search_set_case_sensitive(search, TRUE);

	if(minimum_size >= 0)
	{
		fm_file_search_set_check_minimum_size(search, TRUE);
		fm_file_search_set_minimum_size(search, minimum_size);
	}

	if(maximum_size >= 0)
	{
		fm_file_search_set_check_maximum_size(search, TRUE);
		fm_file_search_set_maximum_size(search, maximum_size);
	}

	if(type != NULL)
	{
		FmMimeType * mime = fm_mime_type_get_for_type(type);
		fm_file_search_set_target_type(search, mime);
	}

	g_signal_connect(search, "loaded", on_search_loaded, loop);
	fm_file_search_run(search);

	g_main_run(loop);
	
	return 0;
}