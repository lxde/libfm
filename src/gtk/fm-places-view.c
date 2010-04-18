/*
 *      fm-places-view.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include "fm-places-view.h"
#include "fm-config.h"
#include "fm-gtk-utils.h"
#include "fm-bookmarks.h"
#include "fm-file-menu.h"
#include "fm-monitor.h"
#include "fm-icon-pixbuf.h"
#include "fm-cell-renderer-pixbuf.h"
#include "fm-file-info-job.h"

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
    FmFileInfo* fi;
    union
    {
        GVolume* vol;
        FmBookmarkItem* bm_item;
    };
}PlaceItem;

static void fm_places_view_finalize  			(GObject *object);

static void on_row_activated( GtkTreeView* view, GtkTreePath* tree_path, GtkTreeViewColumn *col);
static gboolean on_button_press(GtkWidget* view, GdkEventButton* evt);
static gboolean on_button_release(GtkWidget* view, GdkEventButton* evt);

static void on_mount(GtkAction* act, gpointer user_data);
static void on_umount(GtkAction* act, gpointer user_data);
static void on_eject(GtkAction* act, gpointer user_data);

static void on_remove_bm(GtkAction* act, gpointer user_data);
static void on_rename_bm(GtkAction* act, gpointer user_data);
static void on_empty_trash(GtkAction* act, gpointer user_data);
static gboolean update_trash(gpointer user_data);

static gboolean on_dnd_dest_query_info(FmDndDest* dd, int x, int y,
                            			GdkDragAction* action, FmPlacesView* view);

static gboolean on_dnd_dest_files_dropped(FmDndDest* dd, GdkDragAction action,
                                       int info_type, FmFileInfoList* files, FmPlacesView* view);

static void on_trash_changed(GFileMonitor *monitor, GFile *gf, GFile *other, GFileMonitorEvent evt, gpointer user_data);
static void on_use_trash_changed(FmConfig* cfg, gpointer unused);
static void on_pane_icon_size_changed(FmConfig* cfg, gpointer unused);
static void create_trash();
static void update_icons();

static void on_vol_added(GVolumeMonitor* vm, GVolume* vol, gpointer user_data);
static void on_vol_removed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data);
static void on_vol_changed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data);
static void on_mount_added(GVolumeMonitor* vm, GMount* mount, gpointer user_data);

static void on_file_info_job_finished(FmFileInfoJob* job, gpointer user_data);


G_DEFINE_TYPE(FmPlacesView, fm_places_view, GTK_TYPE_TREE_VIEW);

static GtkListStore* model = NULL;
static GVolumeMonitor* vol_mon = NULL;
static FmBookmarks* bookmarks = NULL;
static GtkTreeIter sep_it = {0};
static GtkTreeIter trash_it = {0};
static GFileMonitor* trash_monitor = NULL;
static guint trash_idle = 0;
static guint theme_change_handler = 0;
static guint use_trash_change_handler = 0;
static guint pane_icon_size_change_handler = 0;

static GSList* jobs = NULL;

static guint signals[N_SIGNALS];

static const char vol_menu_xml[]=
"<popup>"
  "<placeholder name='ph3'>"
  "<menuitem action='Mount'/>"
  "<menuitem action='Unmount'/>"
  "<menuitem action='Eject'/>"
  "</placeholder>"
"</popup>";

static GtkActionEntry vol_menu_actions[]=
{
    {"Mount", NULL, N_("Mount Volume"), NULL, NULL, on_mount},
    {"Unmount", NULL, N_("Unmount Volume"), NULL, NULL, on_umount},
    {"Eject", NULL, N_("Eject Removable Media"), NULL, NULL, on_eject}
};

static const char bookmark_menu_xml[]=
"<popup>"
  "<placeholder name='ph3'>"
  "<menuitem action='RenameBm'/>"
  "<menuitem action='RemoveBm'/>"
  "</placeholder>"
"</popup>";

static GtkActionEntry bm_menu_actions[]=
{
    {"RenameBm", GTK_STOCK_EDIT, N_("Rename Bookmark Item"), NULL, NULL, G_CALLBACK(on_rename_bm)},
    {"RemoveBm", GTK_STOCK_REMOVE, N_("Remove from Bookmark"), NULL, NULL, G_CALLBACK(on_remove_bm)}
};

static const char trash_menu_xml[]=
"<popup>"
  "<placeholder name='ph3'>"
  "<menuitem action='EmptyTrash'/>"
  "</placeholder>"
"</popup>";

static GtkActionEntry trash_menu_actions[]=
{
    {"EmptyTrash", NULL, N_("Empty Trash"), NULL, NULL, G_CALLBACK(on_empty_trash)}
};

enum {
    FM_DND_DEST_TARGET_BOOOKMARK = N_FM_DND_DEST_DEFAULT_TARGETS + 1
};

GtkTargetEntry dnd_dest_targets[] =
{
    {"application/x-bookmark", GTK_TARGET_SAME_WIDGET, FM_DND_DEST_TARGET_BOOOKMARK}
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
    case PLACE_VOL:
        g_object_unref(item->vol);
        break;
    }
    fm_file_info_unref(item->fi);
    g_slice_free(PlaceItem, item);
}

static void fm_places_view_finalize(GObject *object)
{
	FmPlacesView *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_PLACES_VIEW(object));

	self = FM_PLACES_VIEW(object);
    if(self->dest_row)
        gtk_tree_path_free(self->dest_row);

	G_OBJECT_CLASS(fm_places_view_parent_class)->finalize(object);
}

static void on_model_destroy(gpointer unused, GObject* _model)
{
    GtkTreeIter it;
    if(jobs)
    {
        GSList* l;
        for(l = jobs; l; l=l->next)
        {
            fm_job_cancel(FM_JOB(l->data));
            g_object_unref(l->data);
        }
        g_slist_free(jobs);
        jobs = NULL;
    }

    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it))
    {
        do
        {
            PlaceItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, COL_INFO, &item, -1);
            if(G_LIKELY(item))
                place_item_free(item);
        }while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it));
    }
    model = NULL;

    g_signal_handler_disconnect(gtk_icon_theme_get_default(), theme_change_handler);
    theme_change_handler = 0;

    g_signal_handler_disconnect(fm_config, use_trash_change_handler);
    use_trash_change_handler = 0;

    g_signal_handler_disconnect(fm_config, pane_icon_size_change_handler);
    pane_icon_size_change_handler = 0;

    g_signal_handlers_disconnect_by_func(vol_mon, on_vol_added, NULL);
    g_signal_handlers_disconnect_by_func(vol_mon, on_vol_removed, NULL);
    g_signal_handlers_disconnect_by_func(vol_mon, on_vol_changed, NULL);
    g_signal_handlers_disconnect_by_func(vol_mon, on_mount_added, NULL);

    g_object_unref(vol_mon);
    vol_mon = NULL;

    if(trash_monitor)
    {
        g_signal_handlers_disconnect_by_func(trash_monitor, on_trash_changed, NULL);
        g_object_unref(trash_monitor);
        trash_monitor = NULL;
    }
    if(trash_idle)
        g_source_remove(trash_idle);
    trash_idle = 0;

    memset(&trash_it, 0, sizeof(GtkTreeIter));
    memset(&sep_it, 0, sizeof(GtkTreeIter));
}

static void update_vol(PlaceItem* item, GtkTreeIter* it)
{
    FmIcon* icon;
    GIcon* gicon;
    char* name;
    GdkPixbuf* pix;
    GMount* mount;

    name = g_volume_get_name(item->vol);
    if(item->fi->icon)
        fm_icon_unref(item->fi->icon);
    gicon = g_volume_get_icon(item->vol);
    icon = fm_icon_from_gicon(gicon);
    item->fi->icon = icon;
    g_object_unref(gicon);

    mount = g_volume_get_mount(item->vol);
    if(mount)
    {
        if(!item->fi->path)
        {
            GFile* gf = g_mount_get_root(mount);
            FmPath* path = fm_path_new_for_gfile(gf);
            g_object_unref(gf);
            item->fi->path = path;
        }
        g_object_unref(mount);
    }
    else
    {
        if(item->fi->path)
        {
            fm_path_unref(item->fi->path);
            item->fi->path = NULL;
        }
    }
    /*
     get mount path here
    if(job && item->fi->path)
        fm_file_info_job_add(job, item->fi->path);
     */

    pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
    gtk_list_store_set(model, it, COL_ICON, pix, COL_LABEL, name, -1);
    g_object_unref(pix);
    g_free(name);
}

