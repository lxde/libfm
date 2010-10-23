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
}


static void fm_dir_list_job_init(FmDirListJob *self)
{
    fm_job_init_cancellable(FM_JOB(self));
}


FmJob* fm_dir_list_job_new(FmPath* path, gboolean dir_only)
{
	FmDirListJob* job = (FmJob*)g_object_new(FM_TYPE_DIR_LIST_JOB, NULL);
	job->dir_path = fm_path_ref(path);
    job->dir_only = dir_only;
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


/* defined in fm-file-info.c */
FmFileInfo* _fm_file_info_new_from_menu_cache_item(FmPath* path, MenuCacheItem* item);

static gpointer list_menu_items(FmJob* fmjob, gpointer user_data)
{
    FmDirListJob* job = (FmDirListJob*)fmjob;
    FmFileInfo* fi;
    MenuCache* mc;
    MenuCacheDir* dir;
    GList* l;
    char* path_str, *p, ch;
    char* menu_name;
    const char* dir_path;
    guint32 de_flag;
    const char* de_name;
    /* example: menu://applications.menu/DesktopSettings */

    path_str = fm_path_to_str(job->dir_path);
    p = path_str + 5; /* skip menu: */
    while(*p == '/')
        ++p;
    menu_name = p;
    while(*p && *p != '/')
        ++p;
    ch = *p;
    *p = '\0';
    menu_name = g_strconcat(menu_name, ".menu", NULL);
    mc = menu_cache_lookup_sync(menu_name);
    /* ensure that the menu cache is loaded */
    if(!mc) /* if it's not loaded */
    {
        /* try to set $XDG_MENU_PREFIX to "lxde-" for lxmenu-data */
        const char* menu_prefix = g_getenv("XDG_MENU_PREFIX");
        if(g_strcmp0(menu_prefix, "lxde-")) /* if current value is not lxde- */
        {
            char* old_prefix = g_strdup(menu_prefix);
            g_setenv("XDG_MENU_PREFIX", "lxde-", TRUE);
            mc = menu_cache_lookup_sync(menu_name);
            /* restore original environment variable */
            if(old_prefix)
            {
                g_setenv("XDG_MENU_PREFIX", old_prefix, TRUE);
                g_free(old_prefix);
            }
            else
                g_unsetenv("XDG_MENU_PREFIX");

            if(!mc)
            {
                g_free(menu_name);
                return NULL;
            }
        }
        else
        {
            g_free(menu_name);
            return NULL;
        }
    }
    g_free(menu_name);
    *p = ch;
    dir_path = p; /* path of menu dir, such as: /Internet */

    de_name = g_getenv("XDG_CURRENT_DESKTOP");
    if(de_name)
        de_flag = menu_cache_get_desktop_env_flag(mc, de_name);
    else
        de_flag = (guint32)-1;

    /* the menu should be loaded now */
    if(*dir_path && !(*dir_path == '/' && dir_path[1]=='\0') )
    {
        char* tmp = g_strconcat("/", menu_cache_item_get_id(MENU_CACHE_ITEM(menu_cache_get_root_dir(mc))), dir_path, NULL);
        dir = menu_cache_get_dir_from_path(mc, tmp);
        g_free(tmp);
    }
    else
        dir = menu_cache_get_root_dir(mc);

    if(dir)
    {
        job->dir_fi = _fm_file_info_new_from_menu_cache_item(job->dir_path, dir);
        for(l=menu_cache_dir_get_children(dir);l;l=l->next)
        {
            MenuCacheItem* item = MENU_CACHE_ITEM(l->data);
            FmPath* item_path;
            GIcon* gicon;
            /* also hide menu items which should be hidden in current DE. */
            if(!item || menu_cache_item_get_type(item) == MENU_CACHE_TYPE_SEP)
                continue;
            if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP && !menu_cache_app_get_is_visible(MENU_CACHE_APP(item), de_flag))
                continue;

            if(G_UNLIKELY(job->dir_only) && menu_cache_item_get_type(item) != MENU_CACHE_TYPE_DIR)
                continue;
            item_path = fm_path_new_child(job->dir_path, menu_cache_item_get_id(item));
            fi = _fm_file_info_new_from_menu_cache_item(item_path, item);
            fm_path_unref(item_path);
            fm_list_push_tail_noref(job->files, fi);
        }
    }
    menu_cache_unref(mc);

    g_free(path_str);
    return NULL;
}

gboolean fm_dir_list_job_list_xdg_menu(FmDirListJob* job)
{
    /* Calling libmenu-cache is only allowed in main thread. */
    fm_job_call_main_thread(FM_JOB(job), list_menu_items, NULL);
    return TRUE;
}

