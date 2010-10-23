/*
 *      fm-file-properties.c
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

#ifndef __FM_FILE_PROPERTIES_H__
#define __FM_FILE_PROPERTIES_H__

#include <gtk/gtk.h>
#include "fm-file-info.h"

G_BEGIN_DECLS

/* FIXME: use list of FmFileInfo is much better here. */
GtkWidget* fm_file_properties_widget_new(FmFileInfoList* files, gboolean toplevel);
gboolean fm_show_file_properties(GtkWindow* parent, FmFileInfoList* files);

G_END_DECLS

#endif
