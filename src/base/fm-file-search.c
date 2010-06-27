#include "fm-file-search.h"

#include "fm-folder.h"

static void fm_file_search_finalize(GObject * object);

G_DEFINE_TYPE(FmFileSearch, fm_file_search, FM_TYPE_FOLDER);

static void fm_file_search_class_init(FmFileSearchClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_search_finalize;
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

static void fm_file_search_finalize(GObject * object)
{
	FmFileSearch * self;
	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_FILE_SEARCH(object));

	self = FM_FILE_SEARCH(object);

	if(self->target)
		g_free(self->target);
		
	if(self->target_folders)
		g_slist_free(self->target_folders);

	if(self->target_type)
		fm_mime_type_unref(self->target_type);

	if (G_OBJECT_CLASS(fm_file_search_parent_class)->finalize)
        (* G_OBJECT_CLASS(fm_file_search_parent_class)->finalize)(object);
}

FmFileSearch * fm_file_search_new_internal()
{
	FmFileSearch * file_search = FM_FILE_SEARCH(g_object_new(FM_TYPE_FILE_SEARCH, NULL));
}
