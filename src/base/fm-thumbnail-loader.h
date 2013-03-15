/*
 * fm-thumbnail-loader.h
 * 
 * Copyright 2013 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#ifndef __FM_THUMBNAIL_LOADER_H__
#define __FM_THUMBNAIL_LOADER_H__

#include <glib-object.h>
#include "fm-file-info.h"

G_BEGIN_DECLS

typedef struct _FmThumbnailResult FmThumbnailResult;

/**
 * FmThumbnailResultCallback:
 * @req: request descriptor
 * @data: user data provided when request was made
 *
 * The callback to requestor when thumbnail is ready.
 * Note that this call is done outside of GTK loop so if the callback
 * wants to use any GTK API it should call gdk_threads_enter() and
 * gdk_threads_leave() for safety.
 *
 * Since: 0.1.0
 */
typedef void (*FmThumbnailResultCallback)(FmThumbnailResult *req, gpointer data);

void _fm_thumbnail_loader_init();

void _fm_thumbnail_loader_finalize();

FmThumbnailResult* fm_thumbnail_loader_load(FmFileInfo* src_file,
                                    guint size,
                                    FmThumbnailResultCallback callback,
                                    gpointer user_data);

void fm_thumbnail_result_cancel(FmThumbnailResult* req);

GObject* fm_thumbnail_result_get_data(FmThumbnailResult* req);

FmFileInfo* fm_thumbnail_result_get_file_info(FmThumbnailResult* req);

guint fm_thumbnail_result_get_size(FmThumbnailResult* req);

/* for toolkit-specific image loading code */

typedef struct _ThumbnailLoaderBackend ThumbnailLoaderBackend;
struct _ThumbnailLoaderBackend {
    GObject* (*read_image_from_file)(const char* filename);
    GObject* (*read_image_from_stream)(GInputStream* stream, guint64 len, GCancellable* cancellable);
    gboolean (*write_image)(GObject* image, const char* filename, const char* uri, const char* mtime);
    GObject* (*scale_image)(GObject* ori_pix, int new_width, int new_height);
    GObject* (*rotate_image)(GObject* image, int degree);
    int (*get_image_width)(GObject* image);
    int (*get_image_height)(GObject* image);
    char* (*get_image_text)(GObject* image, const char* key);
    // const char* (*get_image_orientation)(GObject* image);
    // GObject* (*apply_orientation)(GObject* image);
};

void fm_thumbnail_loader_set_backend(ThumbnailLoaderBackend* _backend);

G_END_DECLS

#endif /* __FM_THUMBNAIL_LOADER_H__ */
