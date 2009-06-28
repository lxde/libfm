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
#include <gio/gio.h>
#include <string.h>
#include <glib/gstdio.h>

#include "fm-mime-type.h"

static void fm_dir_list_job_finalize  			(GObject *object);
G_DEFINE_TYPE(FmDirListJob, fm_dir_list_job, FM_TYPE_JOB);

static gboolean fm_dir_list_job_run(FmDirListJob *job);
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


FmJob* fm_dir_list_job_new(FmPath* path)
{
	/* FIXME: should we cache this with hash table? Or, the cache
	 * should be done at the level of FmFolder instead? */
	FmDirListJob* job = (FmJob*)g_object_new(FM_TYPE_DIR_LIST_JOB, NULL);
	job->dir_path = fm_path_ref(path);
	job->files = fm_file_info_list_new();
	return (FmJob*)job;
}

FmJob* fm_dir_list_job_new_for_gfile(GFile* gf)
{
	/* FIXME: should we cache this with hash table? Or, the cache
	 * should be done at the level of FmFolder instead? */
	FmDirListJob* job = (FmJob*)g_object_new(FM_TYPE_DIR_LIST_JOB, NULL);
	job->dir_path = fm_path_new_for_gfile(gf);
	return (FmJob*)job;
}

static void fm_dir_list_job_finalize(GObject *object)
{
	FmDirListJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_DIR_LIST_JOB(object));

	self = FM_DIR_LIST_JOB(object);

	if(self->dir_path)
		fm_path_unref(self->dir_path);

	if(self->files)
		fm_list_unref(self->files);

	if (G_OBJECT_CLASS(fm_dir_list_job_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_dir_list_job_parent_class)->finalize)(object);
}

gboolean fm_dir_list_job_run(FmDirListJob* job)
{
	GFileEnumerator *enu;
	GFileInfo *inf;
	GError *err = NULL;

	if(fm_path_is_native(job->dir_path)) /* if this is a native file on real file system */
	{
		char* dir_path = fm_path_to_str(job->dir_path);
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
			while( ! FM_JOB(job)->cancel && (name = g_dir_read_name(dir)) )
			{
				struct stat st;
				g_string_truncate(fpath, dir_len);
				g_string_append(fpath, name);
				if( g_stat( fpath->str, &st ) == 0)
				{
					FmFileInfo* fi = fm_file_info_new();
					char* type;
					fi->path = fm_path_new_child(job->dir_path, name);
					fi->disp_name = fi->path->name;

					fi->mode = st.st_mode;
					fi->mtime = st.st_mtime;
					fi->size = st.st_size;

					if( ! FM_JOB(job)->cancel )
					{
						fi->type = fm_mime_type_get_for_native_file(fpath->str, fi->disp_name, &st);
						fi->icon = fm_icon_ref(fi->type->icon);
					}
					fm_list_push_tail_noref(job->files, fi);
				}
			}
			g_string_free(fpath, TRUE);
			g_dir_close(dir);
		}
		g_free(dir_path);
	}
	else /* this is a virtual path or remote file system path */
	{
		FmJob* fmjob = FM_JOB(job);
		GFile* gf;

		if(!fm_job_init_cancellable(fmjob))
			return FALSE;

		gf = fm_path_to_gfile(job->dir_path);
		enu = g_file_enumerate_children (gf, "standard::*", 0, fmjob->cancellable, &err);
		g_object_unref(gf);
		while( ! FM_JOB(job)->cancel )
		{
			FmFileInfo* fi;
			inf = g_file_enumerator_next_file(enu, fmjob->cancellable, &err);
			if(inf)
			{
				fi = fm_file_info_new_from_gfileinfo(job->dir_path, inf);
				fm_list_push_tail_noref(job->files, fi);
			}
			else
            {
                if(err)
                {
                    fm_job_emit_error(fmjob, err, FALSE);
                    g_error_free(err);
                }
				break; /* FIXME: error handling */
            }
			g_object_unref(inf);
		}
		g_file_enumerator_close(enu, NULL, &err);
		g_object_unref(enu);
	}
	return TRUE;
}

FmFileInfoList* fm_dir_dist_job_get_files(FmDirListJob* job)
{
	return job->files;
}
