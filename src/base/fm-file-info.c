/*
 *      fm-file-info.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-file-info.h"
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <grp.h> /* Query group name */
#include <pwd.h> /* Query user name */
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fm-utils.h"
#include <menu-cache.h>

static gboolean use_si_prefix = TRUE;
static FmMimeType* desktop_entry_type = NULL;
static FmMimeType* shortcut_type = NULL;
static FmMimeType* mountable_type = NULL;

/* intialize the file info system */
void _fm_file_info_init()
{
    fm_mime_type_init();
    desktop_entry_type = fm_mime_type_get_for_type("application/x-desktop");

    /* fake mime-types for mountable and shortcuts */
    shortcut_type = fm_mime_type_get_for_type("inode/x-shortcut");
    shortcut_type->description = g_strdup(_("Shortcuts"));

    mountable_type = fm_mime_type_get_for_type("inode/x-mountable");
    mountable_type->description = g_strdup(_("Mount Point"));
}

void _fm_file_info_finalize()
{

}

FmFileInfo* fm_file_info_new ()
{
    FmFileInfo * fi = g_slice_new0( FmFileInfo );
    fi->n_ref = 1;
    return fi;
}

void fm_file_info_set_from_gfileinfo(FmFileInfo* fi, GFileInfo* inf)
{
    const char* tmp;
    GIcon* gicon;
    GFileType type;

    g_return_if_fail(fi->path);

    /* if display name is the same as its name, just use it. */
    tmp = g_file_info_get_display_name(inf);
    if( strcmp(tmp, fi->path->name) == 0 )
        fi->disp_name = fi->path->name;
    else
        fi->disp_name = g_strdup(tmp);

    fi->size = g_file_info_get_size(inf);

    tmp = g_file_info_get_content_type(inf);
    if( tmp )
        fi->type = fm_mime_type_get_for_type( tmp );

    fi->mode = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_MODE);

    fi->uid = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_UID);
    fi->gid = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_GID);

    type = g_file_info_get_file_type(inf);
    if( 0 == fi->mode ) /* if UNIX file mode is not available, compose a fake one. */
    {
        switch(type)
        {
        case G_FILE_TYPE_REGULAR:
            fi->mode |= S_IFREG;
            break;
        case G_FILE_TYPE_DIRECTORY:
            fi->mode |= S_IFDIR;
            break;
        case G_FILE_TYPE_SYMBOLIC_LINK:
            fi->mode |= S_IFLNK;
            break;
        case G_FILE_TYPE_SHORTCUT:
            break;
        case G_FILE_TYPE_MOUNTABLE:
            break;
        }

        /* if it's a special file but it doesn't have UNIX mode, compose a fake one. */
        if(type == G_FILE_TYPE_SPECIAL && 0 == fi->mode)
        {
            if(strcmp(tmp, "inode/chardevice")==0)
                fi->mode |= S_IFCHR;
            else if(strcmp(tmp, "inode/blockdevice")==0)
                fi->mode |= S_IFBLK;
            else if(strcmp(tmp, "inode/fifo")==0)
                fi->mode |= S_IFIFO;
        #ifdef S_IFSOCK
            else if(strcmp(tmp, "inode/socket")==0)
                fi->mode |= S_IFSOCK;
        #endif
        }
    }

    /* set file icon according to mime-type */
    if( !fi->type || !fi->type->icon )
    {
        gicon = g_file_info_get_icon(inf);
        fi->icon = fm_icon_from_gicon(gicon);
        /* g_object_unref(gicon); this is not needed since
         * g_file_info_get_icon didn't increase ref_count.
         * the object returned by g_file_info_get_icon is
         * owned by GFileInfo. */
    }
    else
        fi->icon = fm_icon_ref(fi->type->icon);

    if( type == G_FILE_TYPE_MOUNTABLE || G_FILE_TYPE_SHORTCUT )
    {
        const char* uri = g_file_info_get_attribute_string(inf, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
        if( uri )
        {
            if(g_str_has_prefix(uri, "file:/"))
                fi->target = g_filename_from_uri(uri, NULL, NULL);
            else
                fi->target = g_strdup(uri);
        }

        if( !fi->type )
        {
            /* FIXME: is this appropriate? */
            if( type == G_FILE_TYPE_SHORTCUT )
                fi->type = fm_mime_type_ref(shortcut_type);
            else
                fi->type = fm_mime_type_ref(mountable_type);
        }
        /* FIXME: how about target of symlinks? */
    }

    if(fm_path_is_native(fi->path))
    {
        fi->dev = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_DEVICE);
    }
    else
    {
        tmp = g_file_info_get_attribute_string(inf, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
        fi->fs_id = g_intern_string(tmp);
    }

    fi->mtime = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    fi->atime = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_TIME_ACCESS);
}

