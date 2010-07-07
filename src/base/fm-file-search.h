#ifndef __FM_FILE_SEARCH_H__
#define __FM_FILE_SEARCH_H__

#include <glib-object.h>

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
typedef enum _FmFileSearchMode 	FmFileSearchMode;

enum _FmFileSearchMode
{
	FM_FILE_SEARCH_MODE_EXACT,
	FM_FILE_SEARCH_MODE_FUZZY,
	FM_FILE_SEARCH_MODE_REGEX
};

struct _FmFileSearch
{
	FmFolder parent;

	/* private */
	char * target;
	char * target_contains;
	FmFileSearchMode target_mode;
	FmFileSearchMode content_mode;
	FmPathList * target_folders;
	FmMimeType * target_type;
	gboolean case_sensitive;
	gboolean recursive;
	gboolean show_hidden;
	gboolean check_minimum_size;
	gboolean check_maximum_size;
	goffset minimum_size;
	goffset maximum_size;
};

struct _FmFileSearchClass
{
	FmFolderClass parent_class;
};

GType		fm_file_search_get_type		(void);
FmFileSearch * fm_file_search_new(char * target , char* target_contains, FmPathList * target_folders);
void fm_file_search_run(FmFileSearch * search);
char * fm_file_search_get_target(FmFileSearch * search);
void fm_file_search_set_target(FmFileSearch * search, char * target);
char * fm_file_search_get_target_contains(FmFileSearch * search);
void fm_file_search_set_target_contains(FmFileSearch * search, char * target_contains);
FmFileSearchMode fm_file_search_get_target_mode(FmFileSearch * search);
void fm_file_search_set_target_mode(FmFileSearch * search, FmFileSearchMode target_mode);
FmFileSearchMode fm_file_search_get_content_mode(FmFileSearch * search);
void fm_file_search_set_content_mode(FmFileSearch * search, FmFileSearchMode content_mode);
FmPathList * fm_file_search_get_target_folders(FmFileSearch * search);
void fm_file_search_set_target_folders(FmFileSearch * search, FmPathList * target_folders);
FmMimeType * fm_file_search_get_target_type(FmFileSearch * search);
void fm_file_search_set_target_type(FmFileSearch * search, FmMimeType * target_type);
gboolean fm_file_search_get_case_sensitive(FmFileSearch * search);
void fm_file_search_set_case_sensitive(FmFileSearch * search, gboolean case_sensitive);
gboolean fm_file_search_get_recursive(FmFileSearch * search);
void fm_file_search_set_recursive(FmFileSearch * search, gboolean recursive);
gboolean fm_file_search_get_show_hidden(FmFileSearch * search);
void fm_file_search_set_show_hidden(FmFileSearch * search, gboolean show_hidden);

G_END_DECLS

#endif
