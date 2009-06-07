/*
 *      fm-job.c
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

#include "fm-job.h"

enum {
	FINISHED,
	ERROR,
	CANCELLED,
	N_SIGNALS
};

static void fm_job_finalize  			(GObject *object);
G_DEFINE_TYPE(FmJob, fm_job, G_TYPE_OBJECT);

static gboolean on_idle_cleanup(gpointer unused);

static guint idle_handler = 0;
static GSList* finished = NULL;
G_LOCK_DEFINE_STATIC(idle_handler);

static signals[N_SIGNALS];

static void fm_job_class_init(FmJobClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = fm_job_finalize;

	fm_job_parent_class = (GObjectClass*)g_type_class_peek(G_TYPE_OBJECT);

    signals[FINISHED] =
        g_signal_new( "finished",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmJobClass, finished ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0 );
    signals[ERROR] =
        g_signal_new( "error",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmJobClass, error ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0 );
    signals[CANCELLED] =
        g_signal_new( "cancelled",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmJobClass, cancelled ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0 );
}


static void fm_job_init(FmJob *self)
{
	
}


FmJob* fm_job_new(void)
{
	return (FmJob*)g_object_new(FM_TYPE_JOB, NULL);
}


static void fm_job_finalize(GObject *object)
{
	FmJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_JOB(object));

	self = FM_JOB(object);

	if (G_OBJECT_CLASS(fm_job_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_job_parent_class)->finalize)(object);
}

gboolean fm_job_run(FmJob* job)
{
	FmJobClass* klass = (FmJobClass*)G_OBJECT_GET_CLASS(job);
	if(klass->run)
		return klass->run(job);
	return FALSE;
}

void fm_job_cancel(FmJob* job)
{
	FmJobClass* klass = (FmJobClass*)G_OBJECT_GET_CLASS(job);
	if(klass->cancel)
		return klass->cancel(job);
	job->cancel = TRUE;
}

/* private, should be called from working thread only */
void fm_job_finish(FmJob* job)
{
	G_LOCK(idle_handler);
	if(0 == idle_handler)
		idle_handler = g_idle_add(on_idle_cleanup, NULL);
	finished = g_slist_append(finished, job);
	G_UNLOCK(idle_handler);
}

void fm_job_emit_finished(FmJob* job)
{
	g_signal_emit(job, signals[FINISHED], 0);
}

void fm_job_emit_cancelled(FmJob* job)
{
	g_signal_emit(job, signals[CANCELLED], 0);
}


gboolean on_idle_cleanup(gpointer unused)
{
	GSList* jobs;
	GSList* l;

	G_LOCK(idle_handler);
	jobs = finished;
	finished = NULL;
	idle_handler = 0;
	G_UNLOCK(idle_handler);

	for(l = jobs; l; l=l->next)
	{
		FmJob* job = (FmJob*)l->data;
		/* FIXME: error handling? */
		if(job->cancel)
			fm_job_emit_cancelled(job);
		else
			fm_job_emit_finished(job);
		g_object_unref(job);
	}
	g_slist_free(jobs);
	return FALSE;
}
