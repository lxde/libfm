/*
 *      fm-places-model.h
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

#ifndef __FM_PLACES_MODEL_H__
#define __FM_PLACES_MODEL_H__

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "fm-config.h"
#include "fm-gtk-utils.h"
#include "fm-bookmarks.h"
#include "fm-monitor.h"
#include "fm-icon-pixbuf.h"
#include "fm-file-info-job.h"

G_BEGIN_DECLS


#define FM_TYPE_PLACES_MODEL                (fm_places_model_get_type())
#define FM_PLACES_MODEL(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_TYPE_PLACES_MODEL, FmPlacesModel))
#define FM_PLACES_MODEL_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_TYPE_PLACES_MODEL, FmPlacesModelClass))
#define FM_IS_PLACES_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_TYPE_PLACES_MODEL))
#define FM_IS_PLACES_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_TYPE_PLACES_MODEL))
#define FM_PLACES_MODEL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),\
            FM_TYPE_PLACES_MODEL, FmPlacesModelClass))

typedef struct _FmPlacesModel            FmPlacesModel;
typedef struct _FmPlacesModelClass        FmPlacesModelClass;

enum
{
    FM_PLACES_MODEL_COL_ICON,
    FM_PLACES_MODEL_COL_LABEL,
    FM_PLACES_MODEL_COL_INFO,
    FM_PLACES_MODEL_N_COLS
};

typedef enum
{
    FM_PLACES_ITEM_NONE,
    FM_PLACES_ITEM_PATH,
    FM_PLACES_ITEM_VOL,
}FmPlaceType;

typedef struct _FmPlaceItem
{
    FmPlaceType type : 6;
    gboolean vol_mounted : 1;
    FmFileInfo* fi;
    union
    {
        GVolume* vol;
        FmBookmarkItem* bm_item;
    };
}FmPlaceItem;

struct _FmPlacesModel
{
    GtkListStore parent;

    GVolumeMonitor* vol_mon;
    FmBookmarks* bookmarks;
    GtkTreeIter sep_it;
    GtkTreePath* sep_tp;
    GtkTreeIter trash_it;
    GFileMonitor* trash_monitor;
    guint trash_idle;
    guint theme_change_handler;
    guint use_trash_change_handler;
    guint pane_icon_size_change_handler;
    GdkPixbuf* eject_icon;

    GSList* jobs;
};

struct _FmPlacesModelClass
{
    GtkListStoreClass parent_class;
};


GType fm_places_model_get_type        (void);
GtkListStore* fm_places_model_new            (void);

const GtkTreePath* fm_places_model_get_separator_path(FmPlacesModel* model);

gboolean fm_places_model_iter_is_separator(FmPlacesModel* model, GtkTreeIter* it);

gboolean fm_places_model_path_is_separator(FmPlacesModel* model, GtkTreePath* tp);
gboolean fm_places_model_path_is_bookmark(FmPlacesModel* model, GtkTreePath* tp);
gboolean fm_places_model_path_is_places(FmPlacesModel* model, GtkTreePath* tp);

void fm_places_model_mount_indicator_cell_data_func(GtkCellLayout *cell_layout,
                                           GtkCellRenderer *render,
                                           GtkTreeModel *tree_model,
                                           GtkTreeIter *it,
                                           gpointer user_data);

gboolean fm_places_model_find_path(FmPlacesModel* model, GtkTreeIter* iter, FmPath* path);

G_END_DECLS

#endif /* __FM_PLACES_MODEL_H__ */
