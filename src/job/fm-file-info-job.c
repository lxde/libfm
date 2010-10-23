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

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <menu-cache.h>
#include <errno.h>

static void fm_file_info_job_finalize  			(GObject *object);
static gboolean fm_file_info_job_run(FmJob* fmjob);

const char gfile_info_query_attribs[]="standard::*,unix::*,time::*,access::*,id::filesystem";

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
    fm_list_unref(self->file_infos);

	G_OBJECT_CLASS(fm_file_info_job_parent_class)->finalize(object);
}


static void fm_file_info_job_init(FmFileInfoJob *self)
{
	self->file_infos = fm_file_info_list_new();
    fm_job_init_cancellable(FM_JOB(self));
}

FmJob* fm_file_info_job_new(FmPathList* files_to_query, FmFileInfoJobFlags flags)
{
	GList* l;
	FmFileInfoJob* job = (FmFileInfoJob*)g_object_new(FM_TYPE_FILE_INFO_JOB, NULL);
	FmFileInfoList* file_infos;

    job->flags = flags;
	if(files_to_query)
	{
		file_infos = job->file_infos;
		for(l = fm_list_peek_head_link(files_to_query);l;l=l->next)
		{
			FmPath* path = (FmPath*)l->data;
			FmFileInfo* fi = fm_file_info_new();
			fi->path = fm_path_ref(path);
			fm_list_push_tail_noref(file_infos, fi);
		}
	}
	return (FmJob*)job;
}

void _fm_file_info_set_from_menu_cache_item(FmFileInfo* fi, MenuCacheItem* item);

gboolean fm_file_info_job_run(FmJob* fmjob)
{
	GList* l;
	FmFileInfoJob* job = (FmFileInfoJob*)fmjob;
    GError* err = NULL;

	for(l = fm_list_peek_head_link(job->file_infos); !fm_job_is_cancelled(fmjob) && l;)
	{
		FmFileInfo* fi = (FmFileInfo*)l->data;
        GList* next = l->next;

        job->current = fi->path;

		if(fm_path_is_native(fi->path))
		{
			char* path_str = fm_path_to_str(fi->path);
			if(!_fm_file_info_job_get_info_for_native_file(FM_JOB(job), fi, path_str, &err))
            {
                FmJobErrorAction act = fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_MILD);
                g_error_free(err);
                err = NULL;
                if(act == FM_JOB_RETRY)
                    continue; /* retry */

                next = l->next;
                fm_list_delete_link(job->file_infos, l); /* also calls unref */
            }
			g_free(path_str);
		}
		else
		{
            GFile* gf;
            if(fm_path_is_virtual(fi->path))
            {
                /* this is a xdg menu */
                if(fm_path_is_xdg_menu(fi->path))
                {
                    MenuCache* mc;
                    MenuCacheDir* dir;
                    char* path_str = fm_path_to_str(fi->path);
                    char* menu_name = path_str + 5, ch;
                    char* dir_name;
                    while(*menu_name == '/')
                        ++menu_name;
                    dir_name = menu_name;
                    while(*dir_name && *dir_name != '/')
                        ++dir_name;
                    ch = *dir_name;
                    *dir_name = '\0';
                    menu_name = g_strconcat(menu_name, ".menu", NULL);
                    mc = menu_cache_lookup_sync(menu_name);
                    g_free(menu_name);

                    if(*dir_name && !(*dir_name == '/' && dir_name[1]=='\0') )
                    {
                        char* tmp = g_strconcat("/", menu_cache_item_get_id(MENU_CACHE_ITEM(menu_cache_get_root_dir(mc))), dir_name, NULL);
                        dir = menu_cache_get_dir_from_path(mc, tmp);
                        g_free(tmp);
                    }
                    else
                        dir = menu_cache_get_root_dir(mc);
                    if(dir)
                        _fm_file_info_set_from_menu_cache_item(fi, dir);
                    else
                    {
                        next = l->next;
                        fm_list_delete_link(job->file_infos, l); /* also calls unref */
                    }
                    g_free(path_str);
                    menu_cache_unref(mc);
                    l=l->next;
                    continue;
                }
            }

			gf = fm_path_to_gfile(fi->path);
			if(!_fm_file_info_job_get_info_for_gfile(FM_JOB(job), fi, gf, &err))
            {
                FmJobErrorAction act = fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_MILD);
                g_error_free(err);
                err = NULL;
                if(act == FM_JOB_RETRY)
                    continue; /* retry */

                next = l->next;
                fm_list_delete_link(job->file_infos, l); /* also calls unref */
            }
			g_object_unref(gf);
		}
        l = next;
	}
	return TRUE;
}

