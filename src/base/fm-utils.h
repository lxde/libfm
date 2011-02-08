/*
 *      fm-utils.h
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

#ifndef __FM_UTILS_H__
#define __FM_UTILS_H__

#include <glib.h>
#include <gio/gio.h>
#include "fm-file-info.h"

G_BEGIN_DECLS

char* fm_file_size_to_str( char* buf, goffset size, gboolean si_prefix );

gboolean fm_key_file_get_int(GKeyFile* kf, const char* grp, const char* key, int* val);
gboolean fm_key_file_get_bool(GKeyFile* kf, const char* grp, const char* key, gboolean* val);

char* fm_canonicalize_filename(const char* filename, const char* cwd);

char* fm_str_replace(char* str, char* old, char* new);

G_END_DECLS

#endif
