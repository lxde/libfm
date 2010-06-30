#include <stdio.h>

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

int main(int argc, char** argv)
{
	g_type_init();
	fm_init(NULL);

	FmFileSearch * search;

	FmPath * path = fm_path_new(argv[1]);

	GSList * target_folders = g_slist_append(target_folders, path);

	search = fm_file_search_new(argv[2], target_folders, NULL, FALSE);

	FmFileInfoList * info_list = fm_folder_get_files(FM_FOLDER(search));

	fm_list_foreach(info_list, print_files, NULL);

	printf("%s\n", "File Search Complete");
	return 0;
}
