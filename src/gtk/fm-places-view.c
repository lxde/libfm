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
#include "fm-bookmarks.h"
#include "fm-file-menu.h"

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

/* FIXME: need to replace GIcon with FmIcon later */
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
static gboolean on_button_press(GtkWidget* view, GdkEventButton* evt);
static gboolean on_button_release(GtkWidget* view, GdkEventButton* evt);

static void on_mount(GtkAction* act, gpointer user_data);
static void on_umount(GtkAction* act, gpointer user_data);
static void on_eject(GtkAction* act, gpointer user_data);

G_DEFINE_TYPE(FmPlacesView, fm_places_view, GTK_TYPE_TREE_VIEW);

static GtkListStore* model = NULL;
static GVolumeMonitor* vol_mon = NULL;
static FmBookmarks* bookmarks = NULL;
static GtkTreeIter sep_it = {0};

static guint signals[N_SIGNALS];

const char vol_menu_xml[]=
"<popup>"
  "<placeholder name='ph3'>"
  "<menuitem action='Mount'/>"
  "<menuitem action='Unmount'/>"
  "<menuitem action='Eject'/>"
  "</placeholder>"
"</popup>";

GtkActionEntry vol_menu_actions[]=
{
    {"Mount", NULL, N_("Mount Volume"), NULL, NULL, on_mount},
    {"Unmount", NULL, N_("Unmount Volume"), NULL, NULL, on_umount},
    {"Eject", NULL, N_("Eject Removable Media"), NULL, NULL, on_eject}
};

const char bookmark_menu_xml[]=
"<popup>"
  "<placeholder name='ph3'>"
  "<menuitem action='RenameBm'/>"
  "<menuitem action='RemoveBm'/>"
  "</placeholder>"
"</popup>";

GtkActionEntry bm_menu_actions[]=
{
    {"RenameBm", NULL, N_("Rename Bookmark Item"), NULL, NULL, NULL},
    {"RemoveBm", NULL, N_("Remove from Bookmark"), NULL, NULL, NULL}
};


static void fm_places_view_class_init(FmPlacesViewClass *klass)
{
	GObjectClass *g_object_class;
    GtkWidgetClass* widget_class;
    GtkTreeViewClass* tv_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_places_view_finalize;

    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->button_press_event = on_button_press;
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

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_PLACES_VIEW(object));

	self = FM_PLACES_VIEW(object);

	G_OBJECT_CLASS(fm_places_view_parent_class)->finalize(object);
}

static void on_model_destroy(gpointer unused, GObject* _model)
{
    GtkTreeIter it;
    if(gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            PlaceItem* item;
            gtk_tree_model_get(model, &it, COL_INFO, &item, -1);
            if(item)
                place_item_free(item);
        }while(gtk_tree_model_iter_next(model, &it));
    }
    model = NULL;

    g_object_unref(vol_mon);
    vol_mon = NULL;

    memset(&sep_it, 0, sizeof(GtkTreeIter));
}

static void update_vol(PlaceItem* item, GtkTreeIter* it)
{
    char* name;
    name = g_volume_get_name(item->vol);
    if(item->icon)
        g_object_unref(item->icon);
    item->icon = g_volume_get_icon(item->vol);
    gtk_list_store_set(model, it, COL_ICON, item->icon, COL_LABEL, name, -1);
    g_free(name);
}

static void add_vol(GVolume* vol)
{
    GtkTreeIter it;
    PlaceItem* item;
    item = g_slice_new0(PlaceItem);
    item->type = PLACE_VOL;
    item->vol = (GVolume*)g_object_ref(vol);
    gtk_list_store_insert_before(model, &it, &sep_it);
    gtk_list_store_set(model, &it, COL_INFO, item, -1);
    update_vol(item, &it);
}

static PlaceItem* find_vol(GVolume* vol, GtkTreeIter* _it)
{
    GtkTreeIter it;
    PlaceItem* item;
    /* FIXME: don't need to find from the first iter */
    if(gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            PlaceItem* item;
            gtk_tree_model_get(model, &it, COL_INFO, &item, -1);

            if(item->type == PLACE_VOL && item->vol == vol)
            {
                *_it = it;
                return item;
            }
        }while(it.user_data != sep_it.user_data && gtk_tree_model_iter_next(model, &it));
    }
    return NULL;
}

static void on_vol_added(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    /* g_debug("add vol: %p, uuid: %s, udi: %s", vol, g_volume_get_identifier(vol, "uuid"), g_volume_get_identifier(vol, "hal-udi")); */
    add_vol(vol);
}

static void on_vol_removed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    PlaceItem* item;
    GtkTreeIter it;
    item = find_vol(vol, &it);
    /* g_debug("remove vol: %p, uuid: %s, udi: %s", vol, g_volume_get_identifier(vol, "uuid"), g_volume_get_identifier(vol, "hal-udi")); */
    if(item)
        gtk_list_store_remove(model, &it);
}

static void on_vol_changed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    PlaceItem* item;
    GtkTreeIter it;
    item = find_vol(vol, &it);
    if(item)
        update_vol(item, &it);
}

