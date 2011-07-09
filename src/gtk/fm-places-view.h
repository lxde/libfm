/*
 *      fm-places-view.h
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

#ifndef __FM_PLACES_VIEW_H__
#define __FM_PLACES_VIEW_H__

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "fm-path.h"
#include "fm-dnd-dest.h"

G_BEGIN_DECLS

#define FM_PLACES_VIEW_TYPE             (fm_places_view_get_type())
#define FM_PLACES_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_PLACES_VIEW_TYPE, FmPlacesView))
#define FM_PLACES_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_PLACES_VIEW_TYPE, FmPlacesViewClass))
#define IS_FM_PLACES_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_PLACES_VIEW_TYPE))
#define IS_FM_PLACES_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_PLACES_VIEW_TYPE))

typedef struct _FmPlacesView            FmPlacesView;
typedef struct _FmPlacesViewClass       FmPlacesViewClass;

struct _FmPlacesView
{
    GtkTreeView parent;
    FmDndDest* dnd_dest;
    GtkTreePath* clicked_row;
    GtkCellRendererPixbuf* mount_indicator_renderer;
};

struct _FmPlacesViewClass
{
    GtkTreeViewClass parent_class;
    void (*chdir)(FmPlacesView* view, guint button, FmPath* path);
};

GType       fm_places_view_get_type     (void);
GtkWidget*  fm_places_view_new          (void);
void fm_places_chdir(FmPlacesView* pv, FmPath* path);

G_END_DECLS

#endif /* __FM_PLACES_VIEW_H__ */
