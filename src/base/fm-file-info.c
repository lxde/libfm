/*
 *      fm-file-info.c
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
void fm_file_info_init()
{
	fm_mime_type_init();
    desktop_entry_type = fm_mime_type_get_for_type("application/x-desktop");

    /* fake mime-types for mountable and shortcuts */
    shortcut_type = fm_mime_type_get_for_type("inode/x-shortcut");
    shortcut_type->description = g_strdup(_("Shortcuts"));

    mountable_type = fm_mime_type_get_for_type("inode/x-mountable");
    mountable_type->description = g_strdup(_("Mount Point"));
}

void fm_file_info_finalize()
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

FmFileInfo* _fm_file_info_new_from_menu_cache_item(FmPath* path, MenuCacheItem* item)
{
    FmFileInfo* fi = fm_file_info_new();
    fi->path = fm_path_ref(path);
    fi->disp_name = g_strdup(menu_cache_item_get_name(item));
    fi->icon = fm_icon_from_name(menu_cache_item_get_icon(item));
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
    if( fi->disp_name && fi->disp_name != fi->path->name )
    {
        g_free( fi->disp_name );
        fi->disp_name = NULL;
    }
    if( fi->path )
    {
        fm_path_unref(fi->path);
        fi->path = NULL;
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
    return fi->disp_name;
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
        char* collate = g_utf8_collate_key_for_filename(fi->disp_name, -1);
        if( strcmp(collate, fi->disp_name) )
            fi->collate_key = collate;
        else
            fi->collate_key = fi->disp_name;
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

#if 0
void fm_file_info_reload_mime_type( FmFileInfo* fi,
                                     const char* full_path )
{
    VFSMimeType * old_mime_type;
    struct stat file_stat;

    /* convert FmFileInfo to struct stat */
    /* In current implementation, only st_mode is used in
       mime-type detection, so let's save some CPU cycles
       and don't copy unused fields.
    */
    file_stat.st_mode = fi->mode;
    /*
    file_stat.st_dev = fi->dev;
    file_stat.st_uid = fi->uid;
    file_stat.st_gid = fi->gid;
    file_stat.st_size = fi->size;
    file_stat.st_mtime = fi->mtime;
    file_stat.st_atime = fi->atime;
    file_stat.st_blksize = fi->blksize;
    file_stat.st_blocks = fi->blocks;
    */
    old_mime_type = fi->type;
    fi->type = vfs_mime_type_get_from_file( full_path,
                                                 fi->name, &file_stat );
    fm_file_info_load_special_info( fi, full_path );
    vfs_mime_type_unref( old_mime_type );  /* FIXME: is vfs_mime_type_unref needed ?*/
}


GdkPixbuf* fm_file_info_get_big_icon( FmFileInfo* fi )
{
    /* get special icons for special files, especially for
       some desktop icons */

    if ( G_UNLIKELY( fi->flags != VFS_FILE_INFO_NONE ) )
    {
        int w, h;
        int icon_size;
        vfs_mime_type_get_icon_size( &icon_size, NULL );
        if ( fi->big_thumbnail )
        {
            w = gdk_pixbuf_get_width( fi->big_thumbnail );
            h = gdk_pixbuf_get_height( fi->big_thumbnail );
        }
        else
            w = h = 0;

        if ( ABS( MAX( w, h ) - icon_size ) > 2 )
        {
            char * icon_name = NULL;
            if ( fi->big_thumbnail )
            {
                icon_name = ( char* ) g_object_steal_data(
                                G_OBJECT(fi->big_thumbnail), "name" );
                gdk_pixbuf_unref( fi->big_thumbnail );
                fi->big_thumbnail = NULL;
            }
            if ( G_LIKELY( icon_name ) )
            {
                if ( G_UNLIKELY( icon_name[ 0 ] == '/' ) )
                    fi->big_thumbnail = gdk_pixbuf_new_from_file( icon_name, NULL );
                else
                    fi->big_thumbnail = vfs_load_icon(
                                            gtk_icon_theme_get_default(),
                                            icon_name, icon_size );
            }
            if ( fi->big_thumbnail )
                g_object_set_data_full( G_OBJECT(fi->big_thumbnail), "name", icon_name, g_free );
            else
                g_free( icon_name );
        }
        return fi->big_thumbnail ? gdk_pixbuf_ref( fi->big_thumbnail ) : NULL;
    }
    if( G_UNLIKELY(!fi->type) )
        return NULL;
    return vfs_mime_type_get_icon( fi->type, TRUE );
}

GdkPixbuf* fm_file_info_get_small_icon( FmFileInfo* fi )
{
    return vfs_mime_type_get_icon( fi->type, FALSE );
}

GdkPixbuf* fm_file_info_get_big_thumbnail( FmFileInfo* fi )
{
    return fi->big_thumbnail ? gdk_pixbuf_ref( fi->big_thumbnail ) : NULL;
}

GdkPixbuf* fm_file_info_get_small_thumbnail( FmFileInfo* fi )
{
    return fi->small_thumbnail ? gdk_pixbuf_ref( fi->small_thumbnail ) : NULL;
}

const char* fm_file_info_get_disp_owner( FmFileInfo* fi )
{
    struct passwd * puser;
    struct group* pgroup;
    char uid_str_buf[ 32 ];
    char* user_name;
    char gid_str_buf[ 32 ];
    char* group_name;

    /* FIXME: user names should be cached */
    if ( ! fi->disp_owner )
    {
        puser = getpwuid( fi->uid );
        if ( puser && puser->pw_name && *puser->pw_name )
            user_name = puser->pw_name;
        else
        {
            sprintf( uid_str_buf, "%d", fi->uid );
            user_name = uid_str_buf;
        }

        pgroup = getgrgid( fi->gid );
        if ( pgroup && pgroup->gr_name && *pgroup->gr_name )
            group_name = pgroup->gr_name;
        else
        {
            sprintf( gid_str_buf, "%d", fi->gid );
            group_name = gid_str_buf;
        }
        fi->disp_owner = g_strdup_printf ( "%s:%s", user_name, group_name );
    }
    return fi->disp_owner;
}

static void get_file_perm_string( char* perm, mode_t mode )
{
    perm[ 0 ] = S_ISDIR( mode ) ? 'd' : ( S_ISLNK( mode ) ? 'l' : '-' );
    perm[ 1 ] = ( mode & S_IRUSR ) ? 'r' : '-';
    perm[ 2 ] = ( mode & S_IWUSR ) ? 'w' : '-';
    perm[ 3 ] = ( mode & S_IXUSR ) ? 'x' : '-';
    perm[ 4 ] = ( mode & S_IRGRP ) ? 'r' : '-';
    perm[ 5 ] = ( mode & S_IWGRP ) ? 'w' : '-';
    perm[ 6 ] = ( mode & S_IXGRP ) ? 'x' : '-';
    perm[ 7 ] = ( mode & S_IROTH ) ? 'r' : '-';
    perm[ 8 ] = ( mode & S_IWOTH ) ? 'w' : '-';
    perm[ 9 ] = ( mode & S_IXOTH ) ? 'x' : '-';
    perm[ 10 ] = '\0';
}

const char* fm_file_info_get_disp_perm( FmFileInfo* fi )
{
    if ( ! fi->disp_perm[ 0 ] )
        get_file_perm_string( fi->disp_perm,
                              fi->mode );
    return fi->disp_perm;
}

/* full path of the file is required by this function */
gboolean fm_file_info_is_text( FmFileInfo* fi, const char* file_path )
{
    return mime_type_is_text_file( file_path, fi->type->type );
}

/*
* Run default action of specified file.
* Full path of the file is required by this function.
*/
gboolean fm_file_info_open_file( FmFileInfo* fi,
                                  const char* file_path,
                                  GError** err )
{
    VFSMimeType * mime_type;
    char* app_name;
    VFSAppDesktop* app;
    GList* files = NULL;
    gboolean ret = FALSE;
    char* argv[ 2 ];

    if ( fm_file_info_is_executable( fi, file_path ) )
    {
        argv[ 0 ] = (char *) file_path;
        argv[ 1 ] = '\0';
        ret = g_spawn_async( NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL|
                             G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, err );
    }
    else
    {
        mime_type = fm_file_info_get_mime_type( fi );
        app_name = vfs_mime_type_get_default_action( mime_type );
        if ( app_name )
        {
            app = vfs_app_desktop_new( app_name );
            if ( ! vfs_app_desktop_get_exec( app ) )
                app->exec = g_strdup( app_name );   /* FIXME: app->exec */
            files = g_list_prepend( files, (gpointer) file_path );
            /* FIXME: working dir is needed */
            ret = vfs_app_desktop_open_files( gdk_screen_get_default(),
                                              NULL, app, files, err );
            g_list_free( files );
            vfs_app_desktop_unref( app );
            g_free( app_name );
        }
        vfs_mime_type_unref( mime_type );
    }
    return ret;
}


gboolean fm_file_info_is_thumbnail_loaded( FmFileInfo* fi, gboolean big )
{
    if ( big )
        return ( fi->big_thumbnail != NULL );
    return ( fi->small_thumbnail != NULL );
}

gboolean fm_file_info_load_thumbnail( FmFileInfo* fi,
                                       const char* full_path,
                                       gboolean big )
{
    GdkPixbuf* thumbnail;

    if ( big )
    {
        if ( fi->big_thumbnail )
            return TRUE;
    }
    else
    {
        if ( fi->small_thumbnail )
            return TRUE;
    }
    thumbnail = vfs_thumbnail_load_for_file( full_path,
                                                    big ? big_thumb_size : small_thumb_size , fi->mtime );
    if( G_LIKELY( thumbnail ) )
    {
        if ( big )
            fi->big_thumbnail = thumbnail;
        else
            fi->small_thumbnail = thumbnail;
    }
    else /* fallback to mime_type icon */
    {
        if ( big )
            fi->big_thumbnail = fm_file_info_get_big_icon( fi );
        else
            fi->small_thumbnail = fm_file_info_get_small_icon( fi );
    }
    return ( thumbnail != NULL );
}

void fm_file_info_set_thumbnail_size( int big, int small )
{
    big_thumb_size = big;
    small_thumb_size = small;
}

void fm_file_info_load_special_info( FmFileInfo* fi,
                                      const char* file_path )
{
    /*if ( G_LIKELY(fi->type) && G_UNLIKELY(fi->type->name, "application/x-desktop") ) */
    if ( G_UNLIKELY( g_str_has_suffix( fi->name, ".desktop") ) )
    {
        VFSAppDesktop * desktop;
        const char* icon_name;

        fi->flags |= VFS_FILE_INFO_DESKTOP_ENTRY;
        desktop = vfs_app_desktop_new( file_path );
        if ( vfs_app_desktop_get_disp_name( desktop ) )
        {
            fm_file_info_set_disp_name(
                fi, vfs_app_desktop_get_disp_name( desktop ) );
        }

        if ( (icon_name = vfs_app_desktop_get_icon_name( desktop )) )
        {
            GdkPixbuf* icon;
            int big_size, small_size;
            vfs_mime_type_get_icon_size( &big_size, &small_size );
            if( ! fi->big_thumbnail )
            {
                icon = vfs_app_desktop_get_icon( desktop, big_size, FALSE );
                if( G_LIKELY(icon) )
                    fi->big_thumbnail =icon;
            }
            if( ! fi->small_thumbnail )
            {
                icon = vfs_app_desktop_get_icon( desktop, small_size, FALSE );
                if( G_LIKELY(icon) )
                    fi->small_thumbnail =icon;
            }
        }
        vfs_app_desktop_unref( desktop );
    }
}


char* vfs_file_resolve_path( const char* cwd, const char* relative_path )
{
    GString* ret = g_string_sized_new( 4096 );
    int len;
    gboolean strip_tail;

    g_return_val_if_fail( G_LIKELY(relative_path), NULL );

    len = strlen( relative_path );
    strip_tail = (0 == len || relative_path[len-1] != '/');

    if( G_UNLIKELY(*relative_path != '/') ) /* relative path */
    {
        if( G_UNLIKELY(relative_path[0] == '~') ) /* home dir */
        {
            g_string_append( ret, g_get_home_dir());
            ++relative_path;
        }
        else
        {
            if( ! cwd )
            {
                char *cwd_new;
                cwd_new = g_get_current_dir();
                g_string_append( ret, cwd_new );
                g_free( cwd_new );
            }
            else
                g_string_append( ret, cwd );
        }
    }

    if( relative_path[0] != '/'  && (0 == ret->len || ret->str[ ret->len - 1 ] != '/' ) )
        g_string_append_c( ret, '/' );

    while( G_LIKELY( *relative_path ) )
    {
        if( G_UNLIKELY(*relative_path == '.') )
        {
            if( relative_path[1] == '/' || relative_path[1] == '\0' ) /* current dir */
            {
                relative_path += relative_path[1] ? 2 : 1;
                continue;
            }
            if( relative_path[1] == '.' &&
                ( relative_path[2] == '/' || relative_path[2] == '\0') ) /* parent dir */
            {
                gsize len = ret->len - 2;
                while( ret->str[ len ] != '/' )
                    --len;
                g_string_truncate( ret, len + 1 );
                relative_path += relative_path[2] ? 3 : 2;
                continue;
            }
        }

        do
        {
            g_string_append_c( ret, *relative_path );
        }while( G_LIKELY( *(relative_path++) != '/' && *relative_path ) );
    }

    /* if original path contains tailing '/', preserve it; otherwise, remove it. */
    if( strip_tail && G_LIKELY( ret->len > 1 ) && G_UNLIKELY( ret->str[ ret->len - 1 ] == '/' ) )
        g_string_truncate( ret, ret->len - 1 );
    return g_string_free( ret, FALSE );
}

#endif

/*
void fm_file_info_list_free( GList* list )
{
    g_list_foreach( list, (GFunc)fm_file_info_unref, NULL );
    g_list_free( list );
}
*/

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
