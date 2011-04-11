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

#include "fm-list.h"

G_BEGIN_DECLS

#define FM_PATH(path)   ((FmPath*)path)

typedef struct _FmPath FmPath;
typedef FmList FmPathList;

enum _FmPathFlags
{
    FM_PATH_NONE = 0,
    FM_PATH_IS_NATIVE = 1<<0, /* This is a native path to UNIX, like /home */
    FM_PATH_IS_LOCAL = 1<<1, /* This path refers  to a file on local filesystem */
    FM_PATH_IS_VIRTUAL = 1<<2, /* This path is virtual and it doesn't exist on real filesystem */
    FM_PATH_IS_TRASH = 1<<3, /* This path is under trash:/// */
    FM_PATH_IS_XDG_MENU = 1<<4, /* This path is under menu:/// */

    /* reserved for future use */
    FM_PATH_IS_RESERVED1 = 1<<5,
    FM_PATH_IS_RESERVED2 = 1<<6,
    FM_PATH_IS_RESERVED3 = 1<<7,
};
typedef enum _FmPathFlags FmPathFlags;

struct _FmPath
{
    gint n_ref;
    FmPath* parent;
    guchar flags; /* FmPathFlags flags : 8; */
    char name[1];
};

void _fm_path_init();

/* fm_path_new is deprecated. Use fm_path_new_for_str */
#define fm_path_new(path)   fm_path_new_for_str(path)

FmPath* fm_path_new_for_path(const char* path_name);
FmPath* fm_path_new_for_uri(const char* uri);
FmPath* fm_path_new_for_display_name(const char* path_name);
FmPath* fm_path_new_for_str(const char* path_str);
FmPath* fm_path_new_for_commandline_arg(const char* arg);

FmPath* fm_path_new_child(FmPath* parent, const char* basename);
FmPath* fm_path_new_child_len(FmPath* parent, const char* basename, int name_len);
FmPath* fm_path_new_relative(FmPath* parent, const char* relative_path);
FmPath* fm_path_new_for_gfile(GFile* gf);

/* predefined paths */
FmPath* fm_path_get_root(); /* / */
FmPath* fm_path_get_home(); /* home directory */
FmPath* fm_path_get_desktop(); /* $HOME/Desktop */
FmPath* fm_path_get_trash(); /* trash:/// */
FmPath* fm_path_get_apps_menu(); /* menu://applications.menu/ */

FmPath* fm_path_ref(FmPath* path);
void fm_path_unref(FmPath* path);

FmPath* fm_path_get_parent(FmPath* path);
const char* fm_path_get_basename(FmPath* path);
FmPathFlags fm_path_get_flags(FmPath* path);
gboolean fm_path_has_prefix(FmPath* path, FmPath* prefix);

#define fm_path_is_native(path) (fm_path_get_flags(path)&FM_PATH_IS_NATIVE)
#define fm_path_is_trash(path) (fm_path_get_flags(path)&FM_PATH_IS_TRASH)
#define fm_path_is_trash_root(path) (path == fm_path_get_trash())
#define fm_path_is_virtual(path) (fm_path_get_flags(path)&FM_PATH_IS_VIRTUAL)
#define fm_path_is_local(path) (fm_path_get_flags(path)&FM_PATH_IS_LOCAL)
#define fm_path_is_xdg_menu(path) (fm_path_get_flags(path)&FM_PATH_IS_XDG_MENU)

char* fm_path_to_str(FmPath* path);
char* fm_path_to_uri(FmPath* path);
GFile* fm_path_to_gfile(FmPath* path);

char* fm_path_display_name(FmPath* path, gboolean human_readable);
char* fm_path_display_basename(FmPath* path);

/* For used in hash tables */
guint fm_path_hash(FmPath* path);
gboolean fm_path_equal(FmPath* p1, FmPath* p2);

/* used for completion in fm_path_entry */
gboolean fm_path_equal_str(FmPath *path, const gchar *str, int n);

/* calculate how many elements are in this path. */
int fm_path_depth(FmPath* path);

/* path list */
FmPathList* fm_path_list_new();
FmPathList* fm_path_list_new_from_uri_list(const char* uri_list);
FmPathList* fm_path_list_new_from_uris(const char** uris);
FmPathList* fm_path_list_new_from_file_info_list(FmList* fis);
FmPathList* fm_path_list_new_from_file_info_glist(GList* fis);
FmPathList* fm_path_list_new_from_file_info_gslist(GSList* fis);

gboolean fm_list_is_path_list(FmList* list);

char* fm_path_list_to_uri_list(FmPathList* pl);
/* char** fm_path_list_to_uris(FmPathList* pl); */
void fm_path_list_write_uri_list(FmPathList* pl, GString* buf);

G_END_DECLS

#endif /* __FM_PATH_H__ */
