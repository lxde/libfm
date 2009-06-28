/*
 *      fm-file-ops-job.c
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

#include "fm-file-ops-job.h"
#include "fm-file-ops-job-xfer.h"
#include "fm-file-ops-job-delete.h"

enum
{
	CUR_FILE,
	PERCENT,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

static void fm_file_ops_job_finalize  			(GObject *object);

static gboolean fm_file_ops_job_run(FmJob* fm_job);

/* funcs for io jobs */
static gboolean copy_files(FmFileOpsJob* job);
static gboolean move_files(FmFileOpsJob* job);
static gboolean trash_files(FmFileOpsJob* job);
static gboolean delete_files(FmFileOpsJob* job);
static gboolean chmod_files(FmFileOpsJob* job);
static gboolean chown_files(FmFileOpsJob* job);


G_DEFINE_TYPE(FmFileOpsJob, fm_file_ops_job, FM_TYPE_JOB);

static void fm_file_ops_job_class_init(FmFileOpsJobClass *klass)
{
	GObjectClass *g_object_class;
	FmJobClass* job_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_ops_job_finalize;

	job_class = FM_JOB_CLASS(klass);
	job_class->run = fm_file_ops_job_run;
	job_class->finished = NULL;

    signals[CUR_FILE] =
        g_signal_new( "cur-file",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, cur_file ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[PERCENT] =
        g_signal_new( "percent",
                      G_TYPE_FROM_CLASS ( klass ),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET ( FmFileOpsJobClass, percent ),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE, 1, G_TYPE_UINT );
}


static void fm_file_ops_job_finalize(GObject *object)
{
	FmFileOpsJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_FILE_OPS_JOB(object));

	self = FM_FILE_OPS_JOB(object);

	if(self->srcs);
		fm_list_unref(self->srcs);
	if(self->dest)
		fm_path_unref(self->dest);

	G_OBJECT_CLASS(fm_file_ops_job_parent_class)->finalize(object);
}


static void fm_file_ops_job_init(FmFileOpsJob *self)
{
	
}


FmJob *fm_file_ops_job_new(FmFileOpType type, FmPathList* files)
{
	FmFileOpsJob* job = (FmFileOpsJob*)g_object_new(FM_FILE_OPS_JOB_TYPE, NULL);
	job->srcs = fm_list_ref(files);
	job->type = type;
	return (FmJob*)job;
}

gboolean fm_file_ops_job_run(FmJob* fm_job)
{
	FmFileOpsJob* job = (FmFileOpsJob*)fm_job;
	FmPath* tmp;

	if(!fm_job_init_cancellable(fm_job))
		return FALSE;

	switch(job->type)
	{
	case FM_FILE_OP_COPY:
		copy_files(job);
		break;
	case FM_FILE_OP_MOVE:
		move_files(job);
		break;
	case FM_FILE_OP_TRASH:
		trash_files(job);
		break;
	case FM_FILE_OP_DELETE:
		delete_files(job);
		break;
	case FM_FILE_OP_CHMOD:
		chmod_files(job);
		break;
	case FM_FILE_OP_CHOWN:
		chown_files(job);
		break;
	}
	return TRUE;
}


void fm_file_ops_job_set_dest(FmFileOpsJob* job, FmPath* dest)
{
	job->dest = fm_path_ref(dest);
}

static gboolean on_cancelled(FmFileOpsJob* job)
{
	fm_job_emit_cancelled((FmJob*)job);
	return FALSE;
}

gboolean copy_files(FmFileOpsJob* job)
{
	GList* l;
	/* prepare the job, count total work needed with FmDeepCountJob */
	FmDeepCountJob* dc = fm_deep_count_job_new(job->srcs, FM_DC_JOB_DEFAULT);
	fm_job_run_sync(dc);
	job->total = dc->total_size;
	g_object_unref(dc);
	g_debug("total size to copy: %llu", job->total);

	for(l = fm_list_peek_head_link(job->srcs); l; l=l->next)
	{
		FmPath* path = (FmPath*)l->data;
		FmPath* _dest = fm_path_new_child(job->dest, path->name);
		GFile* src = fm_path_to_gfile(path);
		GFile* dest = fm_path_to_gfile(_dest);
		fm_path_unref(_dest);
		if(!fm_file_ops_job_copy_file(job, src, NULL, dest))
			return FALSE;
	}
	return TRUE;
}

