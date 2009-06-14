/*
*  C Interface: vfs-file-info
*
* Description: File information
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _FM_FILE_INFO_H_
#define _FM_FILE_INFO_H_

#include <glib.h>
#include <gio/gio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fm-path.h"
#include "fm-mime-type.h"

G_BEGIN_DECLS

typedef enum
{
    FM_FILE_INFO_NONE = 0,
    FM_FILE_INFO_HOME_DIR = (1 << 0),
    FM_FILE_INFO_DESKTOP_DIR = (1 << 1),
    FM_FILE_INFO_DESKTOP_ENTRY = (1 << 2),
    FM_FILE_INFO_MOUNT_POINT = (1 << 3),
    FM_FILE_INFO_REMOTE = (1 << 4),
    FM_FILE_INFO_VIRTUAL = (1 << 5)
}FmFileInfoFlag;   /* For future use, not all supported now */

typedef struct _FmFileInfo FmFileInfo;

struct _FmFileInfo
{
    mode_t mode;
    dev_t dev;
    uid_t uid;
    gid_t gid;
    goffset size;
    time_t mtime;
    time_t atime;

    gulong blksize;
    goffset blocks;

    FmPath* path; /* path of the file */
    char* disp_name;  /* displayed name (in UTF-8) */
    char* disp_size;  /* displayed human-readable file size */
    char* disp_mtime; /* displayed last modification time */
	FmMimeType* type;
	GIcon* icon;

    /*<private>*/
    int n_ref;
};

FmFileInfo* fm_file_info_new();
FmFileInfo* fm_file_info_new_from_gfileinfo(FmPath* parent_dir, GFileInfo* inf);
FmFileInfo* fm_file_info_ref( FmFileInfo* fi );
void fm_file_info_unref( FmFileInfo* fi );

FmPath* fm_file_info_get_path( FmFileInfo* fi );
const char* fm_file_info_get_name( FmFileInfo* fi );
const char* fm_file_info_get_disp_name( FmFileInfo* fi );

void fm_file_info_set_disp_name( FmFileInfo* fi, const char* name );

goffset fm_file_info_get_size( FmFileInfo* fi );
const char* fm_file_info_get_disp_size( FmFileInfo* fi );

goffset fm_file_info_get_blocks( FmFileInfo* fi );

mode_t fm_file_info_get_mode( FmFileInfo* fi );
gboolean fm_file_info_is_dir( FmFileInfo* fi );

FmMimeType* fm_file_info_get_mime_type( FmFileInfo* fi );

gboolean fm_file_info_is_dir( FmFileInfo* fi );

gboolean fm_file_info_is_symlink( FmFileInfo* fi );

gboolean fm_file_info_is_image( FmFileInfo* fi );

gboolean fm_file_info_is_desktop_entry( FmFileInfo* fi );

gboolean fm_file_info_is_unknown_type( FmFileInfo* fi );

/* Full path of the file is required by this function */
gboolean fm_file_info_is_executable( FmFileInfo* fi, const char* file_path );

const char* fm_file_info_get_desc( FmFileInfo* fi );
const char* fm_file_info_get_disp_mtime( FmFileInfo* fi );
time_t* fm_file_info_get_mtime( FmFileInfo* fi );
time_t* fm_file_info_get_atime( FmFileInfo* fi );

#if 0
gboolean fm_file_info_get( FmFileInfo* fi,
                            const char* file_path,
                            const char* base_name );

void fm_file_info_reload_mime_type( FmFileInfo* fi,
                                     const char* full_path );


const char* fm_file_info_get_disp_owner( FmFileInfo* fi );
const char* fm_file_info_get_disp_perm( FmFileInfo* fi );


void fm_file_info_set_thumbnail_size( int big, int small );
gboolean fm_file_info_load_thumbnail( FmFileInfo* fi,
                                       const char* full_path,
                                       gboolean big );
gboolean fm_file_info_is_thumbnail_loaded( FmFileInfo* fi,
                                            gboolean big );

GdkPixbuf* fm_file_info_get_big_icon( FmFileInfo* fi );
GdkPixbuf* fm_file_info_get_small_icon( FmFileInfo* fi );

GdkPixbuf* fm_file_info_get_big_thumbnail( FmFileInfo* fi );
GdkPixbuf* fm_file_info_get_small_thumbnail( FmFileInfo* fi );



/* Full path of the file is required by this function */
gboolean fm_file_info_is_text( FmFileInfo* fi, const char* file_path );

/*
* Run default action of specified file.
* Full path of the file is required by this function.
*/
gboolean fm_file_info_open_file( FmFileInfo* fi,
                                  const char* file_path,
                                  GError** err );

void fm_file_info_load_special_info( FmFileInfo* fi,
                                      const char* file_path );


/* resolve file path name */
char* vfs_file_resolve_path( const char* cwd, const char* relative_path );

#endif

void fm_file_info_list_free( GList* list );

G_END_DECLS

#endif
