#ifndef __FM_FILE_SEARCH_H__
#define __FM_FILE_SEARCH_H__

#include <glib-object.h>

#include "fm-file-search-job.h"
#include "fm-mime-type.h"
#include "fm-folder.h"

G_BEGIN_DECLS

#define FM_TYPE_FILE_SEARCH			(fm_file_search_get_type())
#define FM_FILE_SEARCH(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_TYPE_FILE_SEARCH, FmFileSearch))
#define FM_FILE_SEARCH_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_TYPE_FILE_SEARCH, FmFileSearchClass))
#define FM_IS_FILE_SEARCH(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_TYPE_FILE_SEARCH))
#define FM_IS_FILE_SEARCH_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_TYPE_FILE_SEARCH))

typedef struct _FmFileSearch		FmFileSearch;
typedef struct _FmFileSearchClass	FmFileSearchClass;

struct _FmFileSearch
{
	FmFolder parent;
	GSList * rules;
	FmPathList * target_folders;
	FmFileSearchSettings * settings;
};

struct _FmFileSearchClass
{
	FmFolderClass parent_class;
};

GType		fm_file_search_get_type		(void);
void fm_file_search_add_search_func(FmFileSearch * search, FmFileSearchFunc * func, gpointer user_data);
FmFileSearch * fm_file_search_new(FmPathList * target_folders);
void fm_file_search_run(FmFileSearch * search);
FmFileSearchMode fm_file_search_get_target_mode(FmFileSearch * search);
void fm_file_search_set_target_mode(FmFileSearch * search, FmFileSearchMode target_mode);
FmFileSearchMode fm_file_search_get_content_mode(FmFileSearch * search);
void fm_file_search_set_content_mode(FmFileSearch * search, FmFileSearchMode content_mode);
FmPathList * fm_file_search_get_target_folders(FmFileSearch * search);
void fm_file_search_set_target_folders(FmFileSearch * search, FmPathList * target_folders);
gboolean fm_file_search_get_case_sensitive_target(FmFileSearch * search);
void fm_file_search_set_case_sensitive_target(FmFileSearch * search, gboolean case_sensitive);
gboolean fm_file_search_get_case_sensitive_content(FmFileSearch * search);
void fm_file_search_set_case_sensitive_content(FmFileSearch * search, gboolean case_sensitive);
gboolean fm_file_search_get_recursive(FmFileSearch * search);
void fm_file_search_set_recursive(FmFileSearch * search, gboolean recursive);
gboolean fm_file_search_get_show_hidden(FmFileSearch * search);
void fm_file_search_set_show_hidden(FmFileSearch * search, gboolean show_hidden);

G_END_DECLS

#endif
