/*
 *      fm-file-info.h
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

#ifndef _FM_FILE_INFO_H_
#define _FM_FILE_INFO_H_

#include <glib.h>
#include <gio/gio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fm-icon.h"
#include "fm-list.h"
#include "fm-path.h"
#include "fm-mime-type.h"

G_BEGIN_DECLS

/* Some flags are defined for future use and are not supported now */
enum _FmFileInfoFlag
{
    FM_FILE_INFO_NONE = 0,
    FM_FILE_INFO_HOME_DIR = (1 << 0),
    FM_FILE_INFO_DESKTOP_DIR = (1 << 1),
    FM_FILE_INFO_DESKTOP_ENTRY = (1 << 2),
    FM_FILE_INFO_MOUNT_POINT = (1 << 3),
    FM_FILE_INFO_REMOTE = (1 << 4),
    FM_FILE_INFO_VIRTUAL = (1 << 5)
};
typedef enum _FmFileInfoFlag FmFileInfoFlag;

typedef struct _FmFileInfo FmFileInfo;
typedef FmList FmFileInfoList;

/* intialize the file info system */
void _fm_file_info_init();
void _fm_file_info_finalize();

FmFileInfo* fm_file_info_new();
FmFileInfo* fm_file_info_new_from_gfileinfo(FmPath* path, GFileInfo* inf);
void fm_file_info_set_from_gfileinfo(FmFileInfo* fi, GFileInfo* inf);

gboolean fm_file_info_set_from_native_file(FmFileInfo* fi, const char* path, GError** err);

FmFileInfo* fm_file_info_ref( FmFileInfo* fi );
void fm_file_info_unref( FmFileInfo* fi );

void fm_file_info_copy(FmFileInfo* fi, FmFileInfo* src);

/** returned FmPath shouldn't be unref by caller */
FmPath* fm_file_info_get_path( FmFileInfo* fi );
const char* fm_file_info_get_name( FmFileInfo* fi );
const char* fm_file_info_get_disp_name( FmFileInfo* fi );

void fm_file_info_set_path(FmFileInfo* fi, FmPath* path);
void fm_file_info_set_disp_name( FmFileInfo* fi, const char* name );

goffset fm_file_info_get_size( FmFileInfo* fi );
const char* fm_file_info_get_disp_size( FmFileInfo* fi );

goffset fm_file_info_get_blocks( FmFileInfo* fi );

mode_t fm_file_info_get_mode( FmFileInfo* fi );
gboolean fm_file_info_is_dir( FmFileInfo* fi );

FmMimeType* fm_file_info_get_mime_type( FmFileInfo* fi );

gboolean fm_file_info_is_dir( FmFileInfo* fi );

gboolean fm_file_info_is_symlink( FmFileInfo* fi );

gboolean fm_file_info_is_shortcut( FmFileInfo* fi );

gboolean fm_file_info_is_mountable( FmFileInfo* fi );

gboolean fm_file_info_is_image( FmFileInfo* fi );

gboolean fm_file_info_is_text( FmFileInfo* fi );

gboolean fm_file_info_is_desktop_entry( FmFileInfo* fi );

gboolean fm_file_info_is_unknown_type( FmFileInfo* fi );

gboolean fm_file_info_is_hidden(FmFileInfo* fi);

/* if the mime-type is executable, such as shell script, python script, ... */
gboolean fm_file_info_is_executable_type( FmFileInfo* fi);

const char* fm_file_info_get_target( FmFileInfo* fi );

const char* fm_file_info_get_collate_key( FmFileInfo* fi );
const char* fm_file_info_get_desc( FmFileInfo* fi );
const char* fm_file_info_get_disp_mtime( FmFileInfo* fi );
time_t fm_file_info_get_mtime( FmFileInfo* fi );
time_t fm_file_info_get_atime( FmFileInfo* fi );
FmIcon* fm_file_info_get_icon( FmFileInfo* fi );
uid_t fm_file_info_get_uid( FmFileInfo* fi );
gid_t fm_file_info_get_gid( FmFileInfo* fi );
const char* fm_file_info_get_fs_id( FmFileInfo* fi );
dev_t fm_file_info_get_dev( FmFileInfo* fi );

gboolean fm_file_info_can_thumbnail(FmFileInfo* fi);

FmFileInfoList* fm_file_info_list_new();
FmFileInfoList* fm_file_info_list_new_from_glist();

gboolean fm_list_is_file_info_list(FmList* list);

/* return TRUE if all files in the list are of the same type */
gboolean fm_file_info_list_is_same_type(FmFileInfoList* list);

/* return TRUE if all files in the list are on the same fs */
gboolean fm_file_info_list_is_same_fs(FmFileInfoList* list);

#define FM_FILE_INFO(ptr)    ((FmFileInfo*)ptr)

G_END_DECLS

#endif
