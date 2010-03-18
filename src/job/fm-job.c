/*
 *      fm-job.c
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

#include "fm-job.h"
#include "fm-marshal.h"

/**
 * SECTION:fmjob
 * @short_description: Base class of all kinds of asynchronous jobs.
 * @include: libfm/fm.h
 *
 * FmJob can be used to create asynchronous jobs performing some
 * time-consuming tasks in another worker thread.
 * To run a FmJob in another thread you simply call
 * fm_job_run_async(), and then the task will be done in another
 * worker thread. Later, when the job is finished, "finished" signal is
 * emitted. When the job is still running, it's possible to cancel it
 * from main thread by calling fm_job_cancel(). Then, "cancelled" signal
 * will be emitted before emitting "finished" signal. You can also run
 * the job in blocking fashion instead of running it asynchronously by
 * calling fm_job_run_sync().
 */

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
/*
static gboolean fm_job_error_accumulator(GSignalInvocationHint *ihint, GValue *return_accu,
                                           const GValue *handler_return, gpointer data);
*/
static void on_cancellable_cancelled(GCancellable* cancellable, FmJob* job);

G_DEFINE_ABSTRACT_TYPE(FmJob, fm_job, G_TYPE_OBJECT);

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

    /* "finished" signsl is emitted when the job is finished. This signal
     * is not emitted on a cancelled job. */
    signals[FINISHED] =
        g_signal_new( "finished",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmJobClass, finished ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0 );

    /* "error" signsl is emitted when errors happen. */
    signals[ERROR] =
        g_signal_new( "error",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET ( FmJobClass, error ),
                      NULL /*fm_job_error_accumulator*/, NULL,
                      fm_marshal_INT__POINTER_INT,
                      G_TYPE_INT, 2, G_TYPE_POINTER, G_TYPE_INT );

    /* "cancelled" signsl is emitted when the job is cancelled or aborted
     * due to critical errors. For a cancelled job, "finished" signal
     * is not emitted. */
    signals[CANCELLED] =
        g_signal_new( "cancelled",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmJobClass, cancelled ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0 );

    /* "ask" signal is emitted when the job asked for user interactions.
     * The user then will have a list of available options. */
    signals[ASK] =
        g_signal_new( "ask",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET ( FmJobClass, ask ),
                      NULL, NULL,
                      fm_marshal_INT__POINTER_POINTER,
                      G_TYPE_INT, 2, G_TYPE_POINTER, G_TYPE_POINTER );
}

static void fm_job_init(FmJob *self)
{
	/* create the thread pool if it doesn't exist. */
	if( G_UNLIKELY(!thread_pool) )
		thread_pool = g_thread_pool_new((GFunc)job_thread, NULL, -1, FALSE, NULL);
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
    {
        /* FIXME: should we use new API provided in glib 2.22 for this? */
        g_signal_handlers_disconnect_by_func(self->cancellable, on_cancellable_cancelled, self);
		g_object_unref(self->cancellable);
    }

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
	gboolean ret;
    job->running = TRUE;
    ret = klass->run_async(job);
    return ret;
}

/* run a job in current thread in a blocking fashion.  */
gboolean fm_job_run_sync(FmJob* job)
{
	FmJobClass* klass = FM_JOB_CLASS(G_OBJECT_GET_CLASS(job));
	gboolean ret;
    job->running = TRUE;
    ret = klass->run(job);
    job->running = FALSE;
	if(job->cancel)
		fm_job_emit_cancelled(job);
	else
		fm_job_emit_finished(job);
	return ret;
}

static void on_sync_job_finished(FmJob* job, GMainLoop* mainloop)
{
    g_main_loop_quit(mainloop);
    job->running = FALSE;
}

/* Run a job in current thread in a blocking fashion and an additional 
 * mainloop being created to prevent blocking of user interface.
 * A job running synchronously with this function should be unrefed
 * later with g_object_unref when no longer needed. */
gboolean fm_job_run_sync_with_mainloop(FmJob* job)
{
    GMainLoop* mainloop = g_main_loop_new(NULL, FALSE);
    gboolean ret;
    g_object_ref(job); /* acquire a ref because later it will be unrefed in idle handler. */
    g_signal_connect(job, "finished", G_CALLBACK(on_sync_job_finished), mainloop);
    ret = fm_job_run_async(job);
    if(ret)
    {
        g_main_loop_run(mainloop);
        g_signal_handlers_disconnect_by_func(job, on_sync_job_finished, mainloop);
    }
    else
        g_object_unref(job);
    g_main_loop_unref(mainloop);
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

/* cancel the job */
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
	g_mutex_lock(data->job->mutex);
	g_cond_broadcast(data->job->cond);
	g_mutex_unlock(data->job->mutex);
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
	g_mutex_lock(job->mutex);
	g_idle_add( (GSourceFunc)on_idle_call, &data );
	g_cond_wait(job->cond, job->mutex);
	g_mutex_unlock(job->mutex);
	return data.ret;
}

void fm_job_finish(FmJob* job)
{
	G_LOCK(idle_handler);
	if(0 == idle_handler)
		idle_handler = g_idle_add(on_idle_cleanup, NULL);
	finished = g_slist_append(finished, job);
    job->running = FALSE;
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
	const char** options;
};

