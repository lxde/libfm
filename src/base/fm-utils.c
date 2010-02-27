/*
 *      fm-utils.c
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

#include <glib/gi18n-lib.h>
#include <libintl.h>
#include <gio/gdesktopappinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include "fm-utils.h"
#include "fm-file-info-job.h"

#define BI_KiB	((gdouble)1024.0)
#define BI_MiB	((gdouble)1024.0 * 1024.0)
#define BI_GiB	((gdouble)1024.0 * 1024.0 * 1024.0)
#define BI_TiB	((gdouble)1024.0 * 1024.0 * 1024.0 * 1024.0)

#define SI_KB	((gdouble)1000.0)
#define SI_MB	((gdouble)1000.0 * 1000.0)
#define SI_GB	((gdouble)1000.0 * 1000.0 * 1000.0)
#define SI_TB	((gdouble)1000.0 * 1000.0 * 1000.0 * 1000.0)

char* fm_file_size_to_str( char* buf, goffset size, gboolean si_prefix )
{
    const char * unit;
    gdouble val;

	if( si_prefix ) /* 1000 based SI units */
	{
		if(size < (goffset)SI_KB)
		{
			sprintf( buf, ngettext("%u byte", "%u bytes", (guint)size), (guint)size);
			return buf;
		}
		val = (gdouble)size;
		if(val < SI_MB)
		{
			val /= SI_KB;
			unit = _("KB");
		}
		else if(val < SI_GB)
		{
			val /= SI_MB;
			unit = _("MB");
		}
		else if(val < SI_TB)
		{
			val /= SI_GB;
			unit = _("GB");
		}
		else
		{
			val /= SI_TB;
			unit = _("TB");
		}
	}
	else /* 1024-based binary prefix */
	{
		if(size < (goffset)BI_KiB)
		{
			sprintf( buf, ngettext("%u byte", "%u bytes", (guint)size), (guint)size);
			return buf;
		}
		val = (gdouble)size;
		if(val < BI_MiB)
		{
			val /= BI_KiB;
			unit = _("KiB");
		}
		else if(val < BI_GiB)
		{
			val /= BI_MiB;
			unit = _("MiB");
		}
		else if(val < BI_TiB)
		{
			val /= BI_GiB;
			unit = _("GiB");
		}
		else
		{
			val /= BI_TiB;
			unit = _("TiB");
		}
	}
    sprintf( buf, "%.1f %s", val, unit );
	return buf;
}

gboolean fm_key_file_get_int(GKeyFile* kf, const char* grp, const char* key, int* val)
{
    char* str = g_key_file_get_value(kf, grp, key, NULL);
    if(G_LIKELY(str))
    {
        *val = atoi(str);
        g_free(str);
    }
    return str != NULL;
}

gboolean fm_key_file_get_bool(GKeyFile* kf, const char* grp, const char* key, gboolean* val)
{
    char* str = g_key_file_get_value(kf, grp, key, NULL);
    if(G_LIKELY(str))
    {
        *val = (str[0] == '1' || str[0] == 't');
        g_free(str);
    }
    return str != NULL;
}

static void launch_files(GAppLaunchContext* ctx, GAppInfo* app, GList* file_infos)
{

}

