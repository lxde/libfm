/*
 *      fm-places-view.c
 *      
 *      Copyright 2009 PCMan <pcman@thinkpad>
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

#include <glib/gi18n.h>
#include "fm-places-view.h"
#include "fm-gtk-utils.h"

enum
{
    CHDIR,
    N_SIGNALS
};

enum
{
    COL_ICON,
    COL_LABEL,
    COL_INFO,
    N_COLS
};

typedef enum
{
    PLACE_NONE,
    PLACE_PATH,
    PLACE_VOL,
}PlaceType;

typedef struct _PlaceItem
{
    PlaceType type;
    GIcon* icon;
    union
    {
        GVolume* vol;
        FmPath* path;
    };
}PlaceItem;

static void fm_places_view_finalize  			(GObject *object);

static void on_row_activated( GtkTreeView* view, GtkTreePath* tree_path, GtkTreeViewColumn *col);
static gboolean on_button_release(GtkWidget* view, GdkEventButton* evt);

G_DEFINE_TYPE(FmPlacesView, fm_places_view, GTK_TYPE_TREE_VIEW);

static GtkListStore* model = NULL;
static GVolumeMonitor* vol_mon = NULL;
static guint signals[N_SIGNALS];

static void fm_places_view_class_init(FmPlacesViewClass *klass)
{
	GObjectClass *g_object_class;
    GtkWidgetClass* widget_class;
    GtkTreeViewClass* tv_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_places_view_finalize;

    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->button_release_event = on_button_release;

    tv_class = GTK_TREE_VIEW_CLASS(klass);
    tv_class->row_activated = on_row_activated;

    signals[CHDIR] =
        g_signal_new("chdir",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(FmPlacesViewClass, chdir),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);
}


static void place_item_free(PlaceItem* item)
{
    switch(item->type)
    {
    case PLACE_PATH:
        fm_path_unref(item->path);
        break;
    case PLACE_VOL:
        g_object_unref(item->vol);
        break;
    }
    g_slice_free(PlaceItem, item);
}

static void fm_places_view_finalize(GObject *object)
{
	FmPlacesView *self;
    GtkTreeIter it;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_PLACES_VIEW(object));

	self = FM_PLACES_VIEW(object);
    g_object_unref(vol_mon);

    if(gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            PlaceItem* item;
            gtk_tree_model_get(model, COL_INFO, &item, -1);
            place_item_free(item);
        }while(gtk_tree_model_iter_next(model, &it));
    }
    g_object_unref(model);

	G_OBJECT_CLASS(fm_places_view_parent_class)->finalize(object);
}


static void init_model()
{
    if(G_UNLIKELY(!model))
    {
        GtkTreeIter it;
        PlaceItem* item;
        GList *vols, *l;
        GIcon* icon;
        model = gtk_list_store_new(N_COLS, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_POINTER);
        g_object_add_weak_pointer(model, &model);

        item = g_slice_new0(PlaceItem);
        item->type = PLACE_PATH;
        item->path = fm_path_get_home();
        item->icon = g_themed_icon_new("user-home");
        gtk_list_store_append(model, &it);
        gtk_list_store_set(model, &it, COL_ICON, item->icon, COL_LABEL, item->path->name, COL_INFO, item, -1);

        item = g_slice_new0(PlaceItem);
        item->type = PLACE_PATH;
        item->path = fm_path_get_desktop();
        item->icon = g_themed_icon_new("user-desktop");
        gtk_list_store_append(model, &it);
        gtk_list_store_set(model, &it, COL_ICON, item->icon, COL_LABEL, _("Desktop"), COL_INFO, item, -1);

        item = g_slice_new0(PlaceItem);
        item->type = PLACE_PATH;
        item->path = fm_path_get_trash();
        item->icon = g_themed_icon_new("user-trash");
        gtk_list_store_append(model, &it);
        gtk_list_store_set(model, &it, COL_ICON, item->icon, COL_LABEL, _("Trash"), COL_INFO, item, -1);

        item = g_slice_new0(PlaceItem);
        item->type = PLACE_PATH;
        item->path = fm_path_new("applications:///");
        item->icon = g_themed_icon_new("system-software-install");
        gtk_list_store_append(model, &it);
        gtk_list_store_set(model, &it, COL_ICON, item->icon, COL_LABEL, _("Applications"), COL_INFO, item, -1);

        /* volumes */
        vol_mon = g_volume_monitor_get();
        // g_signal_connect
        g_object_add_weak_pointer(vol_mon, &vol_mon);
        vols = g_volume_monitor_get_volumes(vol_mon);
        for(l=vols;l;l=l->next)
        {
            GVolume* vol = G_VOLUME(l->data);
            char* name;
            item = g_slice_new0(PlaceItem);
            item->type = PLACE_VOL;
            item->vol = vol;
            name = g_volume_get_name(vol);
            icon = g_volume_get_icon(vol);
            gtk_list_store_append(model, &it);
            gtk_list_store_set(model, &it, COL_ICON, icon, COL_LABEL, name, COL_INFO, item, -1);
            g_object_unref(icon);
            g_free(name);
        }
        g_list_free(vols);

        /* separator */
        gtk_list_store_append(model, &it);

