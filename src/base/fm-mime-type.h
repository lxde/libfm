/*
 *      fm-mime-type.h
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

#ifndef _FM_MIME_TYPE_H_
#define _FM_MIME_TYPE_H_

#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include "fm-icon.h"

G_BEGIN_DECLS

typedef struct _FmMimeType FmMimeType;

struct _FmMimeType
{
    char* type; /* mime type name */
    char* description;  /* description of the mime type */
	FmIcon* icon;

    int n_ref;
};

void fm_mime_type_init();

void fm_mime_type_finalize();

/* file name used in this API should be encoded in UTF-8 */
FmMimeType* fm_mime_type_get_for_file_name( const char* ufile_name );

FmMimeType* fm_mime_type_get_for_native_file( const char* file_path,  /* Should be on-disk encoding */
                                          const char* base_name,  /* Should be in UTF-8 */
                                          struct stat* pstat );   /* Can be NULL */

FmMimeType* fm_mime_type_get_for_type( const char* type );

FmMimeType* fm_mime_type_new( const char* type_name );
FmMimeType* fm_mime_type_ref( FmMimeType* mime_type );
void fm_mime_type_unref( gpointer mime_type_ );

FmIcon* fm_mime_type_get_icon( FmMimeType* mime_type );

/* Get mime-type string */
const char* fm_mime_type_get_type( FmMimeType* mime_type );

/* Get human-readable description of mime-type */
const char* fm_mime_type_get_desc( FmMimeType* mime_type );

#if 0
/*
 * Get available actions (applications) for this mime-type
 * returned vector should be freed with g_strfreev when not needed.
*/
char** fm_mime_type_get_actions( FmMimeType* mime_type );

/* returned string should be freed with g_strfreev when not needed. */
char* fm_mime_type_get_default_action( FmMimeType* mime_type );

void fm_mime_type_set_default_action( FmMimeType* mime_type,
                                       const char* desktop_id );

/* If user-custom desktop file is created, it's returned in custom_desktop. */
void fm_mime_type_add_action( FmMimeType* mime_type,
                               const char* desktop_id,
                               char** custom_desktop );

char** fm_mime_type_get_all_known_apps();

char** fm_mime_type_join_actions( char** list1, gsize len1,
                                   char** list2, gsize len2 );

GList* fm_mime_type_add_reload_cb( GFreeFunc cb, gpointer user_data );

void fm_mime_type_remove_reload_cb( GList* cb );
#endif

G_END_DECLS

#endif
