/*
 *      fm-file-info-job.h
 *
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


#ifndef __FM_FILE_INFO_JOB_H__
#define __FM_FILE_INFO_JOB_H__

#include "fm-job.h"
#include "fm-file-info.h"

G_BEGIN_DECLS

#define FM_TYPE_FILE_INFO_JOB				(fm_file_info_job_get_type())
#define FM_FILE_INFO_JOB(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_TYPE_FILE_INFO_JOB, FmFileInfoJob))
#define FM_FILE_INFO_JOB_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_TYPE_FILE_INFO_JOB, FmFileInfoJobClass))
#define IS_FM_FILE_INFO_JOB(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_TYPE_FILE_INFO_JOB))
#define IS_FM_FILE_INFO_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_TYPE_FILE_INFO_JOB))

typedef struct _FmFileInfoJob			FmFileInfoJob;
typedef struct _FmFileInfoJobClass		FmFileInfoJobClass;

enum _FmFileInfoJobFlags
{
    FM_FILE_INFO_JOB_NONE = 0,
    FM_FILE_INFO_JOB_FOLLOW_SYMLINK = 1 << 0, /* FIXME: not yet implemented */
    FM_FILE_INFO_JOB_EMIT_FOR_EACH_FILE = 1 << 1 /* FIXME: not yet implemented */
};
typedef enum _FmFileInfoJobFlags        FmFileInfoJobFlags;

struct _FmFileInfoJob
{
	FmJob parent;
    FmFileInfoJobFlags flags;
	FmFileInfoList* file_infos;
    FmPath* current;
};

struct _FmFileInfoJobClass
{
	FmJobClass parent_class;
};

GType fm_file_info_job_get_type(void);
FmJob* fm_file_info_job_new(FmPathList* files_to_query, FmFileInfoJobFlags flags);

/* this can only be called before running the job. */
void fm_file_info_job_add(FmFileInfoJob* job, FmPath* path);
void fm_file_info_job_add_gfile(FmFileInfoJob* job, GFile* gf);

gboolean _fm_file_info_job_get_info_for_native_file(FmJob* job, FmFileInfo* fi, const char* path, GError** err);
gboolean _fm_file_info_job_get_info_for_gfile(FmJob* job, FmFileInfo* fi, GFile* gf, GError** err);

/* This API should only be called in error handler */
FmPath* fm_file_info_job_get_current(FmFileInfoJob* job);

G_END_DECLS

#endif /* __FM_FILE_INFO_JOB_H__ */
