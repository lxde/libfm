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
	/* prepare the job, count total work needed with FmDeepCountJob */
//	FmDeepCountJob* dc = fm_deep_count_job_new(fop_job->srcs, FM_DC_JOB_DEFAULT);
//	fm_job_run_sync(fop_job->dc_job);
	/* unref is not needed since the job will be freed in idle handler. */

	GList* l = fm_list_peek_head_link(job->srcs);
	for(; l; l=l->next)
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

static gboolean delete_file(FmJob* job, GFile* gf, GFileInfo* inf, GError** err)
{
#if 0
	gboolean is_dir;
	if( !inf)
	{
		inf = g_file_query_info(gf, 
							G_FILE_ATTRIBUTE_STANDARD_TYPE","
							G_FILE_ATTRIBUTE_STANDARD_NAME,
							G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
							&job->cancellable, &err);
	}
	if(!inf) /* FIXME: error handling? */
		return FALSE;
	is_dir = (g_file_info_get_file_type(inf)==G_FILE_TYPE_DIRECTORY);
	g_object_unref(inf);

	if( job->cancel )
		return FALSE;

	if( is_dir )
	{
		GFileEnumerator* enu = g_file_enumerate_children(dir,
									G_FILE_ATTRIBUTE_STANDARD_TYPE","
									G_FILE_ATTRIBUTE_STANDARD_NAME
									G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
									job->cancellable, &err);
		while( ! job->cancel )
		{
			inf = g_file_enumerator_next_file(enu, job->cancellable, &err);
			if(inf)
			{
				GFile* sub = g_file_get_child(dir, g_file_info_get_name(inf));
				delete_file(job, sub, inf, &err); /* FIXME: error handling? */
				g_object_unref(sub);
				g_object_unref(inf);
			}
			else
			{
				break; /* FIXME: error handling */
			}
		}
		g_object_unref(enu);
	}
	else
		return g_file_delete(gf, &job->cancellable, &err);
#endif
}

gboolean delete_files(FmFileOpsJob* job)
{
#if 0
	GList* l = fm_list_peek_head_link(job->srcs);
	for(; !FM_JOB(job)->cancel && l;l=l->next)
	{
		GError* err = NULL;
		GFile* src = fm_path_to_gfile((FmPath*)l->data);
		gboolean ret = delete_file(job, src, NULL, &err);
		g_object_unref(src);
		if(!ret) /* error! */
			g_error_free(err);
	}
#endif
	return FALSE;
}

gboolean chmod_files(FmFileOpsJob* job)
{

	return FALSE;
}

gboolean chown_files(FmFileOpsJob* job)
{

	return FALSE;
}
