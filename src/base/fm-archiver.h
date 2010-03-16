/*
 *      fm-archiver.h
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
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

/* handles integration between libfm and some well-known GUI archivers,
 * such as file-roller, xarchiver, and squeeze. */

#ifndef __FM_ARCHIVER_H__
#define __FM_ARCHIVER_H__

#include <glib.h>
#include <gio/gio.h>
#include "fm-path.h"

G_BEGIN_DECLS

typedef struct _FmArchiver FmArchiver;
struct _FmArchiver
{
    char* program;
    char* create_cmd;
    char* extract_cmd;
    char* extract_to_cmd;
    char** mime_types;
};

void _fm_archiver_init();
void _fm_archiver_finalize();

gboolean fm_archiver_is_mime_type_supported(FmArchiver* archiver, const char* type);

gboolean fm_archiver_create_archive(FmArchiver* archiver, GAppLaunchContext* ctx, FmPathList* files);

gboolean fm_archiver_extract_archives(FmArchiver* archiver, GAppLaunchContext* ctx, FmPathList* files);

gboolean fm_archiver_extract_archives_to(FmArchiver* archiver, GAppLaunchContext* ctx, FmPathList* files, FmPath* dest_dir);

/* get default GUI archivers used by libfm */
FmArchiver* fm_archiver_get_default();

/* set default GUI archivers used by libfm */
void fm_archiver_set_default(FmArchiver* archiver);

/* get a list of FmArchiver* of all GUI archivers known to libfm */
GList* fm_archiver_get_all();

G_END_DECLS

#endif /* __FM_ARCHIVER_H__ */
