/*
 *      fm-path.h
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


#ifndef __FM_PATH_H__
#define __FM_PATH_H__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _FmPath FmPath;
typedef enum _FmPathFlags FmPathFlags;

enum _FmPathFlags
{
	FM_PATH_NONE,
	FM_PATH_IS_URI = 1<<0,
	FM_PATH_IS_REMOTE = 1<<1,
	FM_PATH_IS_VIRTUAL = 1<<2,
	FM_PATH_IS_TRASH = 1<<4,
	FM_PATH_MOUNTABLE = 1<<8,
};

struct _FmPath
{
	gint n_ref;
	FmPathFlags flags : 8;
	FmPath* parent;
	char name[1];
};

void fm_path_init();

FmPath*	fm_path_new(const char* path);
FmPath*	fm_path_new_child(FmPath* parent, const char* basename);
FmPath*	fm_path_new_child_len(FmPath* parent, const char* basename, int name_len);
FmPath*	fm_path_new_relative(FmPath* parent, const char* relative_path);
//FmPath*	fm_path_new_relative_len(FmPath* parent, const char* relative_path, int len);
FmPath* fm_path_new_for_gfile(GFile* gf);

/* predefined paths */
FmPath* fm_path_get_root();
FmPath* fm_path_get_home();
FmPath* fm_path_get_desktop();
FmPath* fm_path_get_trash();

FmPath*	fm_path_ref(FmPath* path);
void fm_path_unref(FmPath* path);

FmPath* fm_path_get_parent(FmPath* path);
const char* fm_path_get_basename(FmPath* path);
FmPathFlags fm_path_get_flags(FmPath* path);
gboolean fm_path_is_native(FmPath* path);

char* fm_path_to_str(FmPath* path);
char* fm_path_to_uri(FmPath* path);
GFile* fm_path_to_gfile(FmPath* path);

G_END_DECLS

#endif /* __FM_PATH_H__ */