static void add_vol(GVolume* vol, FmFileInfoJob* job)
{
    GtkTreeIter it;
    PlaceItem* item;
    item = g_slice_new0(PlaceItem);
    item->fi = fm_file_info_new();
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
    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it))
    {
        do
        {
            PlaceItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, COL_INFO, &item, -1);

            if(item && item->type == PLACE_VOL && item->vol == vol)
            {
                *_it = it;
                return item;
            }
        }while(it.user_data != sep_it.user_data && gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it));
    }
    return NULL;
}

void on_vol_added(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    g_debug("add vol: %p, uuid: %s, udi: %s", vol, g_volume_get_identifier(vol, "uuid"), g_volume_get_identifier(vol, "hal-udi"));
    add_vol(vol, NULL);
}

void on_vol_removed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    PlaceItem* item;
    GtkTreeIter it;
    item = find_vol(vol, &it);
    /* g_debug("remove vol: %p, uuid: %s, udi: %s", vol, g_volume_get_identifier(vol, "uuid"), g_volume_get_identifier(vol, "hal-udi")); */
    if(item)
    {
        gtk_list_store_remove(model, &it);
        place_item_free(item);
    }
}

void on_vol_changed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    PlaceItem* item;
    GtkTreeIter it;
    item = find_vol(vol, &it);
    if(item)
        update_vol(item, &it);
}

