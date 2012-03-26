/*
 *      fm-gtk-file-launcher.c
 *
 *      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
#include <gio/gdesktopappinfo.h>

#include "fm-gtk-utils.h"
#include "fm-app-chooser-dlg.h"

#include "fm-config.h"

/* for open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* for read() */
#include <unistd.h>

typedef struct _LaunchData LaunchData;
struct _LaunchData
{
    GtkWindow* parent;
    FmLaunchFolderFunc folder_func;
    gpointer user_data;
};

static GAppInfo* choose_app(GList* file_infos, FmMimeType* mime_type, gpointer user_data, GError** err)
{
    LaunchData* data = (LaunchData*)user_data;
    return fm_choose_app_for_mime_type(data->parent, mime_type, mime_type != NULL);
}

static gboolean on_launch_error(GAppLaunchContext* ctx, GError* err, gpointer user_data)
{
    LaunchData* data = (LaunchData*)user_data;
    fm_show_error(data->parent, NULL, err->message);
    return TRUE;
}

static gboolean on_open_folder(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    LaunchData* data = (LaunchData*)user_data;
    if (data->folder_func)
        return data->folder_func(ctx, folder_infos, data->user_data, err);
    else
        return FALSE;
}

static int on_launch_ask(const char* msg, const char** btn_labels, int default_btn, gpointer user_data)
{
    LaunchData* data = (LaunchData*)user_data;
    /* FIXME: set default button properly */
    return fm_askv(data->parent, NULL, msg, btn_labels);
}

static gboolean file_is_executable_script(FmFileInfo* file)
{
    /* We don't execute remote files */
    if(fm_path_is_local(file->path) && fm_file_info_is_text(file) && fm_file_info_get_size(file) > 2)
    {
        /* check if the first two bytes of the file is #! */
        char* path = fm_path_to_str(file->path);
        int fd = open(path, O_RDONLY);
        g_free(path);
        if(fd != -1)
        {
            char buf[4];
            ssize_t r = read(fd, buf, 2);
            close(fd);
            if(r == 2 && buf[0] == '#' && buf[1] == '!')
                return TRUE; /* the file contains #! in first two bytes */
        }
    }
    return FALSE;
}

static FmFileLauncherExecAction on_exec_file(FmFileInfo* file, gpointer user_data)
{
    LaunchData* data = (LaunchData*)user_data;
    GtkBuilder* b = gtk_builder_new();
    GtkWidget* dlg, *msg, *icon;
    char* msg_str;
    int res;
    gtk_builder_set_translation_domain(b, GETTEXT_PACKAGE);
    gtk_builder_add_from_file(b, PACKAGE_UI_DIR "/exec-file.ui", NULL);
    dlg = gtk_builder_get_object(b, "dlg");
    msg = gtk_builder_get_object(b, "msg");
    icon = gtk_builder_get_object(b, "icon");
    gtk_image_set_from_gicon(GTK_IMAGE(icon), file->icon->gicon, GTK_ICON_SIZE_DIALOG);
    gtk_box_set_homogeneous(GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(dlg))), FALSE);

    /* If it's a script, ask the user first. */
    if(file_is_executable_script(file)) /* if this is an executable script file */
    {
        msg_str = g_strdup_printf(_("This text file '%s' seems to be an executable script.\nWhat do you want to do with it?"), fm_file_info_get_disp_name(file));
        gtk_dialog_set_default_response(GTK_DIALOG(dlg), FM_FILE_LAUNCHER_EXEC_IN_TERMINAL);
    }
    else
    {
        GtkWidget* open = gtk_builder_get_object(b, "open");
        gtk_widget_destroy(open);
        msg_str = g_strdup_printf(_("This file '%s' is executable. Do you want to execute it?"), fm_file_info_get_disp_name(file));
        gtk_dialog_set_default_response(GTK_DIALOG(dlg), FM_FILE_LAUNCHER_EXEC);
    }
    gtk_label_set_text(GTK_LABEL(msg), msg_str);
    g_free(msg_str);

    res = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    g_object_unref(b);

    if(res <=0)
        res = FM_FILE_LAUNCHER_EXEC_CANCEL;

    return res;
}

gboolean fm_launch_files_simple(GtkWindow* parent, GAppLaunchContext* ctx, GList* file_infos, FmLaunchFolderFunc func, gpointer user_data)
{
    FmFileLauncher launcher = {
        choose_app,
        on_open_folder,
        on_exec_file,
        on_launch_error,
        on_launch_ask
    };
    LaunchData data = {parent, func, user_data};
    GAppLaunchContext* _ctx = NULL;
    gboolean ret;

    if (!func)
        launcher.open_folder = NULL;

    if(ctx == NULL)
    {
        _ctx = ctx = gdk_app_launch_context_new();
        gdk_app_launch_context_set_screen(GDK_APP_LAUNCH_CONTEXT(ctx), parent ? gtk_widget_get_screen(GTK_WIDGET(parent)) : gdk_screen_get_default());
        gdk_app_launch_context_set_timestamp(GDK_APP_LAUNCH_CONTEXT(ctx), gtk_get_current_event_time());
        /* FIXME: how to handle gdk_app_launch_context_set_icon? */
    }
    ret = fm_launch_files(ctx, file_infos, &launcher, &data);
    if(_ctx)
        g_object_unref(_ctx);
    return ret;
}

gboolean fm_launch_paths_simple(GtkWindow* parent, GAppLaunchContext* ctx, GList* paths, FmLaunchFolderFunc func, gpointer user_data)
{
    FmFileLauncher launcher = {
        choose_app,
        on_open_folder,
        on_launch_error
    };
    LaunchData data = {parent, func, user_data};
    GAppLaunchContext* _ctx = NULL;
    gboolean ret;
    if(ctx == NULL)
    {
        _ctx = ctx = gdk_app_launch_context_new();
        gdk_app_launch_context_set_screen(GDK_APP_LAUNCH_CONTEXT(ctx), parent ? gtk_widget_get_screen(GTK_WIDGET(parent)) : gdk_screen_get_default());
        gdk_app_launch_context_set_timestamp(GDK_APP_LAUNCH_CONTEXT(ctx), gtk_get_current_event_time());
        /* FIXME: how to handle gdk_app_launch_context_set_icon? */
    }
    ret = fm_launch_paths(ctx, paths, &launcher, &data);
    if(_ctx)
        g_object_unref(_ctx);
    return ret;
}

gboolean fm_launch_file_simple(GtkWindow* parent, GAppLaunchContext* ctx, FmFileInfo* file_info, FmLaunchFolderFunc func, gpointer user_data)
{
    gboolean ret;
    GList* files = g_list_prepend(NULL, file_info);
    ret = fm_launch_files_simple(parent, ctx, files, func, user_data);
    g_list_free(files);
    return ret;
}

gboolean fm_launch_path_simple(GtkWindow* parent, GAppLaunchContext* ctx, FmPath* path, FmLaunchFolderFunc func, gpointer user_data)
{
    gboolean ret;
    GList* files = g_list_prepend(NULL, path);
    ret = fm_launch_paths_simple(parent, ctx, files, func, user_data);
    g_list_free(files);
    return ret;
}
