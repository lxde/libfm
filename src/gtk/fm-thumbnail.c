/*
 *      fm-thumbnail.c
 *
 *      Copyright 2010 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
 * SECTION:fm-thumbnail
 * @short_description: A thumbnails cache loader and generator.
 * @title: FmThumbnailRequest
 *
 * @include: libfm/fm-gtk.h
 *
 * This API allows to generate thumbnails for files and save them on
 * disk then use that cache next time to display them.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "fm-thumbnail.h"

/* FIXME: this function prototype seems to be missing in header files of GdkPixbuf. Bug report to them. */
gboolean gdk_pixbuf_set_option(GdkPixbuf *pixbuf, const gchar *key, const gchar *value);

/**
 * fm_thumbnail_request
 * @src_file: an image file
 * @size: thumbnail size
 * @callback: callback to requestor
 * @user_data: data provided for @callback
 *
 * Schedules loading/generation of thumbnail for @src_file. If the
 * request isn't cancelled then ready thumbnail will be given to the
 * requestor in @callback. Returned descriptor can be used to cancel
 * the job.
 *
 * Returns: (transfer none): request descriptor.
 *
 * Since: 0.1.0
 */
/* in main loop */
FmThumbnailRequest* fm_thumbnail_request(FmFileInfo* src_file,
                                         guint size,
                                         FmThumbnailReadyCallback callback,
                                         gpointer user_data)
{
    return fm_thumbnail_loader_load(src_file, size, callback, user_data);
}

/**
 * fm_thumbnail_request_cancel
 * @req: the request descriptor
 *
 * Cancels request. After return from this call the @req becomes invalid
 * and cannot be used. Caller will never get callback for cancelled
 * request either.
 *
 * Since: 0.1.0
 */
/* in main loop */
void fm_thumbnail_request_cancel(FmThumbnailRequest* req)
{
    fm_thumbnail_result_cancel(req);
}

/**
 * fm_thumbnail_request_get_pixbuf
 * @req: request descriptor
 *
 * Retrieves loaded thumbnail. Returned data are owned by @req and should
 * be not freed by caller.
 *
 * Returns: (transfer none): thumbnail.
 *
 * Since: 0.1.0
 */
/* in main loop */
GdkPixbuf* fm_thumbnail_request_get_pixbuf(FmThumbnailRequest* req)
{
    return GDK_PIXBUF(fm_thumbnail_result_get_data(req));
}

/**
 * fm_thumbnail_request_get_file_info
 * @req: request descriptor
 *
 * Retrieves file descriptor that request is for. Returned data are
 * owned by @req and should be not freed by caller.
 *
 * Returns: (transfer none): file descriptor.
 *
 * Since: 0.1.0
 */
/* in main loop */
FmFileInfo* fm_thumbnail_request_get_file_info(FmThumbnailRequest* req)
{
    return fm_thumbnail_result_get_file_info(req);
}

/**
 * fm_thumbnail_request_get_size
 * @req: request descriptor
 *
 * Retrieves thumbnail size that request is for.
 *
 * Returns: size in pixels.
 *
 * Since: 0.1.0
 */
/* in main loop */
guint fm_thumbnail_request_get_size(FmThumbnailRequest* req)
{
    return fm_thumbnail_result_get_size(req);
}

static GObject* read_image_from_file(const char* filename) {
    return (GObject*)gdk_pixbuf_new_from_file(filename, NULL);
}

static GObject* read_image_from_stream(GInputStream* stream, guint64 len, GCancellable* cancellable)
{
    return (GObject*)gdk_pixbuf_new_from_stream(stream, cancellable, NULL);
}

static gboolean write_image(GObject* image, const char* filename, const char* uri, const char* mtime)
{
    return gdk_pixbuf_save(GDK_PIXBUF(image), filename, "png", NULL,
                     "tEXt::Thumb::URI", uri,
                     "tEXt::Thumb::MTime", mtime, NULL );
}

static GObject* scale_image(GObject* ori_pix, int new_width, int new_height)
{
    return (GObject*)gdk_pixbuf_scale_simple(GDK_PIXBUF(ori_pix), new_width, new_height, GDK_INTERP_BILINEAR);
}

static int get_image_width(GObject* image)
{
    return gdk_pixbuf_get_width(GDK_PIXBUF(image));
}

static int get_image_height(GObject* image)
{
    return gdk_pixbuf_get_height(GDK_PIXBUF(image));
}

static char* get_image_text(GObject* image, const char* key)
{
    return g_strdup(gdk_pixbuf_get_option(GDK_PIXBUF(image), key));
}

static GObject* rotate_image(GObject* image, int degree)
{
	return (GObject*)gdk_pixbuf_rotate_simple(GDK_PIXBUF(image), (GdkPixbufRotation)degree);
}

ThumbnailLoaderBackend gtk_backend = {
    read_image_from_file,
    read_image_from_stream,
    write_image,
    scale_image,
    rotate_image,
    get_image_width,
    get_image_height,
    get_image_text
};

/* in main loop */
void _fm_thumbnail_init()
{
    fm_thumbnail_loader_set_backend(&gtk_backend);
}

void _fm_thumbnail_finalize(void)
{
}
