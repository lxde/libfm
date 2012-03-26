/*
 *      fm-file-launcher.c
 *
 *      Copyright 2009 - 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
#include "fm-file-launcher.h"
#include "fm-file-info-job.h"
#include "fm-app-info.h"

static void launch_files(GAppLaunchContext* ctx, GAppInfo* app, GList* file_infos)
{

}

gboolean fm_launch_desktop_entry(GAppLaunchContext* ctx, const char* file_or_id, GList* uris, FmFileLauncher* launcher, gpointer user_data)
{
    gboolean ret = FALSE;
    GAppInfo* app;
    gboolean is_absolute_path = g_path_is_absolute(file_or_id);
    GList* _uris = NULL;
    GError* err = NULL;

    /* Let GDesktopAppInfo try first. */
    if(is_absolute_path)
        app = g_desktop_app_info_new_from_filename(file_or_id);
    else
        app = g_desktop_app_info_new(file_or_id);

    if(!app) /* gio failed loading it. Let's see what's going on */
    {
        gboolean loaded;
        GKeyFile* kf = g_key_file_new();

        /* load the desktop entry file ourselves */
        if(is_absolute_path)
            loaded = g_key_file_load_from_file(kf, file_or_id, 0, &err);
        else
        {
            char* tmp = g_strconcat("applications/", file_or_id, NULL);
            loaded = g_key_file_load_from_data_dirs(kf, tmp, NULL, 0, &err);
            g_free(tmp);
        }
        if(loaded)
        {
            char* type = g_key_file_get_string(kf, G_KEY_FILE_DESKTOP_GROUP, "Type", NULL);
            /* gio only supports "Application" type. Let's handle other types ourselves. */
            if(type)
            {
                if(strcmp(type, "Link") == 0)
                {
                    char* url = g_key_file_get_string(kf, G_KEY_FILE_DESKTOP_GROUP, "URL", &err);
                    if(url)
                    {
                        char* scheme = g_uri_parse_scheme(url);
                        if(scheme)
                        {
                            if(strcmp(scheme, "file") == 0 ||
                               strcmp(scheme, "trash") == 0 ||
                               strcmp(scheme, "network") == 0 ||
                               strcmp(scheme, "computer") == 0 ||
                               strcmp(scheme, "menu") == 0)
                            {
                                /* OK, it's a file. We can handle it! */
                                /* FIXME: prevent recursive invocation of desktop entry file.
                                 * e.g: If this URL points to the another desktop entry file, and it
                                 * points to yet another desktop entry file, this can create a
                                 * infinite loop. This is a extremely rare case. */
                                FmPath* path = fm_path_new_for_uri(url);
                                _uris = g_list_prepend(_uris, path);
                                ret = fm_launch_paths(ctx, _uris, launcher, user_data);
                                g_list_free(_uris);
                                fm_path_unref(path);
                                _uris = NULL;
                            }
                            else
                            {
                                /* Damn! this actually relies on gconf to work. */
                                /* FIXME: use our own way to get a usable browser later. */
                                app = g_app_info_get_default_for_uri_scheme(scheme);
                                uris = _uris = g_list_prepend(NULL, url);
                            }
                            g_free(scheme);
                        }
                    }
                }
                else if(strcmp(type, "Directory") == 0)
                {
                    /* FIXME: how should this work? It's not defined in the spec. :-( */
                }
                g_free(type);
            }
        }
        g_key_file_free(kf);
    }

    if(app)
        ret = fm_app_info_launch_uris(app, uris, ctx, &err);

    if(err)
    {
        if(launcher->error)
            launcher->error(ctx, err, user_data);
        g_error_free(err);
    }

    if(_uris)
    {
        g_list_foreach(_uris, (GFunc)g_free, NULL);
        g_list_free(_uris);
    }

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
        if (launcher->open_folder && fm_file_info_is_dir(fi))
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
                    fm_launch_desktop_entry(ctx, filename, NULL, launcher, user_data);
                    continue;
                }
                else if(fm_file_info_is_executable_type(fi))
                {
                    /* if it's an executable file, directly execute it. */
                    filename = fm_path_to_str(fi->path);
                    /* FIXME: we need to use eaccess/euidaccess here. */
                    if(g_file_test(filename, G_FILE_TEST_IS_EXECUTABLE))
                    {
                        if(launcher->exec_file)
                        {
                            FmFileLauncherExecAction act = launcher->exec_file(fi, user_data);
                            GAppInfoCreateFlags flags = 0;
                            switch(act)
                            {
                            case FM_FILE_LAUNCHER_EXEC_IN_TERMINAL:
                                flags |= G_APP_INFO_CREATE_NEEDS_TERMINAL;
                                /* NOTE: no break here */
                            case FM_FILE_LAUNCHER_EXEC:
                            {
                                /* filename may contain spaces. Fix #3143296 */
                                char* quoted = g_shell_quote(filename);
                                app = fm_app_info_create_from_commandline(quoted, NULL, flags, NULL);
                                g_free(quoted);
                                if(app)
                                {
                                    if(!fm_app_info_launch(app, NULL, ctx, &err))
                                    {
                                        if(launcher->error)
                                            launcher->error(ctx, err, user_data);
                                        g_error_free(err);
                                        err = NULL;
                                    }
                                    g_object_unref(app);
                                    continue;
                                }
                                break;
                            }
                            case FM_FILE_LAUNCHER_EXEC_OPEN:
                                break;
                            case FM_FILE_LAUNCHER_EXEC_CANCEL:
                                continue;
                            }
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
                        fm_launch_desktop_entry(ctx, fi->target, NULL, launcher, user_data);
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
                fm_app_info_launch_uris(app, fis, ctx, err);
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

gboolean fm_launch_paths(GAppLaunchContext* ctx, GList* paths, FmFileLauncher* launcher, gpointer user_data)
{
    FmJob* job = fm_file_info_job_new(NULL, 0);
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
