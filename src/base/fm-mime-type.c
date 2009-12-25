/*
*  C Implementation: vfs-mime_type-type
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "fm-mime-type.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

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

#if 0
static gboolean fm_mime_type_reload( gpointer user_data )
{
    GList* l;
    /* FIXME: process mime database reloading properly. */
    /* Remove all items in the hash table */
    GDK_THREADS_ENTER();

    g_static_rw_lock_writer_lock( &mime_hash_lock );
    g_hash_table_foreach_remove ( mime_hash, ( GHRFunc ) gtk_true, NULL );
    g_static_rw_lock_writer_unlock( &mime_hash_lock );

    g_source_remove( reload_callback_id );
    reload_callback_id = 0;

    /* g_debug( "reload mime-types" ); */

    /* call all registered callbacks */
    for( l = reload_cb; l; l = l->next )
    {
        FmMimeReloadCbEnt* ent = (FmMimeReloadCbEnt*)l->data;
        ent->cb( ent->user_data );
    }
    GDK_THREADS_LEAVE();
    return FALSE;
}

static void on_mime_cache_changed( VFSFileMonitor* fm,
                                        VFSFileMonitorEvent event,
                                        const char* file_name,
                                        gpointer user_data )
{
    MimeCache* cache = (MimeCache*)user_data;
    switch( event )
    {
    case FM_FILE_MONITOR_CREATE:
    case FM_FILE_MONITOR_DELETE:
        /* NOTE: FAM sometimes generate incorrect "delete" notification for non-existent files.
         *  So if the cache is not loaded originally (the cache file is non-existent), we skip it. */
        if( ! cache->buffer )
            return;
    case FM_FILE_MONITOR_CHANGE:
        mime_cache_reload( cache );
        /* g_debug( "reload cache: %s", file_name ); */
        if( 0 == reload_callback_id )
            reload_callback_id = g_idle_add( fm_mime_type_reload, NULL );
    }
}
#endif

void fm_mime_type_init()
{
    FmMimeType* tmp;
#if 0
    GtkIconTheme * theme;
    MimeCache** caches;
    int i, n_caches;

    mime_type_init();

    /* install file alteration monitor for mime-cache */
    caches = mime_type_get_caches( &n_caches );
    mime_caches_monitor = g_new0( VFSFileMonitor*, n_caches );
    for( i = 0; i < n_caches; ++i )
    {
        VFSFileMonitor* fm = vfs_file_monitor_add_file( caches[i]->file_path,
                                                                on_mime_cache_changed, caches[i] );
        mime_caches_monitor[i] = fm;
    }
#endif

    mime_hash = g_hash_table_new_full( g_str_hash, g_str_equal,
                                       NULL, fm_mime_type_unref );
}

void fm_mime_type_finalize()
{
#if 0
    GtkIconTheme * theme;
    MimeCache** caches;
    int i, n_caches;

    theme = gtk_icon_theme_get_default();
    g_signal_handler_disconnect( theme, theme_change_notify );

    /* remove file alteration monitor for mime-cache */
    caches = mime_type_get_caches( &n_caches );
    for( i = 0; i < n_caches; ++i )
    {
        vfs_file_monitor_remove( mime_caches_monitor[i],
                                        on_mime_cache_changed, caches[i] );
    }
    g_free( mime_caches_monitor );

    mime_type_finalize();
#endif
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

/*
static FmMimeType* fm_mime_type_get_internal(char* type)
{
	FmMimeType* 
}
*/

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
	    if( pstat->st_size == 0 ) /* empty file = text file with 0 characters in it. */
            return fm_mime_type_get_for_type( "text/plain" );
	    else
	    {
            gboolean uncertain;
            char* type = g_content_type_guess( base_name, NULL, 0, &uncertain );
            if( uncertain )
            {
                char buf[4096];
                int fd, len;
                fd = open(file_path, O_RDONLY);
                if( fd >= 0 )
                {
                    g_free(type);
                    len = read(fd, buf, 4096);
                    close(fd);
                    type = g_content_type_guess( NULL, buf, len, &uncertain );
                }
            }
            mime_type = fm_mime_type_get_for_type( type );
            g_free(type);
            return mime_type;
	    }
	}

	if( S_ISDIR(pstat->st_mode) )
	    return fm_mime_type_get_for_type( "inode/directory" );
	if (S_ISCHR(pstat->st_mode))
	    return fm_mime_type_get_for_type( "inode/chardevice" );
	if (S_ISBLK(pstat->st_mode))
	    return fm_mime_type_get_for_type( "inode/blockdevice" );
	if (S_ISFIFO(pstat->st_mode))
	    return fm_mime_type_get_for_type( "inode/fifo" );