void on_mount_added(GVolumeMonitor* vm, GMount* mount, gpointer user_data)
{
    GVolume* vol = g_mount_get_volume(mount);
    if(vol)
    {
        PlaceItem *item;
        GtkTreeIter it;
        item = find_vol(vol, &it);
        if(item && item->type == PLACE_VOL && !item->fi->path)
        {
            GFile* gf = g_mount_get_root(mount);
            FmPath* path = fm_path_new_for_gfile(gf);
            g_debug("mount path: %s", path->name);
            g_object_unref(gf);
            item->fi->path = path;
        }
        g_object_unref(vol);
    }
}

static void add_bookmarks(FmFileInfoJob* job)
{
    PlaceItem* item;
    GList *bms, *l;
    FmIcon* icon = fm_icon_from_name("folder");
    FmIcon* remote_icon = NULL;
    GdkPixbuf* folder_pix = fm_icon_get_pixbuf(icon, fm_config->pane_icon_size);
    GdkPixbuf* remote_pix = NULL;
    bms = fm_bookmarks_list_all(bookmarks);
    for(l=bms;l;l=l->next)
    {
        FmBookmarkItem* bm = (FmBookmarkItem*)l->data;
        GtkTreeIter it;
        GdkPixbuf* pix;
        item = g_slice_new0(PlaceItem);
        item->type = PLACE_PATH;
        item->fi = fm_file_info_new();
        item->fi->path = fm_path_ref(bm->path);
        fm_file_info_job_add(job, item->fi->path);

        if(fm_path_is_native(item->fi->path))
        {
            item->fi->icon = fm_icon_ref(icon);
            pix = folder_pix;
        }
        else
        {
            if(G_UNLIKELY(!remote_icon))
            {
                remote_icon = fm_icon_from_name("folder-remote");
                remote_pix = fm_icon_get_pixbuf(remote_icon, fm_config->pane_icon_size);
            }
            item->fi->icon = fm_icon_ref(remote_icon);
            pix = remote_pix;
        }
        item->bm_item = bm;
        gtk_list_store_append(model, &it);
        gtk_list_store_set(model, &it, COL_ICON, pix, COL_LABEL, bm->name, COL_INFO, item, -1);
    }
    g_object_unref(folder_pix);
    fm_icon_unref(icon);
    if(remote_icon)
    {
        fm_icon_unref(remote_icon);
        if(remote_pix)
            g_object_unref(remote_pix);
    }
}

static void on_bookmarks_changed(FmBookmarks* bm, gpointer user_data)
{
    FmFileInfoJob* job = fm_file_info_job_new(NULL, FM_FILE_INFO_JOB_FOLLOW_SYMLINK);
    GtkTreeIter it = sep_it;
    /* remove all old bookmarks */
    if(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it))
    {
        while(gtk_list_store_remove(model, &it))
            continue;
    }
    add_bookmarks(job);

    g_signal_connect(job, "finished", G_CALLBACK(on_file_info_job_finished), NULL);
    jobs = g_slist_prepend(jobs, job);
    fm_job_run_async(job);
}

