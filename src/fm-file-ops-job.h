/*
 *      fm-file-ops-job.h
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


#ifndef __FM_FILE_OPS_JOB_H__
#define __FM_FILE_OPS_JOB_H__

#include "fm-job.h"
#include "fm-path.h"

G_BEGIN_DECLS

#define FM_FILE_OPS_JOB_TYPE				(fm_file_ops_job_get_type())
#define FM_FILE_OPS_JOB(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_FILE_OPS_JOB_TYPE, FmFileOpsJob))
#define FM_FILE_OPS_JOB_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_FILE_OPS_JOB_TYPE, FmFileOpsJobClass))
#define IS_FM_FILE_OPS_JOB(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_FILE_OPS_JOB_TYPE))
#define IS_FM_FILE_OPS_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_FILE_OPS_JOB_TYPE))

typedef struct _FmFileOpsJob			FmFileOpsJob;
typedef struct _FmFileOpsJobClass		FmFileOpsJobClass;

typedef enum _FmFileOpType FmFileOpType;
enum _FmFileOpType
{
	FM_FILE_OP_NONE,
	FM_FILE_OP_MOVE,
	FM_FILE_OP_COPY,
	FM_FILE_OP_TRASH,
	FM_FILE_OP_DELETE,
	FM_FILE_OP_CHMOD,
	FM_FILE_OP_CHOWN
};

struct _FmFileOpsJob
{
	FmJob parent;
	FmFileOpType type;
	FmPathList* srcs;
	FmPath* dest;

	FmJob* dc_job;
	GCancellable* cancellable;
	GIOSchedulerJob* io_job;
	goffset total;
	goffset finished;
	goffset rate;
	time_t started_time;
	time_t elapsed_time;
	time_t remaining_time;
};

struct _FmFileOpsJobClass
{
	FmJobClass parent_class;
	void (*do_operation)(FmFileOpsJob* job);
};

GType		fm_file_ops_job_get_type		(void);
FmJob*	fm_file_ops_job_new(FmFileOpType type, FmPathList* srcs, FmPath* dest);

G_END_DECLS

#endif /* __FM_FILE_OPS_JOB_H__ */