static void init_model()
{
    if(G_UNLIKELY(!model))
    {
        GtkTreeIter it;
        PlaceItem* item;
        GList *vols, *bms, *l;
        GIcon* icon;
        model = gtk_list_store_new(N_COLS, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_POINTER);
        g_object_weak_ref(model, on_model_destroy, NULL);

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
        g_signal_connect(vol_mon, "volume-added", G_CALLBACK(on_vol_added), NULL);
        g_signal_connect(vol_mon, "volume-removed", G_CALLBACK(on_vol_removed), NULL);
        g_signal_connect(vol_mon, "volume-changed", G_CALLBACK(on_vol_changed), NULL);
        g_object_add_weak_pointer(vol_mon, &vol_mon);

        /* separator */
        gtk_list_store_append(model, &sep_it);

        vols = g_volume_monitor_get_volumes(vol_mon);
        for(l=vols;l;l=l->next)
        {
            GVolume* vol = G_VOLUME(l->data);
            add_vol(vol);
            g_object_unref(vol);
        }
        g_list_free(vols);

        bookmarks = fm_bookmarks_get(); /* bookmarks */
        bms = fm_bookmarks_list_all(bookmarks);
        for(l=bms;l;l=l->next)
        {
            FmBookmarkItem* bm = (FmBookmarkItem*)l->data;
            item = g_slice_new0(PlaceItem);
            item->type = PLACE_PATH;
            item->path = fm_path_ref(bm->path);
            item->icon = g_themed_icon_new("folder"); /* FIXME: get from FmIcon's cache */
            gtk_list_store_append(model, &it);
            gtk_list_store_set(model, &it, COL_ICON, item->icon, COL_LABEL, bm->name, COL_INFO, item, -1);
        }
    }
    else
        g_object_ref(model);
}

static gboolean sep_func( GtkTreeModel* model, GtkTreeIter* it, gpointer data )
{
    return it->user_data == sep_it.user_data;
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
        if(!item)
            return;
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

GtkWidget* place_item_get_menu(PlaceItem* item)
{
    GtkWidget* menu = NULL;
    FmFileMenu* file_menu;
    GtkUIManager* ui = gtk_ui_manager_new();
    GtkActionGroup* act_grp = act_grp = gtk_action_group_new("Popup");

    /* FIXME: merge with FmFileMenu when possible */
    if(item->type == PLACE_PATH)
    {
        gtk_action_group_add_actions(act_grp, bm_menu_actions, G_N_ELEMENTS(bm_menu_actions), item);
        gtk_ui_manager_add_ui_from_string(ui, bookmark_menu_xml, -1, NULL);
    }
    else if(item->type == PLACE_VOL)
    {
        GtkAction* act;
        GMount* mnt;
        gtk_action_group_add_actions(act_grp, vol_menu_actions, G_N_ELEMENTS(vol_menu_actions), item);
        gtk_ui_manager_add_ui_from_string(ui, vol_menu_xml, -1, NULL);

        mnt = g_volume_get_mount(item->vol);
        if(mnt) /* mounted */
        {
            g_object_unref(mnt);
            act = gtk_action_group_get_action(act_grp, "Mount");
            gtk_action_set_sensitive(act, FALSE);
        }
        else /* not mounted */
        {
            act = gtk_action_group_get_action(act_grp, "Unmount");
            gtk_action_set_sensitive(act, FALSE);
        }

        if(g_volume_can_eject(item->vol))
            act = gtk_action_group_get_action(act_grp, "Unmount");
        else
            act = gtk_action_group_get_action(act_grp, "Eject");
        gtk_action_set_visible(act, FALSE);
    }
    else
        goto _out;
    gtk_ui_manager_insert_action_group(ui, act_grp, 0);

    menu = gtk_ui_manager_get_widget(ui, "/popup");
    if(menu)
    {
        g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);
        g_object_weak_ref(menu, g_object_unref, g_object_ref(ui));
    }

_out:
    g_object_unref(act_grp);
    g_object_unref(ui);
    return menu;
}

gboolean on_button_press(GtkWidget* view, GdkEventButton* evt)
{
    GtkTreePath* tp;
    GtkTreeViewColumn* col;
    gboolean ret = GTK_WIDGET_CLASS(fm_places_view_parent_class)->button_press_event(view, evt);

    if(evt->button == 3 && gtk_tree_view_get_path_at_pos(view, evt->x, evt->y, &tp, &col, NULL, NULL))
    {
        GtkTreeIter it;
        if(gtk_tree_model_get_iter(model, &it, tp))
        {
            PlaceItem* item;
            GtkWidget* menu;
            gtk_tree_model_get(model, &it, COL_INFO, &item, -1);
            menu = place_item_get_menu(item);
            if(menu)
                gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 3, evt->time);
        }
        gtk_tree_path_free(tp);
    }
    return ret;
}

void on_mount(GtkAction* act, gpointer user_data)
{
    PlaceItem* item = (PlaceItem*)user_data;
    GMount* mnt = g_volume_get_mount(item->vol);
    if(!mnt)
    {
        if(!fm_mount_volume(NULL, item->vol))
            return;
    }
}

void on_umount(GtkAction* act, gpointer user_data)
{
    PlaceItem* item = (PlaceItem*)user_data;
    GMount* mnt = g_volume_get_mount(item->vol);
    if(mnt)
    {
        fm_unmount_mount(NULL, mnt);
        g_object_unref(mnt);
    }
}

void on_eject(GtkAction* act, gpointer user_data)
{
    PlaceItem* item = (PlaceItem*)user_data;
    fm_eject_volume(NULL, item->vol);
}
