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

struct _FmFileSearch
{
	FmFolder parent;

	/* private */
	char * target;
	char * target_contains;
	GSList * target_folders;
	gboolean check_type;
	FmMimeType * target_type;
	
};

struct _FmFileSearchClass
{
	FmFolderClass parent_class;
};


GType		fm_file_search_get_type		(void);

/* 
target: the target file name, if NULL it is not used to determine matches
target_contains: the target file needs to contain the following string, if NULL it is not used to determine matches, does not work on all types
target_folders: should be a GSList of GFiles representing directories that are to be searched
target_type: target mime type, if NULL it is not used to determine matches
*/
FmFileSearch * fm_file_search_new(char * target , char* target_contains, GSList * target_folders, FmMimeType * target_type);

G_END_DECLS

#endif
