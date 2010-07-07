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

static GOptionEntry entries[] =
{
	{"target", 't', 0, G_OPTION_ARG_STRING, &target, "phrase to search for in file names", NULL },
	{"contains", 'c', 0, G_OPTION_ARG_STRING, &target_contains, "phrase to search for in file contents", NULL},
	{"paths", 'p', 0, G_OPTION_ARG_STRING, &path_list, "paths to search through i.e. /usr/share/local:/usr/share", NULL},
	{"type", 'y', 0, G_OPTION_ARG_STRING, &target_type, "system string representation of type of files to search", NULL},
	{"norecurse", 'r', 0, G_OPTION_ARG_NONE, &not_recursive, "disables recursively searching directories", NULL},
	{"showhidden", 's', 0, G_OPTION_ARG_NONE, &show_hidden, "enables searching hidden files", NULL},
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

	g_signal_connect(search, "loaded", on_search_loaded, loop);
	fm_file_search_run(search);

	g_main_run(loop);
	
	return 0;
}
