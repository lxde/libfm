#include "fm-file-search.h"
#include "fm-folder.h"

G_DEFINE_TYPE(FmFileSearch, fm_file_search, FM_TYPE_FOLDER);

static void fm_file_search_class_init(FmFileSearchClass *klass)
{
	GObjectClass *g_object_class;
    g_object_class = G_OBJECT_CLASS(klass);
	/* TODO: Override the finalize function here */
    /* g_object_class->finalize = fm_file_search_finalize; */
    fm_file_search_parent_class = (GObjectClass*)g_type_class_peek(FM_TYPE_FOLDER);

	/* TODO: Set up signals here if needed */
}

static void fm_file_search_init(FmFileSearch *self)
{
	self->target = NULL;
	self->target_folders = NULL;
	self->target_type = NULL;
	self->check_type = FALSE;
}

FmFileSearch * fm_file_search_new_internal()
{
	FmFileSearch * file_search = FM_FILE_SEARCH(g_object_new(FM_TYPE_FILE_SEARCH, NULL));
	FmFolder * folder = FM_FOLDER(file_search);
}


/* util functions */

/* checks to see if the file "file" is a match to "target" and "type" */
static gboolean check_file_match(FmFileInfo * file, char * target, FmMimeType * type)
{
	if(g_strcmp0(fm_mime_type_get_type(fm_file_info_get_mime_type(file)),fm_mime_type_get_type(type)) == 0)
	{
		if(g_ascii_strcasecmp(fm_file_info_get_disp_name(file),target) == 0)
			return TRUE;

		//TODO: check contents of files of plain text file types
	}

	return FALSE;
}

static void search_folder_helper_foreach()
{

}

/* recursively searches a folder using check_file_match */
static void search_folder_helper(FmFileSearch * search, FmFolder * folder)
{
	FmFileInfoList * files = fm_folder_get_files(folder);

	if(files != NULL)
	{
		//fm_list_foreach();
	}
}

static void search_folder(FmFileSearch * search)
{

}