#ifdef S_ISSOCK
	if (S_ISSOCK(pstat->st_mode))
	    return fm_mime_type_get_for_type( "inode/socket" );
#endif
	/* impossible */
	g_error( "Invalid stat mode: %s", base_name );
	return NULL;
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
		g_themed_icon_prepend_name(gicon, "folder");
	else if( g_content_type_can_be_executable(mime_type->type) )
		g_themed_icon_append_name(gicon, "application-x-executable");

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
#if 0
    GdkPixbuf * icon = NULL;
    const char* sep;
    char icon_name[ 100 ];
    GtkIconTheme *icon_theme;
    int size;

    if ( big )
    {
        if ( G_LIKELY( mime_type->big_icon ) )     /* big icon */
            return gdk_pixbuf_ref( mime_type->big_icon );
        size = big_icon_size;
    }
    else    /* small icon */
    {
        if ( G_LIKELY( mime_type->small_icon ) )
            return gdk_pixbuf_ref( mime_type->small_icon );
        size = small_icon_size;
    }

    icon_theme = gtk_icon_theme_get_default ();

    if ( G_UNLIKELY( 0 == strcmp( mime_type->type, XDG_MIME_TYPE_DIRECTORY ) ) )
    {
        icon = vfs_load_icon ( icon_theme, "folder", size );
        if( G_UNLIKELY(! icon) )
            icon = vfs_load_icon ( icon_theme, "gnome-fs-directory", size );
        if ( big )
            mime_type->big_icon = icon;
        else
            mime_type->small_icon = icon;
        return icon ? gdk_pixbuf_ref( icon ) : NULL;
    }

    sep = strchr( mime_type->type, '/' );
    if ( sep )
    {
        /* convert mime-type foo/bar to foo-bar */
        strcpy( icon_name, mime_type->type );
        icon_name[ (sep - mime_type->type) ] = '-';
        /* is there an icon named foo-bar? */
        icon = vfs_load_icon ( icon_theme, icon_name, size );
        if ( ! icon )
        {
            /* maybe we can find a legacy icon named gnome-mime-foo-bar */
            strcpy( icon_name, "gnome-mime-" );
            strncat( icon_name, mime_type->type, ( sep - mime_type->type ) );
            strcat( icon_name, "-" );
            strcat( icon_name, sep + 1 );
            icon = vfs_load_icon ( icon_theme, icon_name, size );
        }
        /* try gnome-mime-foo */
        if ( G_UNLIKELY( ! icon ) )
        {
            icon_name[ 11 ] = '\0'; /* strlen("gnome-mime-") = 11 */
            strncat( icon_name, mime_type->type, ( sep - mime_type->type ) );
            icon = vfs_load_icon ( icon_theme, icon_name, size );
        }
        /* try foo-x-generic */
        if ( G_UNLIKELY( ! icon ) )
        {
            strncpy( icon_name, mime_type->type, ( sep - mime_type->type ) );
            icon_name[ (sep - mime_type->type) ] = '\0';
            strcat( icon_name, "-x-generic" );
            icon = vfs_load_icon ( icon_theme, icon_name, size );
        }
    }

    if( G_UNLIKELY( !icon ) )
    {
        /* prevent endless recursion of XDG_MIME_TYPE_UNKNOWN */
        if( G_LIKELY( strcmp(mime_type->type, XDG_MIME_TYPE_UNKNOWN) ) )
        {
            /* FIXME: fallback to icon of parent mime-type */
            FmMimeType* unknown;
            unknown = fm_mime_type_get_from_type( XDG_MIME_TYPE_UNKNOWN );
            icon = fm_mime_type_get_icon( unknown, big );
            fm_mime_type_unref( unknown );
        }
        else /* unknown */
        {
            icon = vfs_load_icon ( icon_theme, "unknown", size );
        }
    }

    if ( big )
        mime_type->big_icon = icon;
    else
        mime_type->small_icon = icon;
    return icon ? gdk_pixbuf_ref( icon ) : NULL;
