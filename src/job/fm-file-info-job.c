/*
 *      fm-file-info-job.c
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

#include "fm-file-info-job.h"
#include <glib/gstdio.h>

static void fm_file_info_job_finalize  			(GObject *object);
static gboolean fm_file_info_job_run(FmJob* fmjob);

const char gfile_info_query_flags[]="standard::*,unix::*,time::*,access::*";

G_DEFINE_TYPE(FmFileInfoJob, fm_file_info_job, FM_TYPE_JOB);


static void fm_file_info_job_class_init(FmFileInfoJobClass *klass)
{
	GObjectClass *g_object_class;
	FmJobClass* job_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_info_job_finalize;

	job_class = FM_JOB_CLASS(klass);
	job_class->run = fm_file_info_job_run;
}


static void fm_file_info_job_finalize(GObject *object)
{
	FmFileInfoJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_FILE_INFO_JOB(object));

	self = FM_FILE_INFO_JOB(object);

	G_OBJECT_CLASS(fm_file_info_job_parent_class)->finalize(object);
}


static void fm_file_info_job_init(FmFileInfoJob *self)
{
	self->file_infos = fm_file_info_list_new();
}

FmJob* fm_file_info_job_new (FmPathList* files_to_query)
{
	GList* l;
	FmJob* job = (FmJob*)g_object_new(FM_TYPE_FILE_INFO_JOB, NULL);
	FmFileInfoList* file_infos;
	
	if(files_to_query)
	{
		file_infos = ((FmFileInfoJob*)job)->file_infos;
		for(l = fm_list_peek_head_link(files_to_query);l;l=l->next)
		{
			FmPath* path = (FmPath*)l->data;
			FmFileInfo* fi = fm_file_info_new();
			fi->path = fm_path_ref(path);
			fm_list_push_tail_noref(file_infos, fi);
		}
	}
	return job;
}

gboolean fm_file_info_job_run(FmJob* fmjob)
{
	GList* l;
	FmFileInfoJob* job = (FmFileInfoJob*)fmjob;
	for(l = fm_list_peek_head_link(job->file_infos); !fmjob->cancel && l;l=l->next)
	{
		FmFileInfo* fi = (FmFileInfo*)l->data;
		if(fm_path_is_native(fi->path))
		{
			char* path_str = fm_path_to_str(fi->path);
			fm_file_info_job_get_info_for_native_file(job, fi, path_str);
			g_free(path_str);
		}
		else
		{
			GFile* gf = fm_path_to_gfile(fi->path);
			fm_file_info_job_get_info_for_gfile(job, fi, gf);
			g_object_unref(gf);
		}
	}
	return TRUE;
}

/* this can only be called before running the job. */
void fm_file_info_job_add (FmFileInfoJob* job, FmPath* path)
{
	FmFileInfo* fi = fm_file_info_new();
	fi->path = fm_path_ref(path);
	fm_list_push_tail_noref(job->file_infos, fi);
}

gboolean fm_file_info_job_get_info_for_native_file(FmJob* job, FmFileInfo* fi, const char* path)
{
	struct stat st;
	if( g_stat( path, &st ) == 0 )
	{
		char* type;
		fi->disp_name = fi->path->name;

		fi->mode = st.st_mode;
		fi->mtime = st.st_mtime;
		fi->size = st.st_size;
		fi->dev = st.st_dev;

		if( ! FM_JOB(job)->cancel )
		{
			fi->type = fm_mime_type_get_for_native_file(path, fi->disp_name, &st);

            /* special handling for desktop entry files */
            if(G_UNLIKELY(fm_file_info_is_desktop_entry(fi)))
            {
                char* fpath = fm_path_to_str(fi->path);
                GKeyFile* kf = g_key_file_new();
                GIcon* gicon = NULL;
                if(g_key_file_load_from_file(kf, fpath, 0, NULL))
                {
                    char* icon_name = g_key_file_get_locale_string(kf, "Desktop Entry", "Icon", NULL, NULL);
                    char* title = g_key_file_get_locale_string(kf, "Desktop Entry", "Name", NULL, NULL);
                    if(icon_name)
                    {
                        if(g_path_is_absolute(icon_name))
                        {
                            GFile* gicon_file = g_file_new_for_path(icon_name);
                            gicon = g_file_icon_new(gicon_file);
                            g_object_unref(gicon_file);
                        }
                        else
                            gicon = g_themed_icon_new(icon_name);
                    }
                    if(title)
                        fi->disp_name = title;
                }
                if(gicon)
                {
                    fi->icon = fm_icon_from_gicon(gicon);
                    g_object_unref(gicon);
                }
                else
                    fi->icon = fm_icon_ref(fi->type->icon);
            }
            else
                fi->icon = fm_icon_ref(fi->type->icon);
		}
	}
	else
		return FALSE;
	return TRUE;
}

gboolean fm_file_info_job_get_info_for_gfile(FmJob* job, FmFileInfo* fi, GFile* gf)
{
	GFileInfo* inf;
	GError* err;
	if(!fm_job_init_cancellable(job))
		return FALSE;
	err = NULL;
	inf = g_file_query_info(gf, gfile_info_query_flags, 0, job->cancellable, &err);
	if( !inf )
	{
		fm_job_emit_error(job, err, FALSE);
		g_error_free(err);
		return FALSE;
	}
	fm_file_info_set_from_gfileinfo(fi, inf);
	return TRUE;
}
