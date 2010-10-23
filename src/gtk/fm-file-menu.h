/*
 *      fm-file-menu.h
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

#ifndef __FM_FILE_MENU__
#define __FM_FILE_MENU__

#include <gtk/gtk.h>
#include "fm-file-info.h"
#include "fm-gtk-utils.h"

G_BEGIN_DECLS

typedef struct _FmFileMenu FmFileMenu;
struct _FmFileMenu
{
    FmFileInfoList* file_infos;
    gboolean same_type : 1;
    gboolean same_fs : 1;
    gboolean all_virtual : 1;
    gboolean all_trash : 1;
    gboolean auto_destroy : 1; // private
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkWidget* menu;
    GtkWindow* parent;

    FmLaunchFolderFunc folder_func;
    gpointer folder_func_data;

    FmPath* cwd;
};

FmFileMenu* fm_file_menu_new_for_file(GtkWindow* parent, FmFileInfo* fi, FmPath* cwd, gboolean auto_destroy);
FmFileMenu* fm_file_menu_new_for_files(GtkWindow* parent, FmFileInfoList* files, FmPath* cwd, gboolean auto_destroy);
void fm_file_menu_destroy(FmFileMenu* menu);

gboolean fm_file_menu_is_single_file_type(FmFileMenu* menu);

GtkUIManager* fm_file_menu_get_ui(FmFileMenu* menu);
GtkActionGroup* fm_file_menu_get_action_group(FmFileMenu* menu);

/* build the menu with GtkUIManager */
GtkMenu* fm_file_menu_get_menu(FmFileMenu* menu);

/* call fm_list_ref() if you need to own reference to the returned list. */
FmFileInfoList* fm_file_menu_get_file_info_list(FmFileMenu* menu);

void fm_file_menu_set_folder_func(FmFileMenu* menu, FmLaunchFolderFunc func, gpointer user_data);

G_END_DECLS

#endif