FmFileInfo* fm_file_info_new_from_gfileinfo(FmPath* path, GFileInfo* inf)
{
    FmFileInfo* fi = fm_file_info_new();
    fi->path = fm_path_ref(path);
    fm_file_info_set_from_gfileinfo(fi, inf);
    return fi;
}

void _fm_file_info_set_from_menu_cache_item(FmFileInfo* fi, MenuCacheItem* item)
{
    const char* icon_name = menu_cache_item_get_icon(item);
    fi->disp_name = g_strdup(menu_cache_item_get_name(item));
    if(icon_name)
    {
        char* tmp_name = NULL;
        if(icon_name[0] != '/') /* this is a icon name, not a full path to icon file. */
        {
            char* dot = strrchr(icon_name, '.');
            /* remove file extension, this is a hack to fix non-standard desktop entry files */
            if(G_UNLIKELY(dot))
            {
                ++dot;
                if(strcmp(dot, "png") == 0 ||
                   strcmp(dot, "svg") == 0 ||
                   strcmp(dot, "xpm") == 0)
                {
                    tmp_name = g_strndup(icon_name, dot - icon_name - 1);
                    icon_name = tmp_name;
                }
            }
        }
        fi->icon = fm_icon_from_name(icon_name);
        if(G_UNLIKELY(tmp_name))
            g_free(tmp_name);
    }
    if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_DIR)
    {
        fi->mode |= S_IFDIR;
    }
    else if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP)
    {
        fi->mode |= S_IFREG;
        fi->target = menu_cache_item_get_file_path(item);
    }
    fi->type = fm_mime_type_ref(shortcut_type);
}

FmFileInfo* _fm_file_info_new_from_menu_cache_item(FmPath* path, MenuCacheItem* item)
{
    FmFileInfo* fi = fm_file_info_new();
    fi->path = fm_path_ref(path);
    _fm_file_info_set_from_menu_cache_item(fi, item);
    return fi;
}

static void fm_file_info_clear( FmFileInfo* fi )
{
    if( fi->collate_key )
    {
        if( fi->collate_key != fi->disp_name )
            g_free(fi->collate_key);
        fi->collate_key = NULL;
    }

    if( fi->path )
    {
        if(G_LIKELY(fi->disp_name) && fi->disp_name != fi->path->name)
        {
            g_free( fi->disp_name );
            fi->disp_name = NULL;
        }

        fm_path_unref(fi->path);
        fi->path = NULL;
    }
    else
    {
        if(fi->disp_name)
        {
            g_free(fi->disp_name);
            fi->disp_name = NULL;
        }
    }

    if( fi->disp_size )
    {
        g_free( fi->disp_size );
        fi->disp_size = NULL;
    }

    g_free(fi->target);

    if( fi->type )
    {
        fm_mime_type_unref(fi->type);
        fi->type = NULL;
    }
    if( fi->icon )
    {
        fm_icon_unref(fi->icon);
        fi->icon = NULL;
    }
}

FmFileInfo* fm_file_info_ref( FmFileInfo* fi )
{
    g_atomic_int_inc( &fi->n_ref );
    return fi;
}

void fm_file_info_unref( FmFileInfo* fi )
{
    /* g_debug("unref file info: %d", fi->n_ref); */
    if ( g_atomic_int_dec_and_test( &fi->n_ref) )
    {
        fm_file_info_clear( fi );
        g_slice_free( FmFileInfo, fi );
    }
}