void create_trash()
{
    GtkTreeIter it;
    PlaceItem* item;
    GdkPixbuf* pix;
    GFile* gf;

    gf = g_file_new_for_uri("trash:///");
    trash_monitor = fm_monitor_directory(gf, NULL);
    g_signal_connect(trash_monitor, "changed", G_CALLBACK(on_trash_changed), NULL);
    g_object_unref(gf);

    item = g_slice_new0(PlaceItem);
    item->type = PLACE_PATH;
    item->fi = fm_file_info_new();
    item->fi->path = fm_path_ref(fm_path_get_trash());
    item->fi->icon = fm_icon_from_name("user-trash");
    gtk_list_store_insert(model, &it, 2);
    pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
    gtk_list_store_set(model, &it, COL_ICON, pix, COL_LABEL, _("Trash"), COL_INFO, item, -1);
    g_object_unref(pix);
    trash_it = it;

    if(0 == trash_idle)
        trash_idle = g_idle_add((GSourceFunc)update_trash, NULL);
}

static void init_model()
{
    if(G_UNLIKELY(!model))
    {
        GtkTreeIter it;
        PlaceItem* item;
        GList *vols, *l;
        GIcon* gicon;
        FmIcon* icon;
        GFile* gf;
        GdkPixbuf* pix;
        FmFileInfoJob* job = fm_file_info_job_new(NULL, FM_FILE_INFO_JOB_FOLLOW_SYMLINK);

        theme_change_handler = g_signal_connect(gtk_icon_theme_get_default(), "changed",
                                                G_CALLBACK(update_icons), NULL);

        use_trash_change_handler = g_signal_connect(fm_config, "changed::use_trash",
                                                 G_CALLBACK(on_use_trash_changed), NULL);

        pane_icon_size_change_handler = g_signal_connect(fm_config, "changed::pane_icon_size",
                                                 G_CALLBACK(on_pane_icon_size_changed), NULL);

        model = gtk_list_store_new(N_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
        g_object_weak_ref(G_OBJECT(model), on_model_destroy, NULL);

        item = g_slice_new0(PlaceItem);
        item->type = PLACE_PATH;
        item->fi = fm_file_info_new();
        item->fi->path = fm_path_ref(fm_path_get_home());
        item->fi->icon = fm_icon_from_name("user-home");
        gtk_list_store_append(model, &it);
        pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
        gtk_list_store_set(model, &it, COL_ICON, pix, COL_LABEL, item->fi->path->name, COL_INFO, item, -1);
        g_object_unref(pix);
        fm_file_info_job_add(job, item->fi->path);

        /* Only show desktop in side pane when the user has a desktop dir. */
        if(g_file_test(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP), G_FILE_TEST_IS_DIR))
        {
            item = g_slice_new0(PlaceItem);
            item->type = PLACE_PATH;
            item->fi = fm_file_info_new();
            item->fi->path = fm_path_ref(fm_path_get_desktop());
            item->fi->icon = fm_icon_from_name("user-desktop");
            gtk_list_store_append(model, &it);
            pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
            gtk_list_store_set(model, &it, COL_ICON, pix, COL_LABEL, _("Desktop"), COL_INFO, item, -1);
            g_object_unref(pix);
            fm_file_info_job_add(job, item->fi->path);
        }

        if(fm_config->use_trash)
            create_trash(); /* FIXME: how to handle trash bin? */

        item = g_slice_new0(PlaceItem);
        item->type = PLACE_PATH;
        item->fi = fm_file_info_new();
        item->fi->path = fm_path_ref(fm_path_get_apps_menu());
        item->fi->icon = fm_icon_from_name("system-software-install");
        gtk_list_store_append(model, &it);
        pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
        gtk_list_store_set(model, &it, COL_ICON, pix, COL_LABEL, _("Applications"), COL_INFO, item, -1);
        g_object_unref(pix);
        /* fm_file_info_job_add(job, item->fi->path); */

        /* volumes */
        vol_mon = g_volume_monitor_get();
        g_signal_connect(vol_mon, "volume-added", G_CALLBACK(on_vol_added), NULL);
        g_signal_connect(vol_mon, "volume-removed", G_CALLBACK(on_vol_removed), NULL);
        g_signal_connect(vol_mon, "volume-changed", G_CALLBACK(on_vol_changed), NULL);
        g_signal_connect(vol_mon, "mount-added", G_CALLBACK(on_mount_added), NULL);

        /* separator */
        gtk_list_store_append(model, &sep_it);

        /* add volumes to side-pane */
        vols = g_volume_monitor_get_volumes(vol_mon);
        for(l=vols;l;l=l->next)
        {
            GVolume* vol = G_VOLUME(l->data);
            add_vol(vol, job);
            g_object_unref(vol);
        }
        g_list_free(vols);

        bookmarks = fm_bookmarks_get(); /* bookmarks */
        g_signal_connect(bookmarks, "changed", G_CALLBACK(on_bookmarks_changed), NULL);

        /* add bookmarks to side pane */
        add_bookmarks(job);

        g_signal_connect(job, "finished", G_CALLBACK(on_file_info_job_finished), NULL);
        jobs = g_slist_prepend(jobs, job);
        fm_job_run_async(job);
    }
    else
        g_object_ref(model);
}

