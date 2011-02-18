/*
 *      fm-mime-type.c
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

#include "fm-mime-type.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

/* FIXME: how can we handle reload of xdg mime? */

static GHashTable *mime_hash = NULL;
G_LOCK_DEFINE(mime_hash);

static guint reload_callback_id = 0;
static GList* reload_cb = NULL;

//static VFSFileMonitor** mime_caches_monitor = NULL;

typedef struct {
    GFreeFunc cb;
    gpointer user_data;
}FmMimeReloadCbEnt;

void fm_mime_type_init()
{
    mime_hash = g_hash_table_new_full( g_str_hash, g_str_equal,
                                       NULL, fm_mime_type_unref );
}

void fm_mime_type_finalize()
{
    g_hash_table_destroy( mime_hash );
}

FmMimeType* fm_mime_type_get_for_file_name( const char* ufile_name )
{
    FmMimeType* mime_type;
    char * type;
    gboolean uncertain;
    type = g_content_type_guess( ufile_name, NULL, 0, &uncertain );
    mime_type = fm_mime_type_get_for_type( type );
    g_free(type);
    return mime_type;
}

FmMimeType* fm_mime_type_get_for_native_file( const char* file_path,
                                        const char* base_name,
                                        struct stat* pstat )
{
    FmMimeType* mime_type;
    struct stat st;

    if( !pstat )
    {
        pstat = &st;
        if( stat( file_path, &st ) == -1 )
            return NULL;
    }

    if( S_ISREG(pstat->st_mode) )
    {
        gboolean uncertain;
        char* type = g_content_type_guess( base_name, NULL, 0, &uncertain );
        if( uncertain )
        {
            int fd, len;
            if( pstat->st_size == 0 ) /* empty file = text file with 0 characters in it. */
                return fm_mime_type_get_for_type( "text/plain" );
            fd = open(file_path, O_RDONLY);
            if( fd >= 0 )
            {
                /* #3086703 - PCManFM crashes on non existent directories.
                 * http://sourceforge.net/tracker/?func=detail&aid=3086703&group_id=156956&atid=801864
                 *
                 * NOTE: do not use mmap here. Though we can get little
                 * performance gain, this makes our program more vulnerable
                 * to I/O errors. If the mapped file is truncated by other
                 * processes or I/O errors happen, we may receive SIGBUS.
                 * It's a pity that we cannot use mmap for speed up here. */
            /*
            #ifdef HAVE_MMAP
                const char* buf;
                len = pstat->st_size > 4096 ? 4096 : pstat->st_size;
                buf = (const char*)mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
                if(G_LIKELY(buf != MAP_FAILED))
                {
                    g_free(type);
                    type = g_content_type_guess( NULL, buf, len, &uncertain );
                    munmap(buf, len);
                }
            #else
            */
                char buf[4096];
                len = read(fd, buf, 4096);
                g_free(type);
                type = g_content_type_guess( NULL, buf, len, &uncertain );
            /* #endif */
                close(fd);
            }
        }
        mime_type = fm_mime_type_get_for_type( type );
        g_free(type);
        return mime_type;
    }

    if( S_ISDIR(pstat->st_mode) )
        return fm_mime_type_get_for_type( "inode/directory" );
    if (S_ISCHR(pstat->st_mode))
        return fm_mime_type_get_for_type( "inode/chardevice" );
    if (S_ISBLK(pstat->st_mode))
        return fm_mime_type_get_for_type( "inode/blockdevice" );
    if (S_ISFIFO(pstat->st_mode))
        return fm_mime_type_get_for_type( "inode/fifo" );
    if (S_ISLNK(pstat->st_mode))
        return fm_mime_type_get_for_type( "inode/symlink" );
#ifdef S_ISSOCK
    if (S_ISSOCK(pstat->st_mode))
        return fm_mime_type_get_for_type( "inode/socket" );
#endif
    /* impossible */
    g_debug( "Invalid stat mode: %d, %s", pstat->st_mode & S_IFMT, base_name );
    /* FIXME: some files under /proc/self has st_mode = 0, which causes problems.
     *        currently we treat them as files of unknown type. */
    return fm_mime_type_get_for_type( "application/octet-stream" );
}

FmMimeType* fm_mime_type_get_for_type( const char* type )
{
    FmMimeType * mime_type;

    G_LOCK( mime_hash );
    mime_type = g_hash_table_lookup( mime_hash, type );
    if ( !mime_type )
    {
        mime_type = fm_mime_type_new( type );
        g_hash_table_insert( mime_hash, mime_type->type, mime_type );
    }
    G_UNLOCK( mime_hash );
    fm_mime_type_ref( mime_type );
    return mime_type;
}

FmMimeType* fm_mime_type_new( const char* type_name )
{
    FmMimeType * mime_type = g_slice_new0( FmMimeType );
    GIcon* gicon;
    mime_type->type = g_strdup( type_name );
    mime_type->n_ref = 1;

    gicon = g_content_type_get_icon(mime_type->type);
    if( strcmp(mime_type->type, "inode/directory") == 0 )
        g_themed_icon_prepend_name(G_THEMED_ICON(gicon), "folder");
    else if( g_content_type_can_be_executable(mime_type->type) )
        g_themed_icon_append_name(G_THEMED_ICON(gicon), "application-x-executable");

    mime_type->icon = fm_icon_from_gicon(gicon);
    g_object_unref(gicon);

#if 0
  /* TODO: Special case desktop dir? That could be expensive with xdg dirs... */
  if (strcmp (path, g_get_home_dir ()) == 0)
    type_icon = "user-home";
  else
    type_icon = "text-x-generic";
#endif

    return mime_type;
}

FmMimeType* fm_mime_type_ref( FmMimeType* mime_type )
{
    g_atomic_int_inc(&mime_type->n_ref);
    return mime_type;
}

void fm_mime_type_unref( gpointer mime_type_ )
{
    FmMimeType* mime_type = (FmMimeType*)mime_type_;
    if ( g_atomic_int_dec_and_test(&mime_type->n_ref) )
    {
        g_free( mime_type->type );
        if ( mime_type->icon )
            fm_icon_unref( mime_type->icon );
        g_slice_free( FmMimeType, mime_type );
    }
}

FmIcon* fm_mime_type_get_icon( FmMimeType* mime_type )
{
    return mime_type->icon;
}

const char* fm_mime_type_get_type( FmMimeType* mime_type )
{
    return mime_type->type;
}

/* Get human-readable description of mime type */
const char* fm_mime_type_get_desc( FmMimeType* mime_type )
{
    /* FIXME: is locking needed here or not? */
    if ( G_UNLIKELY( ! mime_type->description ) )
    {
        mime_type->description = g_content_type_get_description( mime_type->type );
        /* FIXME: should handle this better */
        if ( G_UNLIKELY( ! mime_type->description || ! *mime_type->description ) )
            mime_type->description = g_content_type_get_description( mime_type->type );
    }
    return mime_type->description;
}
