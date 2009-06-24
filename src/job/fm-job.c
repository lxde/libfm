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
#include "fm-marshal.h"

enum {
	FINISHED,
	ERROR,
	CANCELLED,
	ASK,
	PROGRESS,
	N_SIGNALS
};

typedef struct _FmIdleCall
{
	FmJob* job;
	FmJobCallMainThreadFunc func;
	gpointer user_data;
	gpointer ret;
}FmIdleCall;

static void fm_job_finalize  			(GObject *object);
G_DEFINE_TYPE(FmJob, fm_job, G_TYPE_OBJECT);

static gboolean fm_job_real_run_async(FmJob* job);
static gboolean on_idle_cleanup(gpointer unused);
static void job_thread(FmJob* job, gpointer unused);

static guint idle_handler = 0;
static GSList* finished = NULL;
G_LOCK_DEFINE_STATIC(idle_handler);

static GThreadPool* thread_pool = NULL;
static guint n_jobs = 0;

static signals[N_SIGNALS];

static void fm_job_class_init(FmJobClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_job_finalize;

	klass->run_async = fm_job_real_run_async;

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
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET ( FmJobClass, error ),
                      NULL, NULL,
                      fm_marshal_BOOL__POINTER_BOOL,
                      G_TYPE_BOOLEAN, 2, G_TYPE_POINTER, G_TYPE_BOOLEAN );

    signals[CANCELLED] =
        g_signal_new( "cancelled",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmJobClass, cancelled ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0 );

    signals[ASK] =
        g_signal_new( "ask",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET ( FmJobClass, ask ),
                      NULL, NULL,
                      fm_marshal_INT__POINTER_INT,
                      G_TYPE_INT, 2, G_TYPE_POINTER, G_TYPE_INT );

}

static void fm_job_init(FmJob *self)
{
	/* create the thread pool if it doesn't exist. */
	if( G_UNLIKELY(!thread_pool) )
		thread_pool = g_thread_pool_new(job_thread, NULL, -1, FALSE, NULL);
	++n_jobs;
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

	if(self->cancellable)
		g_object_unref(self->cancellable);
		
	if(self->mutex)
		g_mutex_free(self->mutex);

	if(self->cond)
		g_cond_free(self->cond);

	if (G_OBJECT_CLASS(fm_job_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_job_parent_class)->finalize)(object);

	--n_jobs;
	if(0 == n_jobs)
	{
		g_thread_pool_free(thread_pool, TRUE, FALSE);
		thread_pool = NULL;
	}
}

static inline void init_mutex(FmJob* job)
{
	if(!job->mutex)
	{
		job->mutex = g_mutex_new();
		job->cond = g_cond_new();
	}
}

gboolean fm_job_real_run_async(FmJob* job)
{
	g_thread_pool_push(thread_pool, job, NULL);
	return TRUE;
}

gboolean fm_job_run_async(FmJob* job)
{
	FmJobClass* klass = FM_JOB_CLASS(G_OBJECT_GET_CLASS(job));
	return klass->run_async(job);
}

/* run a job in current thread in a blocking fashion.  */
gboolean fm_job_run_sync(FmJob* job)
{
	FmJobClass* klass = FM_JOB_CLASS(G_OBJECT_GET_CLASS(job));
	gboolean ret = klass->run(job);
	if(job->cancel)
		fm_job_emit_cancelled(job);
	else
		fm_job_emit_finished(job);
	return ret;
}

/* this is called from working thread */
void job_thread(FmJob* job, gpointer unused)
{
	FmJobClass* klass = FM_JOB_CLASS(G_OBJECT_GET_CLASS(job));
	klass->run(job);

	/* let the main thread know that we're done, and free the job
	 * in idle handler if neede. */
	fm_job_finish(job);
}

void fm_job_cancel(FmJob* job)
{
	FmJobClass* klass = FM_JOB_CLASS(G_OBJECT_GET_CLASS(job));
	job->cancel = TRUE;
	if(job->cancellable)
		g_cancellable_cancel(job->cancellable);
	/* FIXME: is this needed? */
	if(klass->cancel)
		klass->cancel(job);
}

static gboolean on_idle_call(FmIdleCall* data)
{
	data->ret = data->func(data->job, data->user_data);
	g_cond_broadcast(data->job->cond);
	return FALSE;
}

/* Following APIs are private to FmJob and should only be used in the
 * implementation of classes derived from FmJob.
 * Besides, they should be called from working thread only */
gpointer fm_job_call_main_thread(FmJob* job, 
				FmJobCallMainThreadFunc func, gpointer user_data)
{
	FmIdleCall data;
	init_mutex(job);
	data.job = job;
	data.func = func;
	data.user_data = user_data;
	g_idle_add( on_idle_call, &data );
	g_cond_wait(job->cond, job->mutex);
	return data.ret;
}

gpointer fm_job_call_main_thread_async(FmJob* job, GFunc func, gpointer user_data)
{
	/* FIXME: implement async calls to improve performance. */
}

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

struct AskData
{
	const char* question;
	gint options;
};

static gpointer ask_in_main_thread(FmJob* job, struct AskData* data)
{
	gint ret;
	g_signal_emit(job, signals[ASK], 0, data->question, data->options, &ret);
	return GINT_TO_POINTER(ret);
}

gint fm_job_ask(FmJob* job, const char* question, gint options)
{
	struct AskData data;
	data.question = question;
	data.options = options;
	return (gint)fm_job_call_main_thread(job, ask_in_main_thread, &data);
}

/* unref finished job objects in main thread on idle */
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
		if(job->cancel)
			fm_job_emit_cancelled(job);
		else
			fm_job_emit_finished(job);
		g_object_unref(job);
	}
	g_slist_free(jobs);
	return FALSE;
}

/* Used to implement FmJob::run() using gio inside.
 * This API tried to initialize a GCancellable object for use with gio.
 * If this function returns FALSE, that means the job is already 
 * cancelled before the cancellable object is created.
 * So the following I/O operations should be cancelled. */
gboolean fm_job_init_cancellable(FmJob* job)
{
	/* creating a cancellable object in working thread should be ok? */
	job->cancellable = g_cancellable_new();
	/* it's possible that the user calls fm_job_cancel before we
	 * create the cancellable object, so g_cancellable_is_cancelled 
	 * might return FALSE due to racing condition. */
	if( G_UNLIKELY(job->cancel) )
	{
		g_object_unref(job->cancellable);
		job->cancellable = NULL;
		return FALSE;
	}
	return TRUE;
}

struct ErrData
{
	GError* err;
	gboolean recoverable;
};

gpointer error_in_main_thread(FmJob* job, struct ErrData* data)
{
	gboolean ret;
	g_signal_emit(job, signals[ERROR], 0, data->err, data->recoverable, &ret);
	return GINT_TO_POINTER(ret);
}

/* Emit an error signal to notify the main thread when an error occurs.
 * The return value of this function is the return value returned by
 * the connected signal handlers.
 * If recoverable is TRUE, the listener of this 'error' signal can
 * return TRUE in signal handler to ask for retry of the failed operation.
 * If recoverable is FALSE, the return value of this function is 
 * always FALSE. If the error is fatal, the job might be aborted.
 * Otherwise, the listener of 'error' signal can optionally cancel
 * the job. */
gboolean fm_job_emit_error(FmJob* job, GError* err, gboolean recoverable)
{
	gboolean ret;
	struct ErrData data;
	data.err = err;
	data.recoverable = recoverable;
	ret = (gboolean)fm_job_call_main_thread(job, error_in_main_thread, &data);
	return recoverable ? ret : FALSE;
}