static gboolean sep_func( GtkTreeModel* model, GtkTreeIter* it, gpointer data )
{
    return it->user_data == sep_it.user_data;
}

static void on_renderer_icon_size_changed(FmConfig* cfg, gpointer user_data)
{
    FmCellRendererPixbuf* render = (FmCellRendererPixbuf*)user_data;
    fm_cell_renderer_pixbuf_set_fixed_size(render, fm_config->pane_icon_size, fm_config->pane_icon_size);
}

static void on_cell_renderer_pixbuf_destroy(gpointer user_data, GObject* render)
{
    g_signal_handler_disconnect(fm_config, GPOINTER_TO_UINT(user_data));
}

static void fm_places_view_init(FmPlacesView *self)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    GtkTargetList* targets;
    GdkPixbuf* pix;
    guint handler;

    init_model();
    gtk_tree_view_set_model(GTK_TREE_VIEW(self), model);
    g_object_unref(model);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);
    gtk_tree_view_set_row_separator_func(GTK_TREE_VIEW(self), (GtkTreeViewRowSeparatorFunc)sep_func, NULL, NULL );

    col = gtk_tree_view_column_new();
    renderer = fm_cell_renderer_pixbuf_new();
    handler = g_signal_connect(fm_config, "changed::pane_icon_size", G_CALLBACK(on_renderer_icon_size_changed), renderer);
    g_object_weak_ref(G_OBJECT(renderer), (GDestroyNotify)on_cell_renderer_pixbuf_destroy, GUINT_TO_POINTER(handler));
    fm_cell_renderer_pixbuf_set_fixed_size(FM_CELL_RENDERER_PIXBUF(renderer), fm_config->pane_icon_size, fm_config->pane_icon_size);

    gtk_tree_view_column_pack_start( col, renderer, FALSE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "pixbuf", COL_ICON, NULL );

    renderer = gtk_cell_renderer_text_new();
//    g_signal_connect( renderer, "edited", G_CALLBACK(on_bookmark_edited), view );
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "text", COL_LABEL, NULL );
    gtk_tree_view_append_column ( GTK_TREE_VIEW(self), col );

/*
    gtk_drag_source_set(fv->view, GDK_BUTTON1_MASK,
        fm_default_dnd_src_targets, N_FM_DND_SRC_DEFAULT_TARGETS,
        GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK|GDK_ACTION_ASK);
    fm_dnd_src_set_widget(fv->dnd_src, fv->view);
*/
    gtk_drag_dest_set(self, 0,
            fm_default_dnd_dest_targets, N_FM_DND_DEST_DEFAULT_TARGETS,
            GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK|GDK_ACTION_ASK);
    targets = gtk_drag_dest_get_target_list((GtkWidget*)self);
    /* add our own targets */
    gtk_target_list_add_table(targets, dnd_dest_targets, G_N_ELEMENTS(dnd_dest_targets));
    self->dnd_dest = fm_dnd_dest_new((GtkWidget*)self);
    g_signal_connect(self->dnd_dest, "query-info", G_CALLBACK(on_dnd_dest_query_info), self);
    g_signal_connect(self->dnd_dest, "files_dropped", G_CALLBACK(on_dnd_dest_files_dropped), self);
}


