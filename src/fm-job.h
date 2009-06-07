/*
 *      fm-job.h
 *      
 *      Copyright 2009 PCMan <pcman@thinkpad>
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


#ifndef __FM_JOB_H__
#define __FM_JOB_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define FM_TYPE_JOB				(fm_job_get_type())
#define FM_JOB(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_TYPE_JOB, FmJob))
#define FM_JOB_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_TYPE_JOB, FmJobClass))
#define FM_IS_JOB(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_TYPE_JOB))
#define FM_IS_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_TYPE_JOB))

typedef struct _FmJob			FmJob;
typedef struct _FmJobClass		FmJobClass;

struct _FmJob
{
	GObject parent;

	gboolean cancel;
};

struct _FmJobClass
{
	GObjectClass parent_class;

	void (*finished)(FmJob* job);
	void (*error)(FmJob* job);
	void (*cancelled)(FmJob* job);

	gboolean (*run)(FmJob* job);
	void (*cancel)(FmJob* job);
};

GType	fm_job_get_type		(void);
FmJob*	fm_job_new			(void);

gboolean fm_job_run(FmJob* job);
void fm_job_cancel(FmJob* job);

/* private, should be called from working thread only */
void fm_job_finish(FmJob* job);

/* private */
void fm_job_emit_finished(FmJob* job);

/* private */
void fm_job_emit_cancelled(FmJob* job);

G_END_DECLS

#endif /* __FM-JOB_H__ */
