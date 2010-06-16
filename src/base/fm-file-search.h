#ifndef __FM_FILE_SEARCH_H__
#define __FM_FILE_SEARCH_H__

#include <glib-object.h>

#include "fm-mime-type.h"

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
	GObject parent;

	/* private */
	char * target;
	GSList * target_folders;
	FmMimeType * target_type;
	gboolean check_type;
};

struct _FmFileSearchClass
{
	GObjectClass parent_class;
};


GType		fm_file_search_get_type		(void);

G_END_DECLS

#endif
