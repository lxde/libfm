/*
 *      fm-file.c
 *
 *      Copyright 2012 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

/**
 * SECTION:fm-file
 * @short_description: Extensions for GFile interface.
 * @title: FmFile
 *
 * @include: libfm/fm-file.h
 *
 * The #FmFile represents interface to build extensions to GFile which
 * will handle schemas that are absent in Glib/GVFS - such as "search:".
 *
 * To use it the GFile implementation should also implement FmFile vtable
 * calls. The initialization of such implementation should be done from
 * _fm_file_init().
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-file.h"

#include <string.h>

static GHashTable *schemes = NULL;

#define FM_FILE_MODULE_MIN_VERSION 1
#define FM_FILE_MODULE_MAX_VERSION 1


G_DEFINE_INTERFACE(FmFile, fm_file, G_TYPE_FILE)

gboolean fm_file_wants_incremental_false(FmFile *unused)
{
    return FALSE;
}

static void fm_file_default_init(FmFileInterface *iface)
{
    iface->wants_incremental = fm_file_wants_incremental_false;
}


static inline FmFileInterface *fm_find_scheme(const char *name)
{
    return (FmFileInterface*)g_hash_table_lookup(schemes, name);
}

static inline void fm_add_scheme(const char *name, FmFileInterface *iface)
{
    if(fm_find_scheme(name) == NULL)
        g_hash_table_insert(schemes, g_strdup(name), iface);
}

/**
 * fm_file_wants_incremental
 * @file: file to inspect
 *
 * Checks if contents of directory @file cannot be retrieved at once so
 * scanning it may be done in incremental manner for the best results.
 *
 * Returns: %TRUE if retrieve of contents of @file will be incremental.
 *
 * Since: 1.0.2
 */
gboolean fm_file_wants_incremental(FmFile* file)
{
    FmFileInterface *iface;

    g_return_val_if_fail(file != NULL, FALSE);
    if(!FM_IS_FILE(file))
        return FALSE;
    iface = FM_FILE_GET_IFACE(file);
    return iface->wants_incremental ? iface->wants_incremental(file) : FALSE;
}

/**
 * fm_file_new_for_uri
 * @uri: a UTF8 string containing a URI
 *
 * Constructs a #GFile for a given URI. This operation never fails,
 * but the returned object might not support any I/O operation if @uri
 * is malformed or if the uri type is not supported.
 *
 * Returns: a new #GFile.
 *
 * Since: 1.0.2
 */
GFile *fm_file_new_for_uri(const char *uri)
{
    char *scheme;
    FmFileInterface *iface;
    GFile *file = NULL;

    scheme = g_uri_parse_scheme(uri);
    if(scheme)
    {
        iface = fm_find_scheme(scheme);
        if(iface && iface->new_for_uri_name)
            file = iface->new_for_uri_name(&uri[strlen(scheme)+1]);
        g_free(scheme);
        if(file)
            return file;
    }
    return g_file_new_for_uri(uri);
}

/**
 * fm_file_new_for_commandline_arg
 * @arg: a command line string
 *
 * Creates a #GFile with the given argument from the command line.
 * The value of @arg can be either a URI, an absolute path or
 * a relative path resolved relative to the current working directory.
 * This operation never fails, but the returned object might not support
 * any I/O operation if @arg points to a malformed path.
 *
 * Returns: a new #GFile.
 *
 * Since: 1.0.2
 */
GFile *fm_file_new_for_commandline_arg(const char *arg)
{
    char *scheme;
    FmFileInterface *iface;
    GFile *file = NULL;

    scheme = g_uri_parse_scheme(arg);
    if(scheme)
    {
        iface = fm_find_scheme(scheme);
        if(iface && iface->new_for_uri_name)
            file = iface->new_for_uri_name(&arg[strlen(scheme)+1]);
        g_free(scheme);
        if(file)
            return file;
    }
    return g_file_new_for_commandline_arg(arg);
}

void _fm_file_init(void)
{
    schemes = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    //fm_add_scheme("menu", _fm_scheme_menu_interface);
    //fm_add_scheme("search", _fm_scheme_search_interface);
    //fm_module_register_type("scheme", FM_FILE_MODULE_MIN_VERSION,
    //                        FM_FILE_MODULE_MAX_VERSION, fm_file_add_module);
}

void _fm_file_finalize(void)
{
    g_hash_table_destroy(schemes);
    schemes = NULL;
}
