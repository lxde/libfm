/*
 *      fm-file-launcher.h
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

#ifndef __FM_FILE_LAUNCHER_H__
#define __FM_FILE_LAUNCHER_H__

#include <glib.h>
#include <gio/gio.h>
#include "fm-file-info.h"

G_BEGIN_DECLS

typedef gboolean (*FmLaunchFolderFunc)(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err);

enum _FmFileLauncherExecAction
{
    FM_FILE_LAUNCHER_EXEC = 1,
    FM_FILE_LAUNCHER_EXEC_IN_TERMINAL,
    FM_FILE_LAUNCHER_EXEC_OPEN,
    FM_FILE_LAUNCHER_EXEC_CANCEL
};
typedef enum _FmFileLauncherExecAction FmFileLauncherExecAction;

typedef struct _FmFileLauncher FmFileLauncher;
struct _FmFileLauncher
{
    GAppInfo* (*get_app)(GList* file_infos, FmMimeType* mime_type, gpointer user_data, GError** err);
    /* gboolean (*before_open)(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data); */
    gboolean (*open_folder)(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err);
    FmFileLauncherExecAction (*exec_file)(FmFileInfo* file, gpointer user_data);
    gboolean (*error)(GAppLaunchContext* ctx, GError* err, gpointer user_data);
    int (*ask)(const char* msg, const char** btn_labels, int default_btn, gpointer user_data);
};

gboolean fm_launch_files(GAppLaunchContext* ctx, GList* file_infos, FmFileLauncher* launcher, gpointer user_data);
gboolean fm_launch_paths(GAppLaunchContext* ctx, GList* paths, FmFileLauncher* launcher, gpointer user_data);
gboolean fm_launch_desktop_entry(GAppLaunchContext* ctx, const char* file_or_id, GList* uris, FmFileLauncher* launcher, gpointer user_data);

G_END_DECLS

#endif
