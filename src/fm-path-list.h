/*
 *      fm-path-list.h
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

#ifndef __FM_PATH_LIST_H__
#define __FM_PATH_LIST_H__

#include <glib.h>
#include "fm-list.h"
#include "fm-path.h"
#include "fm-file-info.h"

G_BEGIN_DECLS

typedef FmList FmPathList;

FmPathList* fm_path_list_new();
FmPathList* fm_path_list_new_from_uri_list(const char* uri_list);
FmPathList* fm_path_list_new_from_file_info_list(GList* fis);
FmPathList* fm_path_list_new_from_file_info_slist(GSList* fis);

char* fm_path_list_to_uri_list(FmPathList* pl);
guint fm_path_list_get_length(FmPathList* pl);

G_END_DECLS

#endif