GtkWidget *fm_places_view_new(void)
{
	return g_object_new(FM_PLACES_VIEW_TYPE, NULL);
}

void on_row_activated(GtkTreeView* view, GtkTreePath* tree_path, GtkTreeViewColumn *col)
{
    GtkTreeIter it;
    if(gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &it, tree_path))
    {
        PlaceItem* item;
        FmPath* path;
        gtk_tree_model_get(GTK_TREE_MODEL(model), &it, COL_INFO, &item, -1);
        if(!item)
            return;
        switch(item->type)
        {
        case PLACE_PATH:
            path = fm_path_ref(item->fi->path);
            break;
        case PLACE_VOL:
        {
            GFile* gf;
            GMount* mnt = g_volume_get_mount(item->vol);
            if(!mnt)
            {
                GtkWindow* parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
                if(!fm_mount_volume(parent, item->vol, TRUE))
                    return;
                mnt = g_volume_get_mount(item->vol);
                if(!mnt)
                {
                    g_debug("GMount is invalid after successful g_volume_mount().\nThis is quite possible a gvfs bug.\nSee https://bugzilla.gnome.org/show_bug.cgi?id=552168");
                    return;
                }
            }
            gf = g_mount_get_root(mnt);
            g_object_unref(mnt);
            if(gf)
            {
                path = fm_path_new_for_gfile(gf);
                g_object_unref(gf);
            }
            else
                path = NULL;
            break;
        }
        default:
            return;
        }

        if(path)
        {
            g_signal_emit(view, signals[CHDIR], 0, path);
            fm_path_unref(path);
        }
    }
}

void fm_places_select(FmPlacesView* pv, FmPath* path)
{

}

gboolean on_button_release(GtkWidget* view, GdkEventButton* evt)
{
    GtkTreePath* tp;
    GtkTreeViewColumn* col;
    if(evt->button == 1 && gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view), evt->x, evt->y, &tp, &col, NULL, NULL))
    {
        gtk_tree_view_row_activated(GTK_TREE_VIEW(view), tp, col);
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
    gtk_action_group_set_translation_domain(act_grp, GETTEXT_PACKAGE);

    /* FIXME: merge with FmFileMenu when possible */
    if(item->type == PLACE_PATH)
    {
        if(item->bm_item)
        {
            gtk_action_group_add_actions(act_grp, bm_menu_actions, G_N_ELEMENTS(bm_menu_actions), item);
            gtk_ui_manager_add_ui_from_string(ui, bookmark_menu_xml, -1, NULL);
        }
        else if(fm_path_is_trash_root(item->fi->path))
        {
            gtk_action_group_add_actions(act_grp, trash_menu_actions, G_N_ELEMENTS(trash_menu_actions), item);
            gtk_ui_manager_add_ui_from_string(ui, trash_menu_xml, -1, NULL);
        }
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
        g_object_weak_ref(G_OBJECT(menu), g_object_unref, g_object_ref(ui));
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

    if(evt->button == 3 && gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view), evt->x, evt->y, &tp, &col, NULL, NULL))
    {
        GtkTreeIter it;
        if(gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &it, tp) && it.user_data != sep_it.user_data )
        {
            PlaceItem* item;
            GtkWidget* menu;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, COL_INFO, &item, -1);
            menu = place_item_get_menu(item);
            if(menu)
                gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, evt->time);
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
        if(!fm_mount_volume(NULL, item->vol, TRUE))
            return;
    }
    else
        g_object_unref(mnt);
}

void on_umount(GtkAction* act, gpointer user_data)
{
    PlaceItem* item = (PlaceItem*)user_data;
    GMount* mnt = g_volume_get_mount(item->vol);
    if(mnt)
    {
        fm_unmount_mount(NULL, mnt, TRUE);
        g_object_unref(mnt);
    }
}

void on_eject(GtkAction* act, gpointer user_data)
{
    PlaceItem* item = (PlaceItem*)user_data;
    fm_eject_volume(NULL, item->vol, TRUE);
}

void on_remove_bm(GtkAction* act, gpointer user_data)
{
    PlaceItem* item = (PlaceItem*)user_data;
    fm_bookmarks_remove(bookmarks, item->bm_item);
}

