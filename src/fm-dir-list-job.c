/*
 *      fm-dir-list-job.c
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

#include "fm-dir-list-job.h"
#include <string.h>
#include <glib/gstdio.h>

#include "fm-mime-type.h"

static void fm_dir_list_job_finalize  			(GObject *object);
G_DEFINE_TYPE(FmDirListJob, fm_dir_list_job, FM_TYPE_JOB);

static gboolean fm_dir_list_job_run(FmDirListJob *job);
static void fm_dir_list_job_cancel(FmDirListJob *job);
static gpointer job_thread(FmDirListJob* job);


static void fm_dir_list_job_class_init(FmDirListJobClass *klass)
{
	GObjectClass *g_object_class;
	FmJobClass* job_class = FM_JOB_CLASS(klass);
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_dir_list_job_finalize;

	job_class->run = fm_dir_list_job_run;
	fm_dir_list_job_parent_class = (GObjectClass*)g_type_class_peek(FM_TYPE_JOB);
}


static void fm_dir_list_job_init(FmDirListJob *self)
{
	
}


FmJob* fm_dir_list_job_new(GFile* gf)
{
	/* FIXME: we need to cache this with hash table later */
	FmDirListJob* job = (FmJob*)g_object_new(FM_TYPE_DIR_LIST_JOB, NULL);
	job->gf = (GFile*)g_object_ref(gf);
	return (FmJob*)job;
}


static void fm_dir_list_job_finalize(GObject *object)
{
	FmDirListJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_DIR_LIST_JOB(object));

	self = FM_DIR_LIST_JOB(object);

	if(self->cancellable)
		g_object_unref(self->cancellable);

	if(self->gf)
		g_object_unref(self->gf);

	if (G_OBJECT_CLASS(fm_dir_list_job_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_dir_list_job_parent_class)->finalize)(object);
}



gboolean fm_dir_list_job_run(FmDirListJob* job)
{
	GThread* thread = g_thread_create((GThreadFunc)job_thread, job, FALSE, NULL);
	return thread != NULL;
}

void fm_dir_list_job_cancel(FmDirListJob *job)
{
	g_cancellable_cancel(job->cancellable);
	FM_JOB_CLASS(fm_dir_list_job_parent_class)->cancel((FmJob*)job);
}

gpointer job_thread(FmDirListJob* job)
{
	GFileEnumerator *enu;
	GFileInfo *inf;
	GError *err = NULL;
	char* dir_path = g_file_get_path(job->gf);
	if(dir_path) /* if this is a local path */
	{
		GDir* dir = g_dir_open(dir_path, 0, NULL);
		if( dir )
		{
			char* name;
			GString* fpath = g_string_sized_new(4096);
			int dir_len = strlen(dir_path);
			g_string_append_len(fpath, dir_path, dir_len);
			if(fpath->str[dir_len-1] != '/')
			{
				g_string_append_c(fpath, '/');
				++dir_len;
			}
			while( name = g_dir_read_name(dir) )
			{
				struct stat st;
				g_string_truncate(fpath, dir_len);
				g_string_append(fpath, name);
				if( g_stat( fpath->str, &st ) == 0)
				{
					FmFileInfo* fi = fm_file_info_new();
					char* type;
					fi->name = g_strdup(name);
					fi->disp_name = fi->name;

					fi->mode = st.st_mode;
					fi->mtime = st.st_mtime;

					fi->type = fm_mime_type_get_from_file(fpath->str, fi->disp_name, &st);
					fi->icon = g_object_ref(fi->type->icon);

					job->files = g_list_prepend(job->files, fi);
				}
			}
			g_string_free(fpath, TRUE);
			g_dir_close(dir);
		}
		g_free(dir_path);
	}
	else /* this is a virtual path or remote file system path */
	{
		enu = g_file_enumerate_children (job->gf, "standard::*", 0, job->cancellable, &err);
		while( !g_cancellable_is_cancelled(job->cancellable) )
		{
			FmFileInfo* fi;
			inf = g_file_enumerator_next_file(enu, job->cancellable, &err);
			if(inf)
			{
				fi = fm_file_info_new_from_gfileinfo(inf);
				job->files = g_list_prepend(job->files, fi);
			}
			else
				break; /* FIXME: error handling */
			g_object_unref(inf);
		}
		g_file_enumerator_close(enu, NULL, &err);
		g_object_unref(enu);
	}

	/* let the main thread know that we're done. */
	fm_job_finish(job);
	return NULL;
}