gboolean fm_launch_desktop_entry(GAppLaunchContext* ctx, const char* file_or_id, GList* uris, GError** err)
{
    GKeyFile* kf = g_key_file_new();
    gboolean loaded;
    gboolean ret = FALSE;

    if(g_path_is_absolute(file_or_id))
        loaded = g_key_file_load_from_file(kf, file_or_id, 0, err);
    else
    {
        char* tmp = g_strconcat("applications/", file_or_id, NULL);
        loaded = g_key_file_load_from_data_dirs(kf, tmp, NULL, 0, err);
        g_free(tmp);
    }

    if(loaded)
    {
        GList* _uris = NULL;
        GAppInfo* app = NULL;
        char* type = g_key_file_get_string(kf, G_KEY_FILE_DESKTOP_GROUP, "Type", NULL);
        if(type)
        {
            if(strcmp(type, "Application") == 0)
                app = g_desktop_app_info_new_from_keyfile(kf);
            else if(strcmp(type, "Link") == 0)
            {
                char* url = g_key_file_get_string(kf, G_KEY_FILE_DESKTOP_GROUP, "URL", NULL);
                if(url)
                {
                    char* scheme = g_uri_parse_scheme(url);
                    if(scheme)
                    {
                        /* Damn! this actually relies on gconf to work. */
                        /* FIXME: use our own way to get a usable browser later. */
                        app = g_app_info_get_default_for_uri_scheme(scheme);
                        uris = _uris = g_list_prepend(NULL, url);
                        g_free(scheme);
                    }
                }
            }
            else if(strcmp(type, "Directory") == 0)
            {
                /* FIXME: how should this work? It's not defined in the spec. */
            }
            if(app)
                ret = g_app_info_launch_uris(app, uris, ctx, err);
        }
    }
    g_key_file_free(kf);

    return ret;
}

gboolean fm_launch_files(GAppLaunchContext* ctx, GList* file_infos, FmFileLauncher* launcher, gpointer user_data)
{
    GList* l;
    GHashTable* hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
    GList* folders = NULL;
    FmFileInfo* fi;
    GError* err = NULL;
    GAppInfo* app;

    for(l = file_infos; l; l=l->next)
    {
        GList* fis;
        fi = (FmFileInfo*)l->data;
        if(fm_file_info_is_dir(fi))
            folders = g_list_prepend(folders, fi);
        else
        {
            /* FIXME: handle shortcuts, such as the items in menu:// */
            if(fm_path_is_native(fi->path))
            {
                char* filename;
                if(fm_file_info_is_desktop_entry(fi))
                {
                    /* if it's a desktop entry file, directly launch it. */
                    filename = fm_path_to_str(fi->path);
                    if(!fm_launch_desktop_entry(ctx, filename, NULL, &err))
                    {
                        if(launcher->error)
                            launcher->error(ctx, err, user_data);
                        g_error_free(err);
                        err = NULL;
                    }
                    continue;
                }
                else if(fm_file_info_is_executable_type(fi))
                {
                    /* if it's an executable file, directly execute it. */
                    filename = fm_path_to_str(fi->path);
                    /* FIXME: we need to use eaccess/euidaccess here. */
                    if(g_file_test(filename, G_FILE_TEST_IS_EXECUTABLE))
                    {
                        app = g_app_info_create_from_commandline(filename, NULL, 0, NULL);
                        if(app)
                        {
                            if(!g_app_info_launch(app, NULL, ctx, &err))
                            {
                                if(launcher->error)
                                    launcher->error(ctx, err, user_data);
                                g_error_free(err);
                                err = NULL;
                            }
                            g_object_unref(app);
                            continue;
                        }
                    }
                    g_free(filename);
                }
            }
            else /* not a native path */
            {
                if(fm_file_info_is_shortcut(fi) && !fm_file_info_is_dir(fi))
                {
                    /* FIXME: special handling for shortcuts */
                    if(fm_path_is_xdg_menu(fi->path) && fi->target)
                    {
                        if(!fm_launch_desktop_entry(ctx, fi->target, NULL, &err))
                        {
                            if(launcher->error)
                                launcher->error(ctx, err, user_data);
                            g_error_free(err);
                            err = NULL;
                        }
                        continue;
                    }
                }
            }
            if(fi->type && fi->type->type)
            {
                fis = g_hash_table_lookup(hash, fi->type->type);
                fis = g_list_prepend(fis, fi);
                g_hash_table_insert(hash, fi->type->type, fis);
            }
        }
    }

    if(g_hash_table_size(hash) > 0)
    {
        GHashTableIter it;
        const char* type;
        GList* fis;
        g_hash_table_iter_init(&it, hash);
        while(g_hash_table_iter_next(&it, &type, &fis))
        {
            GAppInfo* app = g_app_info_get_default_for_type(type, FALSE);
            if(!app)
            {
                if(launcher->get_app)
                {
                    FmMimeType* mime_type = ((FmFileInfo*)fis->data)->type;
                    app = launcher->get_app(fis, mime_type, user_data, NULL);
                }
            }
            if(app)
            {
                for(l=fis; l; l=l->next)
                {
                    char* uri;
                    fi = (FmFileInfo*)l->data;
                    uri = fm_path_to_uri(fi->path);
                    l->data = uri;
                }
                fis = g_list_reverse(fis);
                g_app_info_launch_uris(app, fis, ctx, err);
                /* free URI strings */
                g_list_foreach(fis, (GFunc)g_free, NULL);
                g_object_unref(app);
            }
            g_list_free(fis);
        }
    }
    g_hash_table_destroy(hash);

    if(folders)
    {
        folders = g_list_reverse(folders);
        if(launcher->open_folder)
        {
            launcher->open_folder(ctx, folders, user_data, &err);
            if(err)
            {
                if(launcher->error)
                    launcher->error(ctx, err, user_data);
                g_error_free(err);
                err = NULL;
            }
        }
        g_list_free(folders);
    }
    return TRUE;
}