//        gtk_list_store_append(model, &it);
//        gtk_list_store_set(model, &it, COL_ICON, icon, );
    }
    else
        g_object_ref(model);
    return model;
}

static gboolean sep_func( GtkTreeModel* model, GtkTreeIter* it, gpointer data )
{
    PlaceItem* item;
    gtk_tree_model_get(model, it, COL_INFO, &item, -1);
    return item == NULL;
}

static void fm_places_view_init(FmPlacesView *self)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;

    init_model();
    gtk_tree_view_set_model(self, model);
    g_object_unref(model);

    gtk_tree_view_set_headers_visible(self, FALSE);
    gtk_tree_view_set_row_separator_func(self, (GtkTreeViewRowSeparatorFunc)sep_func, NULL, NULL );

    col = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start( col, renderer, FALSE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "gicon", COL_ICON, NULL );

    renderer = gtk_cell_renderer_text_new();
//    g_signal_connect( renderer, "edited", G_CALLBACK(on_bookmark_edited), view );
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "text", COL_LABEL, NULL );
    gtk_tree_view_append_column ( self, col );
//    g_signal_connect( view, "button-press-event", G_CALLBACK( on_button_press_event ), NULL );
}


GtkWidget *fm_places_view_new(void)
{
	return g_object_new(FM_PLACES_VIEW_TYPE, NULL);
}

void on_row_activated(GtkTreeView* view, GtkTreePath* tree_path, GtkTreeViewColumn *col)
{
    GtkTreeIter it;
    if(gtk_tree_model_get_iter(model, &it, tree_path))
    {
        PlaceItem* item;
        FmPath* path;
        gtk_tree_model_get(model, &it, COL_INFO, &item, -1);
        switch(item->type)
        {
        case PLACE_PATH:
            path = fm_path_ref(item->path);
            break;
        case PLACE_VOL:
        {
            GFile* gf;
            GMount* mnt = g_volume_get_mount(item->vol);
            if(!mnt)
            {
                if(!fm_mount_volume(NULL, item->vol))
                    return;
                mnt = g_volume_get_mount(item->vol);
            }
            gf = g_mount_get_root(mnt);
            g_object_unref(mnt);
            path = fm_path_new_for_gfile(gf);
            g_object_unref(gf);
            break;
        }
        default:
            return;
        }
        g_signal_emit(view, signals[CHDIR], 0, path);
        fm_path_unref(path);
    }
}

void fm_places_select(FmPlacesView* pv, FmPath* path)
{

}

gboolean on_button_release(GtkWidget* view, GdkEventButton* evt)
{
    GtkTreePath* tp;
    GtkTreeViewColumn* col;
    if(evt->button == 1 && gtk_tree_view_get_path_at_pos(view, evt->x, evt->y, &tp, &col, NULL, NULL))
    {
        gtk_tree_view_row_activated(view, tp, col);
        gtk_tree_path_free(tp);
    }
    return GTK_WIDGET_CLASS(fm_places_view_parent_class)->button_release_event(view, evt);
}