void on_rename_bm(GtkAction* act, gpointer user_data)
{
    PlaceItem* item = (PlaceItem*)user_data;
    char* new_name = fm_get_user_input(NULL, _("Rename Bookmark Item"),
                                        _("Enter a new name:"), item->bm_item->name);
    if(new_name)
    {
        if(strcmp(new_name, item->bm_item->name))
        {
            fm_bookmarks_rename(bookmarks, item->bm_item, new_name);
        }
        g_free(new_name);
    }
}

void on_empty_trash(GtkAction* act, gpointer user_data)
{
    fm_empty_trash();
}

gboolean on_dnd_dest_query_info(FmDndDest* dd, int x, int y,
			GdkDragAction* action, FmPlacesView* view)
{
    gboolean ret = TRUE;
    GtkTreeViewDropPosition pos;
	GtkTreePath* tp = NULL;
    GtkTreeViewColumn* col;
    FmFileInfo* dest = NULL;

    if(gtk_tree_view_get_dest_row_at_pos((GtkTreeView*)view, x, y, &tp, &pos))
    {
        /* FIXME: this is inefficient. we should record the index of separator instead. */
        if(pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE || pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
        {
            GtkTreeIter it;
            if(gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &it, tp))
            {
                PlaceItem* item;
                gtk_tree_model_get(model, &it, COL_INFO, &item, -1);
                if(item && item->fi->path)
                {
                    if(!fm_path_is_virtual(item->fi->path)
                       || fm_path_is_trash_root(item->fi->path))
                    {
                        dest = fm_file_info_ref(item->fi);
                    }
                }
                else
                    *action = 0;
            }
            else
                *action = 0;
        }
        else
        {
            GtkTreePath* sep = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &sep_it);
            if(gtk_tree_path_compare(sep, tp) < 0) /* tp is after sep */
            {
                *action = GDK_ACTION_LINK;
            }
            else
            {
                *action = 0;
                gtk_tree_path_free(tp);
                tp = NULL;
            }
            gtk_tree_path_free(sep);
        }
    }
    else
    {
        tp = gtk_tree_path_new_from_indices(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model), NULL)-1, -1);
        pos = GTK_TREE_VIEW_DROP_AFTER;
        *action = GDK_ACTION_LINK;
    }
    fm_dnd_dest_set_dest_file(view->dnd_dest, dest);
    if(dest)
    {
        ret = FALSE;
        fm_file_info_unref(dest);
    }
    gtk_tree_view_set_drag_dest_row((GtkTreeView*)view, tp, pos);

    if(view->dest_row)
        gtk_tree_path_free(view->dest_row);
    view->dest_row = tp;
    view->dest_pos = pos;

	return ret;
}

gboolean on_dnd_dest_files_dropped(FmDndDest* dd, GdkDragAction action,
                               int info_type, FmFileInfoList* files, FmPlacesView* view)
{
	FmPath* dest;
    GList* l;
    gboolean ret = FALSE;

	dest = fm_dnd_dest_get_dest_path(dd);
    /* g_debug("action= %d, %d files-dropped!, info_type: %d", action, fm_list_get_length(files), info_type); */

    if(!dest && action == GDK_ACTION_LINK) /* add bookmarks */
    {
        GtkTreePath* tp = view->dest_row;
        if(tp)
        {
            GtkTreePath* sep = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &sep_it);
            int idx = gtk_tree_path_get_indices(tp)[0] - gtk_tree_path_get_indices(sep)[0];
            gtk_tree_path_free(sep);
            if(view->dest_pos == GTK_TREE_VIEW_DROP_BEFORE)
                --idx;
            for( l=fm_list_peek_head_link(files); l; l=l->next, ++idx )
            {
                FmBookmarkItem* item;
                FmFileInfo* fi = FM_FILE_INFO(l->data);
                if(fm_file_info_is_dir(fi))
                    item = fm_bookmarks_insert( bookmarks, fi->path, fi->disp_name, idx);
                /* we don't need to add item to places view. Later the bookmarks will be reloaded. */
            }
        }
        ret = TRUE;
    }

    if(view->dest_row)
    {
        gtk_tree_path_free(view->dest_row);
        view->dest_row = NULL;
    }
    return ret;
}

