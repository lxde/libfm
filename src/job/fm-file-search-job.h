#ifndef __FM_FILE_SEARCH_JOB_H__
#define __FM_FILE_SEARCH_JOB_H__

#include "fm-job.h"
#include "fm-file-search.h"
#include "fm-file-info.h"
#include "fm-mime-type.h"

G_BEGIN_DECLS

#define FM_TYPE_FILE_SEARCH_JOB				(fm_file_search_job_get_type())
#define FM_FILE_SEARCH_JOB(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_TYPE_FILE_SEARCH_JOB, FmFileSearchJob))
#define FM_FILE_SEARCH_JOB_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_TYPE_FILE_SEARCH_JOB, FmFileSearchJobClass))
#define FM_IS_FILE_SEARCH_JOB(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_TYPE_FILE_SEARCH_JOB))
#define FM_IS_FILE_SEARCH_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_TYPE_FILE_SEARCH_JOB))

typedef struct _FmFileSearchJob			FmFileSearchJob;
typedef struct _FmFileSearchJobClass		FmFileSearchJobClass;

struct _FmFileSearchJob
{
	FmJob parent;

	/* private */
	FmFileInfoList * files;
	char * target;
	char * target_contains;
	FmFileSearchMode target_mode;
	FmFileSearchMode content_mode;
	GRegex * target_regex;
	GRegex * target_contains_regex;
	GSList * target_folders;
	FmMimeType * target_type;
	gboolean case_sensitive;
	gboolean recursive;
	gboolean show_hidden;
	gboolean check_minimum_size;
	gboolean check_maximum_size;
	goffset minimum_size;
	goffset maximum_size;
};

struct _FmFileSearchJobClass
{
	FmJobClass parent_class;
};

GType		fm_file_search_job_get_type		(void);
FmJob * fm_file_search_job_new(FmFileSearch * search);
FmFileInfoList * fm_file_search_job_get_files(FmFileSearchJob * job);

G_END_DECLS

#endif
