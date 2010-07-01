#include <stdio.h>
#include <gio/gio.h>

#include "fm-file-search.h"
#include "fm-folder.h"
#include "fm-file-info.h"
#include "fm-path.h"
#include "fm.h"

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

	FmFileSearch * search;

	GFile * path = g_file_new_for_path(argv[1]);

	GSList * target_folders = g_slist_append(target_folders, path);

	search = fm_file_search_new(argv[2], argv[3],target_folders, NULL);

	g_signal_connect(search, "loaded", on_search_loaded, loop);

	g_main_run(loop);
	
	return 0;
}