gboolean update_trash(gpointer user_data)
{
    if(fm_config->use_trash)
    {
        GFile* gf = g_file_new_for_uri("trash:///");
        GFileInfo* inf = g_file_query_info(gf, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT, 0, NULL, NULL);
        g_object_unref(gf);
        if(inf)
        {
            FmIcon* icon;
            const char* icon_name;
            PlaceItem* item;
            GdkPixbuf* pix;
            guint32 n = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
            g_object_unref(inf);
            icon_name = n > 0 ? "user-trash-full" : "user-trash";
            icon = fm_icon_from_name(icon_name);
            gtk_tree_model_get(GTK_TREE_MODEL(model), &trash_it, COL_INFO, &item, -1);
            if(item->fi->icon)
                fm_icon_unref(item->fi->icon);
            item->fi->icon = icon;
            /* update the icon */
            pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
            gtk_list_store_set(model, &trash_it, COL_ICON, pix, -1);
            g_object_unref(pix);
        }
    }
    return FALSE;
}

void on_trash_changed(GFileMonitor *monitor, GFile *gf, GFile *other, GFileMonitorEvent evt, gpointer user_data)
{
    if(trash_idle)
        g_source_remove(trash_idle);
    trash_idle = g_idle_add((GSourceFunc)update_trash, NULL);
}

void update_icons()
{
    GtkTreeIter it;
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it);
    do{
        if(it.user_data != sep_it.user_data)
        {
            PlaceItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, COL_INFO, &item, -1);
            /* FIXME: get icon size from FmConfig */
            GdkPixbuf* pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
            gtk_list_store_set(model, &it, COL_ICON, pix, -1);
            g_object_unref(pix);
        }
    }while( gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it) );
}

void on_use_trash_changed(FmConfig* cfg, gpointer unused)
{
    if(cfg->use_trash && trash_it.user_data == NULL)
        create_trash();
    else
    {
        PlaceItem *item;
        gtk_tree_model_get(GTK_TREE_MODEL(model), &trash_it, COL_INFO, &item, -1);
        gtk_list_store_remove(GTK_LIST_STORE(model), &trash_it);
        trash_it.user_data = NULL;
        place_item_free(item);

        if(trash_monitor)
        {
            g_signal_handlers_disconnect_by_func(trash_monitor, on_trash_changed, NULL);
            g_object_unref(trash_monitor);
            trash_monitor = NULL;
        }
        if(trash_idle)
        {
            g_source_remove(trash_idle);
            trash_idle = 0;
        }
    }
}

void on_pane_icon_size_changed(FmConfig* cfg, gpointer unused)
{
    update_icons();
}

void on_file_info_job_finished(FmFileInfoJob* job, gpointer user_data)
{
    GList* l;
    GtkTreeIter it;
    PlaceItem* item;
    FmFileInfo* fi;

    /* g_debug("file info job finished"); */
    jobs = g_slist_remove(jobs, job);

    if(!gtk_tree_model_get_iter_first(model, &it))
        return;

    if(fm_list_is_empty(job->file_infos))
        return;

    /* optimize for one file case */
    if(fm_list_get_length(job->file_infos) == 1)
    {
        fi = FM_FILE_INFO(fm_list_peek_head(job->file_infos));
        do {
            item = NULL;
            gtk_tree_model_get(model, &it, COL_INFO, &item, -1);
            if( item && item->fi && item->fi->path && fm_path_equal(item->fi->path, fi->path) )
            {
                fm_file_info_unref(item->fi);
                item->fi = fm_file_info_ref(fi);
                break;
            }
        }while(gtk_tree_model_iter_next(model, &it));
    }
    else
    {
        do {
            item = NULL;
            gtk_tree_model_get(model, &it, COL_INFO, &item, -1);
            if( item && item->fi && item->fi->path )
            {
                for(l = fm_list_peek_head_link(job->file_infos); l; l = l->next )
                {
                    fi = FM_FILE_INFO(l->data);
                    if(fm_path_equal(item->fi->path, fi->path))
                    {
                        fm_file_info_unref(item->fi);
                        item->fi = fm_file_info_ref(fi);
                        /* remove the file from list to speed up further loading.
                      * This won't cause problem since nobody else if using the list. */
                        fm_list_delete_link(job->file_infos, l);
                        break;
                    }
                }
            }
        }while(gtk_tree_model_iter_next(model, &it));
    }
}
