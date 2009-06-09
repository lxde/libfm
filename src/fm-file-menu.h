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

GtkWidget* fm_file_menu_new_for_file(FmFileInfo* fi);

GtkWidget* fm_file_menu_new_for_files(GList* files);

G_END_DECLS

#endif