char* fm_canonicalize_filename(const char* filename, gboolean expand_cwd)
{
    int len = strlen(filename);
    int i = 0;
    char* ret = g_malloc(len + 1), *p = ret;
    for(; i < len; )
    {
        if(filename[i] == '.')
        {
            if(filename[i+1] == '.' && filename[i+2] == '/' || filename[i+2] == '\0' ) /* .. */
            {
                if(i == 0 && expand_cwd) /* .. is first element */
                {
                    const char* cwd = g_get_current_dir();
                    int cwd_len;
                    const char* sep = strrchr(cwd, '/');
                    if(sep && sep != cwd)
                        cwd_len = (sep - cwd);
                    else
                        cwd_len = strlen(cwd);
                    ret = g_realloc(ret, len + cwd_len + 1 - 1);
                    memcpy(ret, cwd, cwd_len);
                    p = ret + cwd_len;
                }
                else /* other .. in the path */
                {
                    --p;
                    if(p > ret && *p == '/') /* strip trailing / if it's not root */
                        --p;
                    while(p > ret && *p != '/') /* strip basename */
                        --p;
                    if(*p != '/' || p == ret) /* strip trailing / if it's not root */
                        ++p;
                }
                i += 2;
                continue;
            }
            else if(filename[i+1] == '/' || filename[i+1] == '\0' ) /* . */
            {
                if(i == 0 && expand_cwd) /* first element */
                {
                    const char* cwd = g_get_current_dir();
                    int cwd_len = strlen(cwd);
                    ret = g_realloc(ret, len + cwd_len + 1);
                    memcpy(ret, cwd, cwd_len);
                    p = ret + cwd_len;
                }
                ++i;
                continue;
            }
        }
        for(; i < len; ++p)
        {
            /* prevent duplicated / */
            if(filename[i] == '/' && (p > ret && *(p-1) == '/'))
            {
                ++i;
                break;
            }
            *p = filename[i];
            ++i;
            if(*p == '/')
            {
                ++p;
                break;
            }
        }
    }
    if((p-1) > ret && *(p-1) == '/') /* strip trailing / */
        --p;
    *p = 0;
    return ret;
}

gboolean fm_launch_paths(GAppLaunchContext* ctx, GList* paths, FmFileLauncher* launcher, gpointer user_data)
{
    FmJob* job = fm_file_info_job_new(NULL);
    GList* l;
    gboolean ret;
    for(l=paths;l;l=l->next)
        fm_file_info_job_add(FM_FILE_INFO_JOB(job), (FmPath*)l->data);
    ret = fm_job_run_sync_with_mainloop(job);
    if(ret)
    {
        GList* file_infos = fm_list_peek_head_link(FM_FILE_INFO_JOB(job)->file_infos);
        if(file_infos)
            ret = fm_launch_files(ctx, file_infos, launcher, user_data);
        else
            ret = FALSE;
    }
    g_object_unref(job);
    return ret;
}
