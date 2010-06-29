#include <stdio.h>

#include "fm-file-search.h"
#include "fm-folder.h"
#include "fm-file-info.h"
#include "fm-path.h"
#include "fm.h"

static void on_search_loaded()
{
	//FmFileInfoList * list = 

	printf("%s\n", "File Search Complete");
}

int main()
{
	g_type_init();
	fm_init(NULL);

	FmFileSearch * search;

	FmPath * path = fm_path_new("/home/shae");

	GSList * target_folders = g_slist_append(target_folders, path);

	search = fm_file_search_new("demo", target_folders, NULL, FALSE);

	

	//g_signal_connect(FM_FOLDER(search), "loaded", on_search_loaded, NULL);

	return 0;
}