#endif
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

#if 0

/*
* Join two string vector containing app lists to generate a new one.
* Duplicated app will be removed.
*/
char** fm_mime_type_join_actions( char** list1, gsize len1,
                                   char** list2, gsize len2 )
{
    gchar **ret = NULL;
    int i, j, k;

    if ( len1 > 0 || len2 > 0 )
        ret = g_new0( char*, len1 + len2 + 1 );
    for ( i = 0; i < len1; ++i )
    {
        ret[ i ] = g_strdup( list1[ i ] );
    }
    for ( j = 0, k = 0; j < len2; ++j )
    {
        for ( i = 0; i < len1; ++i )
        {
            if ( 0 == strcmp( ret[ i ], list2[ j ] ) )
                break;
        }
        if ( i >= len1 )
        {
            ret[ len1 + k ] = g_strdup( list2[ j ] );
            ++k;
        }
    }
    return ret;
}

char** fm_mime_type_get_actions( FmMimeType* mime_type )
{
    return (char**)mime_type_get_actions( mime_type->type );
}

char* fm_mime_type_get_default_action( FmMimeType* mime_type )
{
    char* def = (char*)mime_type_get_default_action( mime_type->type );
    /* FIXME:
     * If default app is not set, choose one from all availble actions.
     * Is there any better way to do this?
     * Should we put this fallback handling here, or at API of higher level?
     */
    if( ! def )
    {
        char** actions = mime_type_get_actions( mime_type->type );
        if( actions )
        {
            def = g_strdup( actions[0] );
            g_strfreev( actions );
        }
    }
    return def;
}

/*
* Set default app.desktop for specified file.
* app can be the name of the desktop file or a command line.
*/
void fm_mime_type_set_default_action( FmMimeType* mime_type,
                                       const char* desktop_id )
{
    char* cust_desktop = NULL;
/*
    if( ! g_str_has_suffix( desktop_id, ".desktop" ) )
        return;
*/
    fm_mime_type_add_action( mime_type, desktop_id, &cust_desktop );
    if( cust_desktop )
        desktop_id = cust_desktop;
    mime_type_set_default_action( mime_type->type, desktop_id );

    g_free( cust_desktop );
}

/* If user-custom desktop file is created, it's returned in custom_desktop. */
void fm_mime_type_add_action( FmMimeType* mime_type,
                               const char* desktop_id,
                               char** custom_desktop )
{
    mime_type_add_action( mime_type->type, desktop_id, custom_desktop );
}

GList* fm_mime_type_add_reload_cb( GFreeFunc cb, gpointer user_data )
{
    FmMimeReloadCbEnt* ent = g_slice_new( FmMimeReloadCbEnt );
    ent->cb = cb;
    ent->user_data = user_data;
    reload_cb = g_list_append( reload_cb, ent );
    return g_list_last( reload_cb );
}

void fm_mime_type_remove_reload_cb( GList* cb )
{
    g_slice_free( FmMimeReloadCbEnt, cb->data );
    reload_cb = g_list_delete_link( reload_cb, cb );
}

#endif
