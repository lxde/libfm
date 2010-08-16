#ifndef __FM_FILE_SEARCH_JOB_H__
#define __FM_FILE_SEARCH_JOB_H__

#include "fm-job.h"
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


/* shared data types */

typedef enum _FmFileSearchMode 	FmFileSearchMode;
typedef struct _FmFileSearchRule FmFileSearchRule;
typedef struct _FmFileSearchFuncData FmFileSearchFuncData;
typedef struct _FmFileSearchSettings FmFileSearchSettings;
typedef struct _FmFileSearchModifiedTimeRuleData FmFileSearchModifiedTimeRuleData;
typedef gboolean (*FmFileSearchFunc)(FmFileSearchFuncData *, gpointer);

struct _FmFileSearchModifiedTimeRuleData
{
	int start_d;
	int start_m;
	int start_y;
	int end_d;
	int end_m;
	int end_y;
};

enum _FmFileSearchMode
{
	FM_FILE_SEARCH_MODE_EXACT,
	FM_FILE_SEARCH_MODE_REGEX
};

struct _FmFileSearchSettings
{
	FmFileSearchMode target_mode;
	FmFileSearchMode content_mode;
	gboolean case_sensitive_target;
	gboolean case_sensitive_content;
	gboolean recursive;
	gboolean show_hidden;
};

struct _FmFileSearchRule
{
	FmFileSearchFunc function;
	gpointer user_data;
};

struct _FmFileSearchFuncData
{
	GFile * current_file;
	GFileInfo * current_file_info;
	FmFileSearchSettings * settings;
	FmFileSearchJob * job;
};

/* end of shared data types */

struct _FmFileSearchJob
{
	FmJob parent;
	FmFileInfoList * files;
	GSList * rules;
	FmPathList * target_folders;
	FmFileSearchSettings * settings;
	GRegex * target_regex;
	GRegex * target_contains_regex;

	GSList * files_to_add;
};

struct _FmFileSearchJobClass
{
	FmJobClass parent_class;

    void (*files_added)(FmFileSearchJob *, GSList * files);
};

GType		fm_file_search_job_get_type		(void);
FmJob * fm_file_search_job_new(GSList * rules, FmPathList * target_folders, FmFileSearchSettings * settings);
FmFileInfoList * fm_file_search_job_get_files(FmFileSearchJob * job);

/* rules */
gboolean fm_file_search_target_rule(FmFileSearchFuncData * data, gpointer user_data);
gboolean fm_file_search_target_contains_rule(FmFileSearchFuncData * data, gpointer user_data);
gboolean fm_file_search_target_type_rule(FmFileSearchFuncData * data, gpointer user_data);
gboolean fm_file_search_target_type_list_rule(FmFileSearchFuncData * data, gpointer user_data);
gboolean fm_file_search_target_type_generic_rule(FmFileSearchFuncData * data, gpointer user_data);
gboolean fm_file_search_minimum_size_rule(FmFileSearchFuncData * data, gpointer user_data);
gboolean fm_file_search_maximum_size_rule(FmFileSearchFuncData * data, gpointer user_data);
gboolean fm_file_search_modified_time_rule(FmFileSearchFuncData * data, gpointer user_data);

G_END_DECLS

#endif
