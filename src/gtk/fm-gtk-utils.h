/*
 *      fm-gtk-utils.h
 *      
 *      Copyright 2009 PCMan <pcman@debian>
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


#ifndef __FM_GTK_UTILS_H__
#define __FM_GTK_UTILS_H__

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <stdarg.h>

#include "fm-path.h"
#include "fm-file-ops-job.h"

G_BEGIN_DECLS

/* Convinient dialog functions */

/* Display an error message to the user */
void fm_show_error(GtkWindow* parent, const char* msg);

/* Ask the user a yes-no question. */
int fm_yes_no(GtkWindow* parent, const char* question);
int fm_ok_cancel(GtkWindow* parent, const char* question);

/* Ask the user a question with a NULL-terminated array of
 * options provided. The return value was index of the selected option. */
int fm_ask(GtkWindow* parent, const char* question, ...);
int fm_askv(GtkWindow* parent, const char* question, const char** options);
int fm_ask_valist(GtkWindow* parent, const char* question, va_list options);

char* fm_get_user_input(GtkWindow* parent, const char* title, const char* msg, const char* default_text);
FmPath* fm_get_user_input_path(GtkWindow* parent, const char* title, const char* msg, FmPath* default_path);

/* Ask the user to select a folder. */
FmPath* fm_select_folder(GtkWindow* parent);

/* Mount */
gboolean fm_mount_path(GtkWindow* parent, FmPath* path);
gboolean fm_mount_volume(GtkWindow* parent, GVolume* vol);

/* File operations */
void fm_copy_files(FmPathList* files, FmPath* dest_dir);
void fm_move_files(FmPathList* files, FmPath* dest_dir);

void fm_move_or_copy_files_to(FmPathList* files, gboolean is_move);
#define fm_move_files_to(files)   fm_move_or_copy_files_to(files, TRUE)
#define fm_copy_files_to(files)   fm_move_or_copy_files_to(files, FALSE)

void fm_trash_files(FmPathList* files);
void fm_delete_files(FmPathList* files);

/* void fm_rename_files(FmPathList* files); */
void fm_rename_file(FmPath* file);

G_END_DECLS

#endif /* __FM_GTK_UTILS_H__ */