/* this can only be called before running the job. */
void fm_file_info_job_add(FmFileInfoJob* job, FmPath* path)
{
	FmFileInfo* fi = fm_file_info_new();
	fi->path = fm_path_ref(path);
	fm_list_push_tail_noref(job->file_infos, fi);
}

void fm_file_info_job_add_gfile(FmFileInfoJob* job, GFile* gf)
{
    FmPath* path = fm_path_new_for_gfile(gf);
	FmFileInfo* fi = fm_file_info_new();
	fi->path = path;
	fm_list_push_tail_noref(job->file_infos, fi);
}

gboolean _fm_file_info_job_get_info_for_native_file(FmJob* job, FmFileInfo* fi, const char* path, GError** err)
{
	struct stat st;
    gboolean is_link;
_retry:
	if( lstat( path, &st ) == 0 )
	{
		char* type;
        fi->disp_name = fi->path->name;
        fi->mode = st.st_mode;
        fi->mtime = st.st_mtime;
        fi->atime = st.st_atime;
        fi->size = st.st_size;
        fi->dev = st.st_dev;
        fi->uid = st.st_uid;
        fi->gid = st.st_gid;

		if( ! fm_job_is_cancelled(FM_JOB(job)) )
		{
            /* FIXME: handle symlinks */
            if(S_ISLNK(st.st_mode))
            {
                stat(path, &st);
                fi->target = g_file_read_link(path, NULL);
            }

			fi->type = fm_mime_type_get_for_native_file(path, fi->disp_name, &st);

            /* special handling for desktop entry files */
            if(G_UNLIKELY(fm_file_info_is_desktop_entry(fi)))
            {
                char* fpath = fm_path_to_str(fi->path);
                GKeyFile* kf = g_key_file_new();
                FmIcon* icon = NULL;
                if(g_key_file_load_from_file(kf, fpath, 0, NULL))
                {
                    char* icon_name = g_key_file_get_locale_string(kf, "Desktop Entry", "Icon", NULL, NULL);
                    char* title = g_key_file_get_locale_string(kf, "Desktop Entry", "Name", NULL, NULL);
                    if(icon_name)
                    {
                        if(icon_name[0] != '/') /* this is a icon name, not a full path to icon file. */
                        {
                            char* dot = strrchr(icon_name, '.');
                            /* remove file extension */
                            if(dot)
                            {
                                ++dot;
                                if(strcmp(dot, "png") == 0 ||
                                   strcmp(dot, "svg") == 0 ||
                                   strcmp(dot, "xpm") == 0)
                                   *(dot-1) = '\0';
                            }
                        }
                        icon = fm_icon_from_name(icon_name);
                        g_free(icon_name);
                    }
                    if(title)
                        fi->disp_name = title;
                }
                if(icon)
                    fi->icon = icon;
                else
                    fi->icon = fm_icon_ref(fi->type->icon);
            }
            else
                fi->icon = fm_icon_ref(fi->type->icon);
		}
	}
	else
    {
        g_set_error(err, G_IO_ERROR, g_io_error_from_errno(errno), g_strerror(errno));
		return FALSE;
    }
	return TRUE;
}

gboolean _fm_file_info_job_get_info_for_gfile(FmJob* job, FmFileInfo* fi, GFile* gf, GError** err)
{
	GFileInfo* inf;
	inf = g_file_query_info(gf, gfile_info_query_attribs, 0, fm_job_get_cancellable(job), err);
	if( !inf )
		return FALSE;
	fm_file_info_set_from_gfileinfo(fi, inf);

	return TRUE;
}

/* This API should only be called in error handler */
FmPath* fm_file_info_job_get_current(FmFileInfoJob* job)
{
    return job->current;
}
