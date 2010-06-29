#include "fm-file-search.h"

#include "fm-folder.h"
#include "fm-file-search-job.h"

static void fm_file_search_finalize(GObject * object);

G_DEFINE_TYPE(FmFileSearch, fm_file_search, FM_TYPE_FOLDER);

static void on_file_search_job_finished(FmFileSearchJob * job, FmFileSearch * search);

static void fm_file_search_class_init(FmFileSearchClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_search_finalize;
	fm_file_search_parent_class = g_type_class_peek(FM_TYPE_FOLDER);

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

FmFileSearch * fm_file_search_new(char * target, GSList * target_folders, FmMimeType * target_type, gboolean check_type)
{
	FmFileSearch * file_search = FM_FILE_SEARCH(g_object_new(FM_TYPE_FILE_SEARCH, NULL));

	file_search->target = g_strdup(target);
	file_search->target_folders = g_slist_copy(target_folders);
	file_search->target_type = target_type; /* does this need to be referenced */
	file_search->check_type = check_type;

	FmJob * file_search_job = fm_file_search_job_new(file_search);

	g_signal_connect(file_search_job, "finished", on_file_search_job_finished, file_search);

	//fm_job_run_async(file_search_job);
	fm_job_run_sync(file_search_job);
}

static void on_file_search_job_finished(FmFileSearchJob * job, FmFileSearch * search)
{
	FmFolder * folder = FM_FOLDER(search);

	folder->files = fm_file_search_job_get_files(job);

	g_signal_emit_by_name(folder, "loaded", folder, NULL);
}