void fm_file_info_copy(FmFileInfo* fi, FmFileInfo* src)
{
    FmPath* tmp_path = fm_path_ref(src->path);
    FmMimeType* tmp_type = fm_mime_type_ref(src->type);
    FmIcon* tmp_icon = fm_icon_ref(src->icon);
    /* NOTE: we need to ref source first. Otherwise,
     * if path, mime_type, and icon are identical in src
     * and fi, calling fm_file_info_clear() first on fi
     * might unref that. */
    fm_file_info_clear(fi);
    fi->path = tmp_path;
    fi->type = tmp_type;
    fi->icon = tmp_icon;

    fi->mode = src->mode;
    if(fm_path_is_native(fi->path))
        fi->dev = src->dev;
    else
        fi->fs_id = src->fs_id;
    fi->uid = src->uid;
    fi->gid = src->gid;
    fi->size = src->size;
    fi->mtime = src->mtime;
    fi->atime = src->atime;

    fi->blksize = src->blksize;
    fi->blocks = src->blocks;

    if(src->disp_name == src->path->name)
        fi->disp_name = src->disp_name;
    else
        fi->disp_name = g_strdup(src->disp_name);

    fi->collate_key = g_strdup(src->collate_key);
    fi->disp_size = g_strdup(src->disp_size);
    fi->disp_mtime = g_strdup(src->disp_mtime);
    fi->type = fm_mime_type_ref(src->type);
    fi->icon = fm_icon_ref(src->icon);
}

FmPath* fm_file_info_get_path( FmFileInfo* fi )
{
    return fi->path;
}

const char* fm_file_info_get_name( FmFileInfo* fi )
{
    return fi->path->name;
}

/* Get displayed name encoded in UTF-8 */
const char* fm_file_info_get_disp_name( FmFileInfo* fi )
{
    if(G_UNLIKELY(!fi->disp_name))
    {
        /* FIXME: this is not guaranteed to be UTF-8.
         * Encoding conversion is needed here. */
        return fi->path->name;
    }
    return fi->disp_name;
}

void fm_file_info_set_path(FmFileInfo* fi, FmPath* path)
{
    if(fi->path)
    {
        if(fi->path->name == fi->disp_name)
            fi->disp_name = NULL;
        fm_path_unref(fi->path);
    }

    if(path)
    {
        fi->path = fm_path_ref(path);
        /* FIXME: need to handle UTF-8 issue here */
        fi->disp_name = fi->path->name;
    }
    else
        fi->path = NULL;
}

void fm_file_info_set_disp_name( FmFileInfo* fi, const char* name )
{
    if ( fi->disp_name && fi->disp_name != fi->path->name )
        g_free( fi->disp_name );
    fi->disp_name = g_strdup( name );
}

goffset fm_file_info_get_size( FmFileInfo* fi )
{
    return fi->size;
}

const char* fm_file_info_get_disp_size( FmFileInfo* fi )
{
    if ( G_UNLIKELY( !fi->disp_size ) )
    {
        if( S_ISREG(fi->mode) )
        {
            char buf[ 64 ];
            fm_file_size_to_str( buf, fi->size, use_si_prefix );
            fi->disp_size = g_strdup( buf );
        }
    }
    return fi->disp_size;
}

goffset fm_file_info_get_blocks( FmFileInfo* fi )
{
    return fi->blocks;
}

FmMimeType* fm_file_info_get_mime_type( FmFileInfo* fi )
{
    return fi->type ? fm_mime_type_ref(fi->type) : NULL;
}

mode_t fm_file_info_get_mode( FmFileInfo* fi )
{
    return fi->mode;
}

gboolean fm_file_info_is_dir( FmFileInfo* fi )
{
    return (S_ISDIR( fi->mode ) ||
        (S_ISLNK( fi->mode ) && (0 == strcmp( fi->type->type, "inode/directory" ))));
}

gboolean fm_file_info_is_symlink( FmFileInfo* fi )
{
    return S_ISLNK( fi->mode ) ? TRUE : FALSE;
}

gboolean fm_file_info_is_shortcut(FmFileInfo* fi)
{
    return fi->type == shortcut_type;
}

gboolean fm_file_info_is_mountable(FmFileInfo* fi)
{
    return fi->type == mountable_type;
}

gboolean fm_file_info_is_image( FmFileInfo* fi )
{
    /* FIXME: We had better use functions of xdg_mime to check this */
    if ( ! strncmp( "image/", fi->type->type, 6 ) )
        return TRUE;
    return FALSE;
}

gboolean fm_file_info_is_text( FmFileInfo* fi )
{
    if(g_content_type_is_a(fi->type->type, "text/plain"))
        return TRUE;
    return FALSE;
}

gboolean fm_file_info_is_desktop_entry( FmFileInfo* fi )
{
    return fi->type == desktop_entry_type;
    /* return g_strcmp0(fi->type->type, "application/x-desktop") == 0; */
}

gboolean fm_file_info_is_unknown_type( FmFileInfo* fi )
{
    return g_content_type_is_unknown(fi->type->type);
}

