/*
 *      fm-dir-list-job.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-dir-list-job.h"
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <string.h>
#include <glib/gstdio.h>
#include "fm-mime-type.h"
#include "fm-file-info-job.h"
#include <menu-cache.h>

extern const char gfile_info_query_attribs[]; /* defined in fm-file-info-job.c */

static void fm_dir_list_job_finalize  			(GObject *object);
G_DEFINE_TYPE(FmDirListJob, fm_dir_list_job, FM_TYPE_JOB);

static gboolean fm_dir_list_job_run(FmDirListJob *job);


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

    if(self->dir_fi)
        fm_file_info_unref(self->dir_fi);

	if(self->files)
		fm_list_unref(self->files);

	if (G_OBJECT_CLASS(fm_dir_list_job_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_dir_list_job_parent_class)->finalize)(object);
}

static void on_menu_cache_reload(MenuCache* mc, gpointer user_data)
{
    GMainLoop* mainloop = (GMainLoop*)user_data;
    g_main_loop_quit(mainloop);
}

static gpointer list_apps(FmJob* fmjob, gpointer user_data)
{
    FmDirListJob* job = (FmDirListJob*)fmjob;
    const char* dir_path = (const char*)user_data;
    FmFileInfo* fi;
    MenuCache* mc = menu_cache_lookup("applications.menu");
    MenuCacheDir* dir;
    GList* l;

    /* ensure that the menu cache is loaded */
    if(! menu_cache_get_root_dir(mc)) /* if it's not yet loaded */
    {
        GMainLoop* mainloop = g_main_loop_new(NULL, FALSE);
        gpointer notify_id = menu_cache_add_reload_notify(mc, on_menu_cache_reload, mainloop);
        g_main_loop_run(mainloop);
        menu_cache_remove_reload_notify(mc, notify_id);
    }

    if(*dir_path && !(*dir_path == '/' && dir_path[1]=='\0') )
    {
        char* tmp = g_strconcat("/", menu_cache_item_get_id(menu_cache_get_root_dir(mc)), dir_path, NULL);
        dir = menu_cache_get_dir_from_path(mc, tmp);
        g_free(tmp);
    }
    else
        dir = menu_cache_get_root_dir(mc);

    if(dir)
    {
        FmMimeType* shortcut_type = fm_mime_type_get_for_type("inode/x-shortcut");
        for(l=menu_cache_dir_get_children(dir);l;l=l->next)
        {
            MenuCacheItem* item = MENU_CACHE_ITEM(l->data);
            GIcon* gicon;
            if(!item || menu_cache_item_get_type(item) == MENU_CACHE_TYPE_SEP)
                continue;
            fi = fm_file_info_new();
            fi->path = fm_path_new_child(job->dir_path, menu_cache_item_get_id(item));
            fi->disp_name = g_strdup(menu_cache_item_get_name(item));
            fi->icon = fm_icon_from_name(menu_cache_item_get_icon(item));
            if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_DIR)
            {
                fi->mode |= S_IFDIR;
            }
            else if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP)
            {
                fi->mode |= S_IFREG;
                fi->target = menu_cache_item_get_file_path(item);
            }
            fi->type = fm_mime_type_ref(shortcut_type);
            fm_list_push_tail_noref(job->files, fi);
        }
        fm_mime_type_unref(shortcut_type);
    }
    menu_cache_unref(mc);
}

gboolean fm_dir_list_job_list_apps(FmDirListJob* job, const char* dir_path)
{
    /* Calling libmenu-cache is only allowed in main thread. */
    fm_job_call_main_thread(job, list_apps, dir_path);
    return TRUE;
}

gboolean fm_dir_list_job_run(FmDirListJob* job)
{
	GFileEnumerator *enu;
	GFileInfo *inf;
    FmFileInfo* fi;
	GError *err = NULL;

	if(fm_path_is_native(job->dir_path)) /* if this is a native file on real file system */
	{
		char* dir_path;
		GDir* dir;

        dir_path = fm_path_to_str(job->dir_path);

        fi = fm_file_info_new();
        fi->path = fm_path_ref(job->dir_path);
        if( fm_file_info_job_get_info_for_native_file(job, fi, dir_path) )
        {
            job->dir_fi = fi;
            if(! fm_file_info_is_dir(fi))
            {
                fm_file_info_unref(fi);
                return FALSE;
            }
        }
        else
        {
            fm_file_info_unref(fi);
            return FALSE;
        }

        dir = g_dir_open(dir_path, 0, NULL);
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
				g_string_truncate(fpath, dir_len);
				g_string_append(fpath, name);
				fi = fm_file_info_new();
				fi->path = fm_path_new_child(job->dir_path, name);
				if( fm_file_info_job_get_info_for_native_file(job, fi, fpath->str) )
					fm_list_push_tail_noref(job->files, fi);
				else /* failed! */
					fm_file_info_unref(fi);
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
        char* str = fm_path_to_str(job->dir_path);
        /* handle some built-in virtual dirs */
        // if( G_UNLIKELY(job->dir_path == fm_path_get_applications()) ) /* applications */
        if( G_UNLIKELY( g_str_has_prefix(str, "applications://") ) ) /* applications */
            return fm_dir_list_job_list_apps(job, str+15);
        g_free(str);

		if(!fm_job_init_cancellable(fmjob))
			return FALSE;
		gf = fm_path_to_gfile(job->dir_path);

_retry:
        inf = g_file_query_info(gf, gfile_info_query_attribs, 0, fmjob->cancellable, &err);
        if(!inf )
        {
            if( fm_job_emit_error(fmjob, err, TRUE) )
            {
                g_error_free(err);
                err = NULL;
                goto _retry;
            }
            else
            {
                g_object_unref(gf);
                g_error_free(err);
                return FALSE;
            }
        }

        if( g_file_info_get_file_type(inf) != G_FILE_TYPE_DIRECTORY)
        {
            err = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY, _("It's not a valid directory"));
            fm_job_emit_error(fmjob, err, FALSE);
            g_error_free(err);
            g_object_unref(gf);
            g_object_unref(inf);
            return FALSE;
        }

        /* FIXME: should we use fm_file_info_new + fm_file_info_set_from_gfileinfo? */
        job->dir_fi = fm_file_info_new_from_gfileinfo(job->dir_path, inf);
        g_object_unref(inf);

		enu = g_file_enumerate_children (gf, gfile_info_query_attribs, 0, fmjob->cancellable, &err);
		g_object_unref(gf);
        if(enu)
        {
            while( ! FM_JOB(job)->cancel )
            {
                inf = g_file_enumerator_next_file(enu, fmjob->cancellable, &err);
                if(inf)
                {
                    FmPath* sub = fm_path_new_child(job->dir_path, g_file_info_get_name(inf));
                    fi = fm_file_info_new_from_gfileinfo(sub, inf);
                    fm_path_unref(sub);
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
        else
        {
            fm_job_emit_error(fmjob, err, FALSE);
            g_error_free(err);
            return FALSE;
        }
	}
	return TRUE;
}

FmFileInfoList* fm_dir_dist_job_get_files(FmDirListJob* job)
{
	return job->files;
}
