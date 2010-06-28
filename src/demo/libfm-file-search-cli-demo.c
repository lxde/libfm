#include <stdio.h>

#include "fm-file-search.h"
#include "fm-folder.h"
#include "fm-file-info.h"
#include "fm-path.h"

static void on_search_loaded(FmFolder * folder, gpointer user_data)
{
	//FmFileInfoList * list = 
}

int main()
{
	g_type_init();

	FmFileSearch * search;

	GSList * target_folders = g_slist_append(target_folders, fm_path_get_home());

	search = fm_file_search_new("demo", target_folders, NULL, FALSE);

	g_signal_connect(FM_FOLDER(search), "loaded", on_search_loaded, NULL);

	printf("%s\n", "File Search Complete");
	return 0;
}