/* full path of the file is required by this function */
gboolean fm_file_info_is_executable_type( FmFileInfo* fi )
{
    // FIXME: didn't check access rights.
//    return mime_type_is_executable_file( file_path, fi->type->type );
    return g_content_type_can_be_executable(fi->type->type);
}

gboolean fm_file_info_is_hidden(FmFileInfo* fi)
{
    const char* name = fi->path->name;
    /* files with . prefix or ~ suffix are regarded as hidden files.
     * dirs with . prefix are regarded as hidden dirs. */
    return (name[0] == '.' ||
       (!fm_file_info_is_dir(fi) && g_str_has_suffix(name, "~")) );
}

gboolean fm_file_info_can_thumbnail(FmFileInfo* fi)
{
    /* We cannot use S_ISREG here as this exclude all symlinks */
    if( !(fi->mode & S_IFREG) ||
        fm_file_info_is_desktop_entry(fi) ||
        fm_file_info_is_unknown_type(fi))
        return FALSE;
    return TRUE;
}

const char* fm_file_info_get_collate_key( FmFileInfo* fi )
{
    if( G_UNLIKELY(!fi->collate_key) )
    {
        char* casefold = g_utf8_casefold(fi->disp_name, -1);
        char* collate = g_utf8_collate_key_for_filename(casefold, -1);
        g_free(casefold);
        if( strcmp(collate, fi->disp_name) )
            fi->collate_key = collate;
        else
        {
            fi->collate_key = fi->disp_name;
            g_free(collate);
        }
    }
    return fi->collate_key;
}

const char* fm_file_info_get_target( FmFileInfo* fi )
{
    return fi->target;
}

const char* fm_file_info_get_desc( FmFileInfo* fi )
{
    /* FIXME: how to handle descriptions for virtual files without mime-tyoes? */
    return fi->type ? fm_mime_type_get_desc( fi->type ) : NULL;
}

const char* fm_file_info_get_disp_mtime( FmFileInfo* fi )
{
    /* FIXME: This can cause problems if the file really has mtime=0. */
    /*        We'd better hide mtime for virtual files only. */
    if(fi->mtime > 0)
    {
        if ( ! fi->disp_mtime )
        {
            char buf[ 128 ];
            strftime( buf, sizeof( buf ),
                      "%x %R",
                      localtime( &fi->mtime ) );
            fi->disp_mtime = g_strdup( buf );
        }
    }
    return fi->disp_mtime;
}

time_t* fm_file_info_get_mtime( FmFileInfo* fi )
{
    return &fi->mtime;
}

time_t* fm_file_info_get_atime( FmFileInfo* fi )
{
    return &fi->atime;
}

static FmListFuncs fm_list_funcs =
{
    fm_file_info_ref,
    fm_file_info_unref
};

FmFileInfoList* fm_file_info_list_new()
{
    return fm_list_new(&fm_list_funcs);
}

gboolean fm_list_is_file_info_list(FmList* list)
{
    return list->funcs == &fm_list_funcs;
}

/* return TRUE if all files in the list are of the same type */
gboolean fm_file_info_list_is_same_type(FmFileInfoList* list)
{
    /* FIXME: handle virtual files without mime-types */
    if( ! fm_list_is_empty(list) )
    {
        GList* l = fm_list_peek_head_link(list);
        FmFileInfo* fi = (FmFileInfo*)l->data;
        l = l->next;
        for(;l;l=l->next)
        {
            FmFileInfo* fi2 = (FmFileInfo*)l->data;
            if(fi->type != fi2->type)
                return FALSE;
        }
    }
    return TRUE;
}

/* return TRUE if all files in the list are on the same fs */
gboolean fm_file_info_list_is_same_fs(FmFileInfoList* list)
{
    if( ! fm_list_is_empty(list) )
    {
        GList* l = fm_list_peek_head_link(list);
        FmFileInfo* fi = (FmFileInfo*)l->data;
        l = l->next;
        for(;l;l=l->next)
        {
            FmFileInfo* fi2 = (FmFileInfo*)l->data;
            gboolean is_native = fm_path_is_native(fi->path);
            if( is_native != fm_path_is_native(fi2->path) )
                return FALSE;
            if( is_native )
            {
                if( fi->dev != fi2->dev )
                    return FALSE;
            }
            else
            {
                if( fi->fs_id != fi2->fs_id )
                    return FALSE;
            }
        }
    }
    return TRUE;
}
