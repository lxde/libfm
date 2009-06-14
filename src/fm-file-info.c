/*
*  C Implementation: vfs-file-info
*
* Description: File information
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "fm-file-info.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <grp.h> /* Query group name */
#include <pwd.h> /* Query user name */
#include <string.h>

#include "fm-utils.h"

static gboolean use_si_prefix = TRUE;


FmFileInfo* fm_file_info_new ()
{
    FmFileInfo * fi = g_slice_new0( FmFileInfo );
    fi->n_ref = 1;
    return fi;
}

FmFileInfo* fm_file_info_new_from_gfileinfo(FmPath* parent_dir, GFileInfo* inf)
{
	FmFileInfo* fi = fm_file_info_new();
	const char* tmp;
	fi->path = fm_path_new_child(parent_dir, g_file_info_get_name(inf));

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

	if( !fi->type )
		fi->icon = g_object_ref(g_file_info_get_icon(inf));
	else
		fi->icon = g_object_ref(fi->type->icon);
	return fi;
}

static void fm_file_info_clear( FmFileInfo* fi )
{
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
	if( fi->type )
	{
		fm_mime_type_unref(fi->type);
		fi->type = NULL;
	}
    if( fi->icon )
    {
		g_object_unref(fi->icon);
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
    if ( S_ISDIR( fi->mode ) )
        return TRUE;
    if ( S_ISLNK( fi->mode ) &&
         0 == strcmp( fi->type->type, "inode/directory" ) )
    {
        return TRUE;
    }
    return FALSE;
}

gboolean fm_file_info_is_symlink( FmFileInfo* fi )
{
    return S_ISLNK( fi->mode ) ? TRUE : FALSE;
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
//    return 0 != (fi->flags & VFS_FILE_INFO_DESKTOP_ENTRY);
	return FALSE;
}

gboolean fm_file_info_is_unknown_type( FmFileInfo* fi )
{
	return g_content_type_is_unknown(fi->type->type);
}

/* full path of the file is required by this function */
gboolean fm_file_info_is_executable( FmFileInfo* fi, const char* file_path )
{
	// FIXME: didn't check access rights.
//    return mime_type_is_executable_file( file_path, fi->type->type );
	return g_content_type_can_be_executable(fi->type->type);
}


const char* fm_file_info_get_desc( FmFileInfo* fi )
{
	/* FIXME: how to handle descriptions for virtual files without mime-tyoes? */
    return fi->type ? fm_mime_type_get_description( fi->type ) : NULL;
}

const char* fm_file_info_get_disp_mtime( FmFileInfo* fi )
{
    if ( ! fi->disp_mtime )
    {
        char buf[ 128 ];
        strftime( buf, sizeof( buf ),
                  "%x %R",
                  localtime( &fi->mtime ) );
        fi->disp_mtime = g_strdup( buf );
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

void fm_file_info_list_free( GList* list )
{
    g_list_foreach( list, (GFunc)fm_file_info_unref, NULL );
    g_list_free( list );
}

