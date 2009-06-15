/*
 *      fm-deep-count-job.h
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


#ifndef __FM_DEEP_COUNT_JOB_H__
#define __FM_DEEP_COUNT_JOB_H__

#include "fm-job.h"
#include "fm-path.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define FM_DEEP_COUNT_JOB_TYPE				(fm_deep_count_job_get_type())
#define FM_DEEP_COUNT_JOB(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_DEEP_COUNT_JOB_TYPE, FmDeepCountJob))
#define FM_DEEP_COUNT_JOB_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_DEEP_COUNT_JOB_TYPE, FmDeepCountJobClass))
#define IS_FM_DEEP_COUNT_JOB(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_DEEP_COUNT_JOB_TYPE))
#define IS_FM_DEEP_COUNT_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_DEEP_COUNT_JOB_TYPE))

typedef struct _FmDeepCountJob			FmDeepCountJob;
typedef struct _FmDeepCountJobClass		FmDeepCountJobClass;

struct _FmDeepCountJob
{
	FmJob parent;
	GCancellable* cancellable;
	GIOSchedulerJob* io_job;
	FmPathList* paths;
	goffset total_size;
	goffset total_block_size;
	guint count;
};

struct _FmDeepCountJobClass
{
	FmJobClass parent_class;
};

GType		fm_deep_count_job_get_type(void);
FmJob*	fm_deep_count_job_new(FmPathList* paths);

G_END_DECLS

#endif /* __FM_DEEP_COUNT_JOB_H__ */