gboolean move_files(FmFileOpsJob* job)
{
#if 0
	GFile *dest;
	GFileInfo* inf;
	GList* l;
	GError* err = NULL;
	guint32 dest_dev = 0;

	g_return_val_if_fail(job->dest, FALSE);

	dest = fm_path_to_gfile(job->dest);
	inf = g_file_query_info(dest, G_FILE_ATTRIBUTE_UNIX_DEVICE, 0, cancellable, &err);
	if(inf)
	{
		dest_dev = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_DEVICE);
		g_object_ref(inf);
	}
	else
	{
		/* FIXME: error */
		g_object_unref(dest);
		return FALSE;
	}	

	if( job->cancel )
		return FALSE;

	l = fm_list_peek_head_link(job->srcs);
	for(;l;l=l->next)
	{
		GFile* src = fm_path_to_gfile((FmPath*)l->data);
		guint32 src_dev = 0;
		inf = g_file_query_info(src, G_FILE_ATTRIBUTE_UNIX_DEVICE, 0, cancellable, &err);
		g_object_unref(src);
		if(inf)
		{
			src_dev = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_DEVICE);
			g_object_ref(inf);
		}
		else
		{
			
		}
	}
#endif
	return FALSE;
}

gboolean trash_files(FmFileOpsJob* job)
{
/*
	GList* l = fm_list_peek_head_link(job->srcs);
	for(; !FM_JOB(job)->cancel && l;l=l->next)
	{
		GError* err = NULL;
		GFile* src = fm_path_to_gfile((FmPath*)l->data);
		if( !g_file_trash(src, cancellable, &err) )
		{
			g_debug(err->message);
			g_error_free(err);
		}
	}
*/
	return FALSE;
}

gboolean delete_files(FmFileOpsJob* job)
{
	GList* l;
	/* prepare the job, count total work needed with FmDeepCountJob */
	FmDeepCountJob* dc = fm_deep_count_job_new(job->srcs, FM_DC_JOB_DEFAULT);
	fm_job_run_sync(dc);
	job->total = dc->total_size;
	g_object_unref(dc);
	g_debug("total size to delete: %llu", job->total);

	l = fm_list_peek_head_link(job->srcs);
	for(; !FM_JOB(job)->cancel && l;l=l->next)
	{
		GError* err = NULL;
		GFile* src = fm_path_to_gfile((FmPath*)l->data);
		gboolean ret = fm_file_ops_job_delete_file(job, src, NULL, &err);
		g_object_unref(src);
		if(!ret) /* error! */
        {
            fm_job_emit_error(job, err, FALSE);
			g_error_free(err);
        }
	}
	return TRUE;
}

gboolean chmod_files(FmFileOpsJob* job)
{

	return FALSE;
}

gboolean chown_files(FmFileOpsJob* job)
{

	return FALSE;
}

static void emit_cur_file(FmFileOpsJob* job, const char* cur_file)
{
	g_signal_emit(job, signals[CUR_FILE], 0, cur_file);
}

void fm_file_ops_job_emit_cur_file(FmFileOpsJob* job, const char* cur_file)
{
	fm_job_call_main_thread(job, emit_cur_file, cur_file);
}

static void emit_percent(FmFileOpsJob* job, gpointer percent)
{
	g_signal_emit(job, signals[PERCENT], 0, (guint)percent);
}

void fm_file_ops_job_emit_percent(FmFileOpsJob* job)
{
    guint percent = (guint)(job->finished + job->current) * 100 / job->total;
    if( percent > job->percent )
    {
    	fm_job_call_main_thread(job, emit_percent, (gpointer)percent);
        job->percent = percent;
    }
}

