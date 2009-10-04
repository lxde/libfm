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

G_BEGIN_DECLS

typedef (*FmFileMenuFolderHook)(FmFileInfo* fi, gpointer user_data);

typedef struct _FmFileMenu FmFileMenu;
struct _FmFileMenu
{
	FmFileInfoList* file_infos;
	gboolean same_type;
	GtkUIManager* ui;
	GtkActionGroup* act_grp;

	/* <private> */
    gboolean use_trash;
	gboolean auto_destroy;
	GtkWidget* menu;

    FmFileMenuFolderHook folder_hook;
    gpointer folder_hook_data;
};

FmFileMenu* fm_file_menu_new_for_file(FmFileInfo* fi, gboolean auto_destroy);
FmFileMenu* fm_file_menu_new_for_files(FmFileInfoList* files, gboolean auto_destroy);
void fm_file_menu_destroy(FmFileMenu* menu);

gboolean fm_file_menu_is_single_file_type(FmFileMenu* menu);

GtkUIManager* fm_file_menu_get_ui(FmFileMenu* menu);
GtkActionGroup* fm_file_menu_get_action_group(FmFileMenu* menu);

/* build the menu with GtkUIManager */
GtkMenu* fm_file_menu_get_menu(FmFileMenu* menu);

/* call fm_file_info_list_unref() after the returned list is no more needed. */
FmFileInfoList* fm_file_menu_get_file_info_list(FmFileMenu* menu);

void fm_file_menu_set_folder_hook(FmFileMenu* menu, FmFileMenuFolderHook hook, gpointer user_data);

G_END_DECLS

#endif