static gboolean fm_dir_list_job_run_posix(FmDirListJob* job)
{
    FmFileInfo* fi;
	GError *err = NULL;
    char* dir_path;
    GDir* dir;

    dir_path = fm_path_to_str(job->dir_path);

    fi = fm_file_info_new();
    fi->path = fm_path_ref(job->dir_path);
    if( _fm_file_info_job_get_info_for_native_file(FM_JOB(job), fi, dir_path, NULL) )
    {
        job->dir_fi = fi;
        if(! fm_file_info_is_dir(fi))
        {
            err = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY, _("The specified directory is not valid"));
            fm_file_info_unref(fi);
            fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_CRITICAL);
            g_error_free(err);
            return FALSE;
        }
    }
    else
    {
        err = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY, _("The specified directory is not valid"));
        fm_file_info_unref(fi);
        fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_CRITICAL);
        g_error_free(err);
        return FALSE;
    }

    dir = g_dir_open(dir_path, 0, &err);
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
        while( ! fm_job_is_cancelled(FM_JOB(job)) && (name = g_dir_read_name(dir)) )
        {
            g_string_truncate(fpath, dir_len);
            g_string_append(fpath, name);

            if(job->dir_only) /* if we only want directories */
            {
                struct stat st;
                /* FIXME: this results in an additional stat() call, which is inefficient */
                if(stat(fpath->str, &st) == -1 || !S_ISDIR(st.st_mode))
                    continue;
            }

            fi = fm_file_info_new();
            fi->path = fm_path_new_child(job->dir_path, name);

        _retry:
            if( _fm_file_info_job_get_info_for_native_file(FM_JOB(job), fi, fpath->str, &err) )
                fm_list_push_tail_noref(job->files, fi);
            else /* failed! */
            {
                FmJobErrorAction act = fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_MILD);
                g_error_free(err);
                err = NULL;
                if(act == FM_JOB_RETRY)
                    goto _retry;

                fm_file_info_unref(fi);
            }
        }
        g_string_free(fpath, TRUE);
        g_dir_close(dir);
    }
    else
    {
        fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_CRITICAL);
        g_error_free(err);
    }
    g_free(dir_path);
    return TRUE;
}

static gboolean fm_dir_list_job_run_gio(FmDirListJob* job)
{
	GFileEnumerator *enu;
	GFileInfo *inf;
    FmFileInfo* fi;
	GError *err = NULL;
    FmJob* fmjob = FM_JOB(job);
    GFile* gf;
    const char* query;

    /* handle some built-in virtual dirs */
    if( fm_path_is_xdg_menu(job->dir_path) ) /* xdg menu:// */
        return fm_dir_list_job_list_xdg_menu(job);

    gf = fm_path_to_gfile(job->dir_path);
_retry:
    inf = g_file_query_info(gf, gfile_info_query_attribs, 0, fm_job_get_cancellable(fmjob), &err);
    if(!inf )
    {
        FmJobErrorAction act = fm_job_emit_error(fmjob, err, FM_JOB_ERROR_MODERATE);
        if( act == FM_JOB_RETRY )
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
        err = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY, _("The specified directory is not valid"));
        fm_job_emit_error(fmjob, err, FM_JOB_ERROR_CRITICAL);
        g_error_free(err);
        g_object_unref(gf);
        g_object_unref(inf);
        return FALSE;
    }

    /* FIXME: should we use fm_file_info_new + fm_file_info_set_from_gfileinfo? */
    job->dir_fi = fm_file_info_new_from_gfileinfo(job->dir_path, inf);
    g_object_unref(inf);

    if(G_UNLIKELY(job->dir_only))
    {
        query = G_FILE_ATTRIBUTE_STANDARD_TYPE","G_FILE_ATTRIBUTE_STANDARD_NAME","
                G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN","G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP","
                G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL","
                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","G_FILE_ATTRIBUTE_STANDARD_ICON","
                G_FILE_ATTRIBUTE_STANDARD_SIZE","G_FILE_ATTRIBUTE_STANDARD_TARGET_URI","
                "unix::*,time::*,access::*,id::filesystem";
    }
    else
        query = gfile_info_query_attribs;

    enu = g_file_enumerate_children (gf, query, 0, fm_job_get_cancellable(fmjob), &err);
    g_object_unref(gf);
    if(enu)
    {
        while( ! fm_job_is_cancelled(FM_JOB(job)) )
        {
            inf = g_file_enumerator_next_file(enu, fm_job_get_cancellable(fmjob), &err);
            if(inf)
            {
                FmPath* sub;
                if(G_UNLIKELY(job->dir_only))
                {
                    /* FIXME: handle symlinks */
                    if(g_file_info_get_file_type(inf) != G_FILE_TYPE_DIRECTORY)
                    {
                        g_object_unref(inf);
                        continue;
                    }
                }

                sub = fm_path_new_child(job->dir_path, g_file_info_get_name(inf));
                fi = fm_file_info_new_from_gfileinfo(sub, inf);
                fm_path_unref(sub);
                fm_list_push_tail_noref(job->files, fi);
            }
            else
            {
                if(err)
                {
                    FmJobErrorAction act = fm_job_emit_error(fmjob, err, FM_JOB_ERROR_MILD);
                    g_error_free(err);
                    /* FM_JOB_RETRY is not supported. */
                    if(act == FM_JOB_ABORT)
                        fm_job_cancel(FM_JOB(job));
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
        fm_job_emit_error(fmjob, err, FM_JOB_ERROR_CRITICAL);
        g_error_free(err);
        return FALSE;
    }
    return TRUE;
}

gboolean fm_dir_list_job_run(FmDirListJob* job)
{
    gboolean ret;
	if(fm_path_is_native(job->dir_path)) /* if this is a native file on real file system */
        ret = fm_dir_list_job_run_posix(job);
	else /* this is a virtual path or remote file system path */
        ret = fm_dir_list_job_run_gio(job);
    return ret;
}

FmFileInfoList* fm_dir_dist_job_get_files(FmDirListJob* job)
{
	return job->files;
}