static gpointer ask_in_main_thread(FmJob* job, struct AskData* data)
{
	gint ret;
	g_signal_emit(job, signals[ASK], 0, data->question, data->options, &ret);
	return GINT_TO_POINTER(ret);
}

gint fm_job_ask(FmJob* job, const char* question, ...)
{
    gint ret;
    va_list args;
    va_start (args, question);
    ret = fm_job_ask_valist(job, question, args);
    va_end (args);
    return ret;
}

gint fm_job_askv(FmJob* job, const char* question, const char** options)
{
	struct AskData data;
	data.question = question;
	data.options = options;
	return (gint)fm_job_call_main_thread(job, ask_in_main_thread, &data);
}

gint fm_job_ask_valist(FmJob* job, const char* question, va_list options)
{
    GArray* opts = g_array_sized_new(TRUE, TRUE, sizeof(char*), 6);
    gint ret;
    const char* opt = va_arg(options, const char*);
    while(opt)
    {
        g_array_append_val(opts, opt);
        opt = va_arg (options, const char *);
    }
    ret = fm_job_askv(job, question, opts->data);
    g_array_free(opts, TRUE);
    return ret;
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
        fm_job_emit_finished(job);
		g_object_unref(job);
	}
	g_slist_free(jobs);
	return FALSE;
}

/* Used by derived classes to implement FmJob::run() using gio inside.
 * This API tried to initialize a GCancellable object for use with gio and
 * should only be called once in the constructor of derived classes which
 * require the use of GCancellable. */
void fm_job_init_cancellable(FmJob* job)
{
    job->cancellable = g_cancellable_new();
    g_signal_connect(job->cancellable, "cancelled", G_CALLBACK(on_cancellable_cancelled), job);
}

/* Used to implement FmJob::run() using gio inside.
 * This API tried to initialize a GCancellable object for use with gio. */
GCancellable* fm_job_get_cancellable(FmJob* job)
{
	return job->cancellable;
}

/* Let the job use an existing cancellable object.
 * This can be used when you wish to share a cancellable object
 * among different jobs.
 * This should only be called before the job is launched. */
void fm_job_set_cancellable(FmJob* job, GCancellable* cancellable)
{
    if(G_UNLIKELY(job->cancellable))
    {
        g_signal_handlers_disconnect_by_func(job->cancellable, on_cancellable_cancelled, job);
        g_object_unref(job->cancellable);
    }
    if(G_LIKELY(cancellable))
    {
        job->cancellable = (GCancellable*)g_object_ref(cancellable);
        g_signal_connect(job->cancellable, "cancelled", G_CALLBACK(on_cancellable_cancelled), job);
    }
    else
        job->cancellable = NULL;
}

void on_cancellable_cancelled(GCancellable* cancellable, FmJob* job)
{
    job->cancel = TRUE;
}

struct ErrData
{
	GError* err;
	FmJobErrorSeverity severity;
};

gpointer error_in_main_thread(FmJob* job, struct ErrData* data)
{
	gboolean ret;
    g_debug("FmJob error: %s", data->err->message);
	g_signal_emit(job, signals[ERROR], 0, data->err, data->severity, &ret);
	return GINT_TO_POINTER(ret);
}

/* Emit an 'error' signal to notify the main thread when an error occurs.
 * The return value of this function is the return value returned by
 * the connected signal handlers.
 * If severity is FM_JOB_ERROR_CRITICAL, the returned value is ignored and
 * fm_job_cancel() is called to abort the job. Otherwise, the signal
 * handler of this error can return FM_JOB_RETRY to ask for retrying the
 * failed operation, return FM_JOB_CONTINUE to ignore the error and
 * continue the remaining job, or return FM_JOB_ABORT to abort the job.
 * If FM_JOB_ABORT is returned by the signal handler, fm_job_cancel
 * will be called in fm_job_emit_error().
 */
FmJobErrorAction fm_job_emit_error(FmJob* job, GError* err, FmJobErrorSeverity severity)
{
	gboolean ret;
	struct ErrData data;
	data.err = err;
	data.severity = severity;
	ret = (gboolean)fm_job_call_main_thread(job, error_in_main_thread, &data);
    if(severity == FM_JOB_ERROR_CRITICAL || ret == FM_JOB_ABORT)
    {
        ret = FM_JOB_ABORT;
        fm_job_cancel(job);
        /* FIXME: do we need fm_job_is_aborted()? */
    }

    /* If the job is already cancelled, retry is not allowed. */
    if(ret == FM_JOB_RETRY )
    {
        if(job->cancel || (err->domain == G_IO_ERROR && err->code == G_IO_ERROR_CANCELLED))
            ret = FM_JOB_CONTINUE;
    }

	return ret;
}

/* FIXME: need to re-think how to do this in a correct way. */
/*
gboolean fm_job_error_accumulator(GSignalInvocationHint *ihint, GValue *return_accu,
                                   const GValue *handler_return, gpointer data)
{
    int val = g_value_get_int(handler_return);
    g_debug("accumulate: %d, %d", g_value_get_int(return_accu), val);
    g_value_set_int(return_accu, val);
    return val != FM_JOB_CONTINUE;
}
*/

/* return TRUE if the job is already cancelled */
gboolean fm_job_is_cancelled(FmJob* job)
{
    return job->cancel;
}

/* return TRUE if the job is still running */
gboolean fm_job_is_running(FmJob* job)
{
    return job->running;
}
