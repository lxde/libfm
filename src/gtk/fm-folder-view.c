/*
 *      folder-view.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
 * SECTION:fm-folder-view
 * @short_description: A folder view widget.
 * @title: FmFolderView
 *
 * @include: libfm/fm-folder-view.h
 *
 * The #FmFolderView represents view of content of a folder with
 * support of drag & drop and other file/directory operations.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include "gtk-compat.h"

#include "fm-marshal.h"
#include "fm-config.h"
#include "fm-folder-view.h"
#include "fm-folder.h"
#include "fm-folder-model.h"
#include "fm-gtk-marshal.h"
#include "fm-cell-renderer-text.h"
#include "fm-cell-renderer-pixbuf.h"
#include "fm-gtk-utils.h"

#include "exo/exo-icon-view.h"
#include "exo/exo-tree-view.h"

#include "fm-dnd-auto-scroll.h"

enum{
    CLICKED,
    SEL_CHANGED,
    SORT_CHANGED,
    //CHDIR,
    N_SIGNALS
};

struct _FmFolderView
{
    GtkScrolledWindow parent;

    FmFolderViewMode mode;
    GtkSelectionMode sel_mode;
    GtkSortType sort_type;
    FmFolderModelViewCol sort_by;

    gboolean show_hidden;

    GtkWidget* view; /* either ExoIconView or ExoTreeView */
    FmFolderModel* model; /* FmFolderView doesn't use abstract GtkTreeModel! */
    FmCellRendererPixbuf* renderer_pixbuf; /* reference is bound to GtkWidget */
    guint icon_size_changed_handler;

    FmDndSrc* dnd_src; /* dnd source manager */
    FmDndDest* dnd_dest; /* dnd dest manager */

    /* wordarounds to fix new gtk+ bug introduced in gtk+ 2.20: #612802 */
    GtkTreeRowReference* activated_row_ref; /* for row-activated handler */
    guint row_activated_idle;

    FmFileInfoList* cached_selected_files;
    FmPathList* cached_selected_file_paths;
};

#define SINGLE_CLICK_TIMEOUT    600

static guint signals[N_SIGNALS];

static void fm_folder_view_dispose(GObject *object);
G_DEFINE_TYPE(FmFolderView, fm_folder_view, GTK_TYPE_SCROLLED_WINDOW);

static GList* fm_folder_view_get_selected_tree_paths(FmFolderView* fv);

static gboolean on_folder_view_focus_in(GtkWidget* widget, GdkEventFocus* evt);

static gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmFolderView* fv);
static void on_sel_changed(GObject* obj, FmFolderView* fv);
static void on_sort_col_changed(GtkTreeSortable* sortable, FmFolderView* fv);
FmJobErrorAction on_error(FmFolderView* fv, GError* err, FmJobErrorSeverity severity);

static void on_dnd_src_data_get(FmDndSrc* ds, FmFolderView* fv);

static void on_single_click_changed(FmConfig* cfg, FmFolderView* fv);
static void on_big_icon_size_changed(FmConfig* cfg, FmFolderView* fv);
static void on_small_icon_size_changed(FmConfig* cfg, FmFolderView* fv);
static void on_thumbnail_size_changed(FmConfig* cfg, FmFolderView* fv);

static void cancel_pending_row_activated(FmFolderView* fv);

//static void on_folder_reload(FmFolder* folder, FmFolderView* fv);
//static void on_folder_loaded(FmFolder* folder, FmFolderView* fv);
//static void on_folder_unmounted(FmFolder* folder, FmFolderView* fv);
//static void on_folder_removed(FmFolder* folder, FmFolderView* fv);

static void fm_folder_view_class_init(FmFolderViewClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass *widget_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_folder_view_dispose;
    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->focus_in_event = on_folder_view_focus_in;

    fm_folder_view_parent_class = (GtkScrolledWindowClass*)g_type_class_peek(GTK_TYPE_SCROLLED_WINDOW);

    /**
     * FmFolderView::clicked:
     * @view: the widget that emitted the signal
     * @type: (#FmFolderViewClickType) type of click
     * @file: (#FmFileInfo *) file on which cursor is
     *
     * The #FmFolderView::clicked signal is emitted when user clicked
     * somewhere in the folder area. If click was on free folder area
     * then @file is %NULL.
     *
     * Since: 0.1.0
     */
    signals[CLICKED]=
        g_signal_new("clicked",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewClass, clicked),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__UINT_POINTER,
                     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);

    /**
     * FmFolderView::sel-changed:
     * @view: the widget that emitted the signal
     * @n_sel: number of files currently selected in the folder
     *
     * The #FmFolderView::sel-changed signal is emitted when
     * selection of the view got changed.
     *
     * Before 1.0.0 parameter was list of currently selected files.
     *
     * Since: 0.1.0
     */
    signals[SEL_CHANGED]=
        g_signal_new("sel-changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewClass, sel_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * FmFolderView::sort-changed:
     * @view: the widget that emitted the signal
     *
     * The #FmFolderView::sort-changed signal is emitted when sorting
     * of the view got changed.
     *
     * Since: 0.1.10
     */
    signals[SORT_CHANGED]=
        g_signal_new("sort-changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewClass, sort_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /* FIXME: add "chdir" so main-win can connect to it and sync side-pane */
}

static gboolean on_folder_view_focus_in(GtkWidget* widget, GdkEventFocus* evt)
{
    FmFolderView* fv = FM_FOLDER_VIEW(widget);
    if( fv->view )
    {
        gtk_widget_grab_focus(fv->view);
        return TRUE;
    }
    return FALSE;
}

static void on_single_click_changed(FmConfig* cfg, FmFolderView* fv)
{
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        exo_tree_view_set_single_click(EXO_TREE_VIEW(fv->view), cfg->single_click);
        break;
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        exo_icon_view_set_single_click(EXO_ICON_VIEW(fv->view), cfg->single_click);
        break;
    }
}

static void item_clicked( FmFolderView* fv, GtkTreePath* path, FmFolderViewClickType type )
{
    GtkTreeIter it;
    if(path)
    {
        if(gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, path))
        {
            FmFileInfo* fi;
            gtk_tree_model_get(GTK_TREE_MODEL(fv->model), &it, COL_FILE_INFO, &fi, -1);
            g_signal_emit(fv, signals[CLICKED], 0, type, fi);
        }
    }
    else
        g_signal_emit(fv, signals[CLICKED], 0, type, NULL);
}

static void on_icon_view_item_activated(ExoIconView* iv, GtkTreePath* path, FmFolderView* fv)
{
    item_clicked(fv, path, FM_FV_ACTIVATED);
}

static gboolean on_idle_tree_view_row_activated(gpointer user_data)
{
    FmFolderView* fv = (FmFolderView*)user_data;
    GtkTreePath* path;
    if(gtk_tree_row_reference_valid(fv->activated_row_ref))
    {
        path = gtk_tree_row_reference_get_path(fv->activated_row_ref);
        item_clicked(fv, path, FM_FV_ACTIVATED);
        gtk_tree_path_free(path);
    }
    gtk_tree_row_reference_free(fv->activated_row_ref);
    fv->activated_row_ref = NULL;
    fv->row_activated_idle = 0;
    return FALSE;
}

static void on_tree_view_row_activated(GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, FmFolderView* fv)
{
    /* Due to GTK+ and libexo bugs, here a workaround is needed.
     * https://bugzilla.gnome.org/show_bug.cgi?id=612802
     * http://bugzilla.xfce.org/show_bug.cgi?id=6230
     * Gtk+ 2.20+ changed its behavior, which is really bad.
     * row-activated signal is now issued in the second button-press events
     * rather than double click events. The content of the view and model
     * gets changed in row-activated signal handler before button-press-event
     * handling is finished, and this breaks button-press handler of ExoTreeView
     * and causing some selection-related bugs since select function cannot be reset.*/

    cancel_pending_row_activated(fv);
    if(fv->model)
    {
        fv->activated_row_ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(fv->model), path);
        g_idle_add(on_idle_tree_view_row_activated, fv);
    }
}

static void fm_folder_view_init(FmFolderView *self)
{
    gtk_scrolled_window_set_hadjustment((GtkScrolledWindow*)self, NULL);
    gtk_scrolled_window_set_vadjustment((GtkScrolledWindow*)self, NULL);
    gtk_scrolled_window_set_policy((GtkScrolledWindow*)self, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* config change notifications */
    g_signal_connect(fm_config, "changed::single_click", G_CALLBACK(on_single_click_changed), self);

    /* dnd support */
    self->dnd_src = fm_dnd_src_new(NULL);
    g_signal_connect(self->dnd_src, "data-get", G_CALLBACK(on_dnd_src_data_get), self);

    self->dnd_dest = fm_dnd_dest_new(NULL);

    self->mode = -1;
    self->sort_type = GTK_SORT_ASCENDING;
    self->sort_by = COL_FILE_NAME;
}

/**
 * fm_folder_view_new
 * @mode: initial mode of view
 *
 * Creates new folder view.
 *
 * Returns: a new #FmFolderView widget.
 *
 * Since: 0.1.0
 */
FmFolderView* fm_folder_view_new(FmFolderViewMode mode)
{
    FmFolderView* fv = (FmFolderView*)g_object_new(FM_FOLDER_VIEW_TYPE, NULL);
    fm_folder_view_set_mode(fv, mode);
    return fv;
}

static void unset_model(FmFolderView* fv)
{
    if(fv->model)
    {
        FmFolderModel* model = fv->model;
        g_signal_handlers_disconnect_by_func(model, on_sort_col_changed, fv);
        /* g_debug("unset_model: %p, n_ref = %d", model, G_OBJECT(model)->ref_count); */
        g_object_unref(model);
        fv->model = NULL;
    }
}

static void unset_view(FmFolderView* fv);
static void fm_folder_view_dispose(GObject *object)
{
    FmFolderView *self;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FOLDER_VIEW(object));
    self = (FmFolderView*)object;
    /* g_debug("fm_folder_view_dispose: %p", self); */

    unset_model(self);

    if(G_LIKELY(self->view))
        unset_view(self);

    if(self->cached_selected_files)
    {
        fm_file_info_list_unref(self->cached_selected_files);
        self->cached_selected_files = NULL;
    }

    if(self->cached_selected_file_paths)
    {
        fm_path_list_unref(self->cached_selected_file_paths);
        self->cached_selected_file_paths = NULL;
    }

    if(self->dnd_src)
    {
        g_signal_handlers_disconnect_by_func(self->dnd_src, on_dnd_src_data_get, self);
        g_object_unref(self->dnd_src);
        self->dnd_src = NULL;
    }
    if(self->dnd_dest)
    {
        g_object_unref(self->dnd_dest);
        self->dnd_dest = NULL;
    }

    g_signal_handlers_disconnect_by_func(fm_config, on_single_click_changed, object);
    cancel_pending_row_activated(self); /* this frees activated_row_ref */

    if(self->icon_size_changed_handler)
    {
        g_signal_handler_disconnect(fm_config, self->icon_size_changed_handler);
        self->icon_size_changed_handler = 0;
    }
    (* G_OBJECT_CLASS(fm_folder_view_parent_class)->dispose)(object);
}

static void set_icon_size(FmFolderView* fv, guint icon_size)
{
    FmCellRendererPixbuf* render = fv->renderer_pixbuf;
    fm_cell_renderer_pixbuf_set_fixed_size(render, icon_size, icon_size);

    if(!fv->model)
        return;

    fm_folder_model_set_icon_size(fv->model, icon_size);

    if( fv->mode != FM_FV_LIST_VIEW ) /* this is an ExoIconView */
    {
        /* FIXME: reset ExoIconView item sizes */
    }
}

static void on_big_icon_size_changed(FmConfig* cfg, FmFolderView* fv)
{
    set_icon_size(fv, cfg->big_icon_size);
}

static void on_small_icon_size_changed(FmConfig* cfg, FmFolderView* fv)
{
    set_icon_size(fv, cfg->small_icon_size);
}

static void on_thumbnail_size_changed(FmConfig* cfg, FmFolderView* fv)
{
    /* FIXME: thumbnail and icons should have different sizes */
    /* maybe a separate API: fm_folder_model_set_thumbnail_size() */
    set_icon_size(fv, cfg->thumbnail_size);
}

static void on_drag_data_received(GtkWidget *dest_widget,
                                    GdkDragContext *drag_context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *sel_data,
                                    guint info,
                                    guint time,
                                    FmFolderView* fv )
{
    fm_dnd_dest_drag_data_received(fv->dnd_dest, drag_context, x, y, sel_data, info, time);
}

static gboolean on_drag_drop(GtkWidget *dest_widget,
                               GdkDragContext *drag_context,
                               gint x,
                               gint y,
                               guint time,
                               FmFolderView* fv)
{
    gboolean ret = FALSE;
    GdkAtom target = gtk_drag_dest_find_target(dest_widget, drag_context, NULL);
    if(target != GDK_NONE)
        ret = fm_dnd_dest_drag_drop(fv->dnd_dest, drag_context, target, x, y, time);
    return ret;
}

static GtkTreePath* get_drop_path(FmFolderView* fv, gint x, gint y)
{
    GtkTreePath* tp = NULL;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        {
            GtkTreeViewColumn* col;
            gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(fv->view), x, y, &x, &y);
            if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(fv->view), x, y, &tp, &col, NULL, NULL))
            {
                if(gtk_tree_view_column_get_sort_column_id(col)!=COL_FILE_NAME)
                {
                    gtk_tree_path_free(tp);
                    tp = NULL;
                }
            }
            if(tp)
                gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(fv->view), tp,
                                                GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
            break;
        }
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        {
            tp = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(fv->view), x, y);
            exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(fv->view), tp, EXO_ICON_VIEW_DROP_INTO);
            break;
        }
    }

    return tp;
}

static gboolean on_drag_motion(GtkWidget *dest_widget,
                                 GdkDragContext *drag_context,
                                 gint x,
                                 gint y,
                                 guint time,
                                 FmFolderView* fv)
{
    gboolean ret;
    GdkDragAction action = 0;
    GdkAtom target = gtk_drag_dest_find_target(dest_widget, drag_context, NULL);

    if(target == GDK_NONE)
        return FALSE;

    ret = FALSE;
    /* files are being dragged */
    if(fm_dnd_dest_is_target_supported(fv->dnd_dest, target))
    {
        GtkTreePath* tp = get_drop_path(fv, x, y);
        if(tp)
        {
            GtkTreeIter it;
            if(gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, tp))
            {
                FmFileInfo* fi;
                gtk_tree_model_get(GTK_TREE_MODEL(fv->model), &it, COL_FILE_INFO, &fi, -1);
                fm_dnd_dest_set_dest_file(fv->dnd_dest, fi);
            }
            gtk_tree_path_free(tp);
        }
        else
        {
            FmFolderModel* model = fv->model;
            if (model)
            {
                FmFolder* folder = fm_folder_model_get_folder(model);
                fm_dnd_dest_set_dest_file(fv->dnd_dest, fm_folder_get_info(folder));
            }
            else
                fm_dnd_dest_set_dest_file(fv->dnd_dest, NULL);
        }
        action = fm_dnd_dest_get_default_action(fv->dnd_dest, drag_context, target);
        ret = action != 0;
    }
    gdk_drag_status(drag_context, action, time);

    return ret;
}

static void on_drag_leave(GtkWidget *dest_widget,
                                GdkDragContext *drag_context,
                                guint time,
                                FmFolderView* fv)
{
    fm_dnd_dest_drag_leave(fv->dnd_dest, drag_context, time);
}

static inline void create_icon_view(FmFolderView* fv, GList* sels)
{
    GList *l;
    GtkCellRenderer* render;
    FmFolderModel* model = fv->model;
    int icon_size = 0;

    fv->view = exo_icon_view_new();

    fv->renderer_pixbuf = fm_cell_renderer_pixbuf_new();
    render = (GtkCellRenderer*)fv->renderer_pixbuf;

    g_object_set((GObject*)render, "follow-state", TRUE, NULL );
    /* FIXME: is GtkCellLayout compatible with GtkContainer=>GtkTreeView=>ExoIconView? */
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(fv->view), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render, "pixbuf", COL_FILE_ICON );
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render, "info", COL_FILE_INFO );

    if(fv->mode == FM_FV_COMPACT_VIEW) /* compact view */
    {
        fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_small_icon_size_changed), fv);
        icon_size = fm_config->small_icon_size;
        fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
        if(model)
            fm_folder_model_set_icon_size(model, icon_size);

        render = fm_cell_renderer_text_new();
        g_object_set((GObject*)render,
                     "xalign", 1.0, /* FIXME: why this needs to be 1.0? */
                     "yalign", 0.5,
                     NULL );
        exo_icon_view_set_layout_mode( (ExoIconView*)fv->view, EXO_ICON_VIEW_LAYOUT_COLS );
        exo_icon_view_set_orientation( (ExoIconView*)fv->view, GTK_ORIENTATION_HORIZONTAL );
    }
    else /* big icon view or thumbnail view */
    {
        if(fv->mode == FM_FV_ICON_VIEW)
        {
            fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::big_icon_size", G_CALLBACK(on_big_icon_size_changed), fv);
            icon_size = fm_config->big_icon_size;
            fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
            if(model)
                fm_folder_model_set_icon_size(model, icon_size);

            render = fm_cell_renderer_text_new();
            /* FIXME: set the sizes of cells according to iconsize */
            g_object_set((GObject*)render,
                         "wrap-mode", PANGO_WRAP_WORD_CHAR,
                         "wrap-width", 90,
                         "alignment", PANGO_ALIGN_CENTER,
                         "xalign", 0.5,
                         "yalign", 0.0,
                         NULL );
            exo_icon_view_set_column_spacing( (ExoIconView*)fv->view, 4 );
            exo_icon_view_set_item_width ( (ExoIconView*)fv->view, 110 );
        }
        else
        {
            fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::thumbnail_size", G_CALLBACK(on_thumbnail_size_changed), fv);
            icon_size = fm_config->thumbnail_size;
            fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
            if(model)
                fm_folder_model_set_icon_size(model, icon_size);

            render = fm_cell_renderer_text_new();
            /* FIXME: set the sizes of cells according to iconsize */
            g_object_set((GObject*)render,
                         "wrap-mode", PANGO_WRAP_WORD_CHAR,
                         "wrap-width", 180,
                         "alignment", PANGO_ALIGN_CENTER,
                         "xalign", 0.5,
                         "yalign", 0.0,
                         NULL );
            exo_icon_view_set_column_spacing( (ExoIconView*)fv->view, 8 );
            exo_icon_view_set_item_width ( (ExoIconView*)fv->view, 200 );
        }
    }
    /* FIXME: is GtkCellLayout compatible with GtkContainer=>GtkTreeView=>ExoIconView? */
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(fv->view), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render,
                                "text", COL_FILE_NAME );
    exo_icon_view_set_item_width((ExoIconView*)fv->view, 96);
    exo_icon_view_set_search_column((ExoIconView*)fv->view, COL_FILE_NAME);
    g_signal_connect(fv->view, "item-activated", G_CALLBACK(on_icon_view_item_activated), fv);
    g_signal_connect(fv->view, "selection-changed", G_CALLBACK(on_sel_changed), fv);
    exo_icon_view_set_model((ExoIconView*)fv->view, (GtkTreeModel*)fv->model);
    exo_icon_view_set_selection_mode((ExoIconView*)fv->view, fv->sel_mode);
    exo_icon_view_set_single_click((ExoIconView*)fv->view, fm_config->single_click);
    exo_icon_view_set_single_click_timeout((ExoIconView*)fv->view, SINGLE_CLICK_TIMEOUT);

    for(l = sels;l;l=l->next)
        exo_icon_view_select_path((ExoIconView*)fv->view, l->data);
}

static inline void create_list_view(FmFolderView* fv, GList* sels)
{
    GtkTreeViewColumn* col;
    GtkTreeSelection* ts;
    GList *l;
    GtkCellRenderer* render;
    FmFolderModel* model = fv->model;
    int icon_size = 0;
    fv->view = exo_tree_view_new();

    fv->renderer_pixbuf = fm_cell_renderer_pixbuf_new();
    render = (GtkCellRenderer*)fv->renderer_pixbuf;
    fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_small_icon_size_changed), fv);
    icon_size = fm_config->small_icon_size;
    fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
    if(model)
        fm_folder_model_set_icon_size(model, icon_size);

    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(fv->view), TRUE);
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Name"));
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_set_attributes(col, render,
                                        "pixbuf", COL_FILE_ICON,
                                        "info", COL_FILE_INFO, NULL);
    render = gtk_cell_renderer_text_new();
    g_object_set(render, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_set_attributes(col, render, "text", COL_FILE_NAME, NULL);
    gtk_tree_view_column_set_sort_column_id(col, COL_FILE_NAME);
    gtk_tree_view_column_set_expand(col, TRUE);
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(col, 200);
    gtk_tree_view_append_column(GTK_TREE_VIEW(fv->view), col);
    /* only this column is activable */
    exo_tree_view_set_activable_column((ExoTreeView*)fv->view, col);

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(_("Description"), render, "text", COL_FILE_DESC, NULL);
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_sort_column_id(col, COL_FILE_DESC);
    gtk_tree_view_append_column(GTK_TREE_VIEW(fv->view), col);

    render = gtk_cell_renderer_text_new();
    g_object_set(render, "xalign", 1.0, NULL);
    col = gtk_tree_view_column_new_with_attributes(_("Size"), render, "text", COL_FILE_SIZE, NULL);
    gtk_tree_view_column_set_sort_column_id(col, COL_FILE_SIZE);
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(fv->view), col);

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(_("Modified"), render, "text", COL_FILE_MTIME, NULL);
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_sort_column_id(col, COL_FILE_MTIME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(fv->view), col);

    gtk_tree_view_set_search_column(GTK_TREE_VIEW(fv->view), COL_FILE_NAME);

    gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(fv->view), TRUE);
    exo_tree_view_set_single_click((ExoTreeView*)fv->view, fm_config->single_click);
    exo_tree_view_set_single_click_timeout((ExoTreeView*)fv->view, SINGLE_CLICK_TIMEOUT);

    ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
    g_signal_connect(fv->view, "row-activated", G_CALLBACK(on_tree_view_row_activated), fv);
    g_signal_connect(ts, "changed", G_CALLBACK(on_sel_changed), fv);
    /*cancel_pending_row_activated(fv);*/ /* FIXME: is this needed? */
    gtk_tree_view_set_model(GTK_TREE_VIEW(fv->view), GTK_TREE_MODEL(fv->model));
    gtk_tree_selection_set_mode(ts, fv->sel_mode);
    for(l = sels;l;l=l->next)
        gtk_tree_selection_select_path(ts, (GtkTreePath*)l->data);
}

static void unset_view(FmFolderView* fv)
{
    /* these signals connected by view creators */
    g_signal_handlers_disconnect_by_func(fv->view, on_tree_view_row_activated, fv);
    if(fv->mode == FM_FV_LIST_VIEW)
    {
        GtkTreeSelection* ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
        g_signal_handlers_disconnect_by_func(ts, on_sel_changed, fv);
    }
    else
        g_signal_handlers_disconnect_by_func(fv->view, on_sel_changed, fv);
    /* these signals connected by fm_folder_view_set_mode() */
    g_signal_handlers_disconnect_by_func(fv->view, on_drag_motion, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_drag_leave, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_drag_drop, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_drag_data_received, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_btn_pressed, fv);

    fm_dnd_unset_dest_auto_scroll(fv->view);
    gtk_widget_destroy(GTK_WIDGET(fv->view));
    fv->view = NULL;
}

/**
 * fm_folder_view_set_mode
 * @fv: a widget to apply
 * @mode: new mode of view
 *
 * Changes current view mode for folder in @fv.
 *
 * Since: 0.1.0
 */
void fm_folder_view_set_mode(FmFolderView* fv, FmFolderViewMode mode)
{
    if( mode != fv->mode )
    {
        GList *sels;
        gboolean has_focus;

        if( G_LIKELY(fv->view) )
        {
            has_focus = GTK_WIDGET_HAS_FOCUS(fv->view);
            /* preserve old selections */
            sels = fm_folder_view_get_selected_tree_paths(fv);

            unset_view(fv); /* it will destroy the fv->view widget */

            /* FIXME: compact view and icon view actually use the same
             * type of widget, ExoIconView. So it may be better to
             * reuse the widget when available. */
        }
        else
        {
            sels = NULL;
            has_focus = FALSE;
        }

        if(fv->icon_size_changed_handler)
        {
            g_signal_handler_disconnect(fm_config, fv->icon_size_changed_handler);
            fv->icon_size_changed_handler = 0;
        }

        fv->mode = mode;
        switch(mode)
        {
        case FM_FV_COMPACT_VIEW:
        case FM_FV_ICON_VIEW:
        case FM_FV_THUMBNAIL_VIEW:
            create_icon_view(fv, sels);
            break;
        case FM_FV_LIST_VIEW: /* detailed list view */
            create_list_view(fv, sels);
        }
        g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
        g_list_free(sels);

        /* FIXME: maybe calling set_icon_size here is a good idea */

        gtk_drag_source_set(fv->view, GDK_BUTTON1_MASK,
            fm_default_dnd_src_targets, N_FM_DND_SRC_DEFAULT_TARGETS,
            GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK|GDK_ACTION_ASK);
        fm_dnd_src_set_widget(fv->dnd_src, fv->view);

        gtk_drag_dest_set(fv->view, 0,
            fm_default_dnd_dest_targets, N_FM_DND_DEST_DEFAULT_TARGETS,
            GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK|GDK_ACTION_ASK);
        fm_dnd_dest_set_widget(fv->dnd_dest, fv->view);
        g_signal_connect_after(fv->view, "drag-motion", G_CALLBACK(on_drag_motion), fv);
        g_signal_connect(fv->view, "drag-leave", G_CALLBACK(on_drag_leave), fv);
        g_signal_connect(fv->view, "drag-drop", G_CALLBACK(on_drag_drop), fv);
        g_signal_connect(fv->view, "drag-data-received", G_CALLBACK(on_drag_data_received), fv);
        g_signal_connect(fv->view, "button-press-event", G_CALLBACK(on_btn_pressed), fv);

        fm_dnd_set_dest_auto_scroll(fv->view, gtk_scrolled_window_get_hadjustment((GtkScrolledWindow*)fv), gtk_scrolled_window_get_vadjustment((GtkScrolledWindow*)fv));

        gtk_widget_show(fv->view);
        gtk_container_add(GTK_CONTAINER(fv), fv->view);

        if(has_focus) /* restore the focus if needed. */
            gtk_widget_grab_focus(fv->view);
    }
    else
    {
        /* g_debug("same mode"); */
    }
}

/**
 * fm_folder_view_get_mode
 * @fv: a widget to inspect
 *
 * Retrieves current view mode for folder in @fv.
 *
 * Returns: current mode of view.
 *
 * Since: 0.1.0
 */
FmFolderViewMode fm_folder_view_get_mode(FmFolderView* fv)
{
    return fv->mode;
}

/**
 * fm_folder_view_set_selection_mode
 * @fv: a widget to apply
 * @mode: new mode of selection in @fv.
 *
 * Changes selection mode in @fv.
 *
 * Since: 0.1.0
 */
void fm_folder_view_set_selection_mode(FmFolderView* fv, GtkSelectionMode mode)
{
    if(fv->sel_mode != mode)
    {
        fv->sel_mode = mode;
        switch(fv->mode)
        {
        case FM_FV_LIST_VIEW:
        {
            GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
            gtk_tree_selection_set_mode(sel, mode);
            break;
        }
        case FM_FV_ICON_VIEW:
        case FM_FV_COMPACT_VIEW:
        case FM_FV_THUMBNAIL_VIEW:
            exo_icon_view_set_selection_mode(EXO_ICON_VIEW(fv->view), mode);
            break;
        }
    }
}

/**
 * fm_folder_view_get_selection_mode
 * @fv: a widget to inspect
 *
 * Retrieves current selection mode in @fv.
 *
 * Returns: current selection mode.
 *
 * Since: 0.1.0
 */
GtkSelectionMode fm_folder_view_get_selection_mode(FmFolderView* fv)
{
    return fv->sel_mode;
}

/**
 * fm_folder_view_sort
 * @fv: a widget to apply
 * @type: new mode of sorting (ascending or descending)
 * @by: criteria of sorting
 *
 * Changes sorting in the view.
 *
 * Since: 0.1.0
 */
void fm_folder_view_sort(FmFolderView* fv, GtkSortType type, FmFolderModelViewCol by)
{
    if(type == GTK_SORT_ASCENDING || type == GTK_SORT_DESCENDING)
        fv->sort_type = type;
    if(FM_FOLDER_MODEL_COL_IS_VALID(by))
        fv->sort_by = by;
    if(fv->model)
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fv->model),
                                             fv->sort_by, fv->sort_type);
}

/**
 * fm_folder_view_get_sort_type
 * @fv: a widget to inspect
 *
 * Retrieves current sorting type in @fv.
 *
 * Returns: mode of sorting (ascending or descending)
 *
 * Since: 0.1.0
 */
GtkSortType fm_folder_view_get_sort_type(FmFolderView* fv)
{
    return fv->sort_type;
}

/**
 * fm_folder_view_get_sort_by
 * @fv: a widget to inspect
 *
 * Retrieves current criteria of sorting in @fv (e.g. by name).
 *
 * Returns: criteria of sorting.
 *
 * Since: 0.1.0
 */
int fm_folder_view_get_sort_by(FmFolderView* fv)
{
    return fv->sort_by;
}

/**
 * fm_folder_view_set_show_hidden
 * @fv: a widget to apply
 * @show: new setting
 *
 * Changes whether hidden files in folder shown in @fv should be visible
 * or not.
 *
 * See also: fm_folder_view_get_show_hidden().
 *
 * Since: 0.1.0
 */
void fm_folder_view_set_show_hidden(FmFolderView* fv, gboolean show)
{
    if(show != fv->show_hidden )
    {
        fv->show_hidden = show;
        if(G_LIKELY(fv->model))
            fm_folder_model_set_show_hidden(fv->model, show);
    }
}

/**
 * fm_folder_view_get_show_hidden
 * @fv: a widget to inspect
 *
 * Retrieves setting whether hidden files in folder shown in @fv should
 * be visible or not.
 *
 * Returns: %TRUE if hidden files are visible.
 *
 * See also: fm_folder_view_set_show_hidden().
 *
 * Since: 0.1.0
 */
gboolean fm_folder_view_get_show_hidden(FmFolderView* fv)
{
    return fv->show_hidden;
}

/* returned list should be freed with g_list_free_full(list, gtk_tree_path_free) */
static GList* fm_folder_view_get_selected_tree_paths(FmFolderView* fv)
{
    GList *sels = NULL;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
    {
        GtkTreeSelection* sel;
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
        sels = gtk_tree_selection_get_selected_rows(sel, NULL);
        break;
    }
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        sels = exo_icon_view_get_selected_items(EXO_ICON_VIEW(fv->view));
        break;
    }
    return sels;
}

/**
 * fm_folder_view_dup_selected_files
 * @fv: a FmFolderView object
 *
 * Retrieves a list of
 * the currently selected files. The list should be freed after usage with
 * fm_file_info_list_unref(). If there are no files selected then return
 * value is %NULL.
 *
 * Before 1.0.0 this API had name fm_folder_view_get_selected_files.
 *
 * Return value: (transfer full): list of selected file infos.
 *
 * Since: 0.1.0
 */
static inline FmFileInfoList* fm_folder_view_get_selected_files(FmFolderView* fv)
{
    /* don't generate the data again if we have it cached. */
    if(!fv->cached_selected_files)
    {
        GList* sels = fm_folder_view_get_selected_tree_paths(fv);
        GList *l, *next;
        if(sels)
        {
            fv->cached_selected_files = fm_file_info_list_new();
            for(l = sels;l;l=next)
            {
                FmFileInfo* fi;
                GtkTreeIter it;
                GtkTreePath* tp = (GtkTreePath*)l->data;
                gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, l->data);
                gtk_tree_model_get(GTK_TREE_MODEL(fv->model), &it, COL_FILE_INFO, &fi, -1);
                gtk_tree_path_free(tp);
                next = l->next;
                l->data = fm_file_info_ref( fi );
                l->prev = l->next = NULL;
                fm_file_info_list_push_tail_link(fv->cached_selected_files, l);
            }
        }
    }
    return fv->cached_selected_files;
}

FmFileInfoList* fm_folder_view_dup_selected_files(FmFolderView* fv)
{
    return fm_file_info_list_ref(fm_folder_view_get_selected_files(fv));
}

/**
 * fm_folder_view_dup_selected_file_paths
 * @fv: a FmFolderView object
 *
 * Retrieves a list of
 * the currently selected files. The list should be freed after usage with
 * fm_path_list_unref(). If there are no files selected then return value
 * is %NULL.
 *
 * Before 1.0.0 this API had name fm_folder_view_get_selected_file_paths.
 *
 * Return value: (transfer full): list of selected file paths.
 *
 * Since: 0.1.0
 */
FmPathList* fm_folder_view_dup_selected_file_paths(FmFolderView* fv)
{
    if(!fv->cached_selected_file_paths)
    {
        FmFileInfoList* files = fm_folder_view_get_selected_files(fv);
        if(files)
            fv->cached_selected_file_paths = fm_path_list_new_from_file_info_list(files);
        else
            fv->cached_selected_file_paths = NULL;
    }
    return fm_path_list_ref(fv->cached_selected_file_paths);
}

static void on_sel_changed(GObject* obj, FmFolderView* fv)
{
    /* FIXME: this is inefficient, but currently there is no better way */

    /* clear cached selected files */
    if(fv->cached_selected_files)
    {
        fm_file_info_list_unref(fv->cached_selected_files);
        fv->cached_selected_files = NULL;
    }
    if(fv->cached_selected_file_paths)
    {
        fm_path_list_unref(fv->cached_selected_file_paths);
        fv->cached_selected_file_paths = NULL;
    }

    /* if someone is connected to our "sel-changed" signal. */
    if(g_signal_has_handler_pending(fv, signals[SEL_CHANGED], 0, TRUE))
    {
        /* get currently selected files, and cached them inside fm_folder_view_dup_selected_files(). */
        gint files = 0;

        switch(fv->mode)
        {
        case FM_FV_LIST_VIEW:
        {
            GtkTreeSelection* sel;
            sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
            files = gtk_tree_selection_count_selected_rows(sel);
            break;
        }
        case FM_FV_ICON_VIEW:
        case FM_FV_COMPACT_VIEW:
        case FM_FV_THUMBNAIL_VIEW:
            files = exo_icon_view_count_selected_items(EXO_ICON_VIEW(fv->view));
            break;
        }

        /* emit a selection changed notification to the world. */
        g_signal_emit(fv, signals[SEL_CHANGED], 0, files);
    }
}

static void on_sort_col_changed(GtkTreeSortable* sortable, FmFolderView* fv)
{
    int col;
    GtkSortType order;
    if(gtk_tree_sortable_get_sort_column_id(sortable, &col, &order))
    {
        fv->sort_by = col;
        fv->sort_type = order;
        g_signal_emit(fv, signals[SORT_CHANGED], 0);
    }
}

static gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmFolderView* fv)
{
    GList* sels = NULL;
    FmFolderViewClickType type = 0;
    GtkTreePath* tp;

    if(!fv->model)
        return FALSE;

    /* FIXME: handle single click activation */
    if( evt->type == GDK_BUTTON_PRESS )
    {
        /* special handling for ExoIconView */
        if(evt->button != 1)
        {
            if(fv->mode==FM_FV_ICON_VIEW || fv->mode==FM_FV_COMPACT_VIEW || fv->mode==FM_FV_THUMBNAIL_VIEW)
            {
                /* select the item on right click for ExoIconView */
                if(exo_icon_view_get_item_at_pos(EXO_ICON_VIEW(view), evt->x, evt->y, &tp, NULL))
                {
                    /* if the hit item is not currently selected */
                    if(!exo_icon_view_path_is_selected(EXO_ICON_VIEW(view), tp))
                    {
                        sels = exo_icon_view_get_selected_items(EXO_ICON_VIEW(view));
                        if( sels ) /* if there are selected items */
                        {
                            exo_icon_view_unselect_all(EXO_ICON_VIEW(view)); /* unselect all items */
                            g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
                            g_list_free(sels);
                        }
                        exo_icon_view_select_path(EXO_ICON_VIEW(view), tp);
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(view), tp, NULL, FALSE);
                    }
                    gtk_tree_path_free(tp);
                }
            }
            else if( fv->mode == FM_FV_LIST_VIEW
                     && evt->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(view)))
            {
                /* special handling for ExoTreeView */
                /* Fix #2986834: MAJOR PROBLEM: Deletes Wrong File Frequently. */
                GtkTreeViewColumn* col;
                if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view), evt->x, evt->y, &tp, &col, NULL, NULL))
                {
                    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
                    if(!gtk_tree_selection_path_is_selected(tree_sel, tp))
                    {
                        gtk_tree_selection_unselect_all(tree_sel);
                        if(col == exo_tree_view_get_activable_column(EXO_TREE_VIEW(view)))
                        {
                            gtk_tree_selection_select_path(tree_sel, tp);
                            gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), tp, NULL, FALSE);
                        }
                    }
                    gtk_tree_path_free(tp);
                }
            }
        }

        if(evt->button == 2) /* middle click */
            type = FM_FV_MIDDLE_CLICK;
        else if(evt->button == 3) /* right click */
            type = FM_FV_CONTEXT_MENU;
    }

    if( type != FM_FV_CLICK_NONE )
    {
        sels = fm_folder_view_get_selected_tree_paths(fv);
        if( sels || type == FM_FV_CONTEXT_MENU )
        {
            item_clicked(fv, sels ? sels->data : NULL, type);
            if(sels)
            {
                g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
                g_list_free(sels);
            }
        }
    }
    return FALSE;
}

/* TODO: select files by custom func, not yet implemented */
void fm_folder_view_select_custom(FmFolderView* fv, GFunc filter, gpointer user_data)
{
}

/**
 * fm_folder_view_select_all
 * @fv: a widget to apply
 *
 * Selects all files in folder.
 *
 * Since: 0.1.0
 */
void fm_folder_view_select_all(FmFolderView* fv)
{
    GtkTreeSelection * tree_sel;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
        gtk_tree_selection_select_all(tree_sel);
        break;
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        exo_icon_view_select_all(EXO_ICON_VIEW(fv->view));
        break;
    }
}

static void on_dnd_src_data_get(FmDndSrc* ds, FmFolderView* fv)
{
    FmFileInfoList* files = fm_folder_view_dup_selected_files(fv);
    if(files)
    {
        fm_dnd_src_set_files(ds, files);
        fm_file_info_list_unref(files);
    }
}

/**
 * fm_folder_view_select_invert
 * @fv: a widget to apply
 *
 * Selects all unselected files in @fv but unselects all selected.
 *
 * Since: 0.1.0
 */
void fm_folder_view_select_invert(FmFolderView* fv)
{
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        {
            GtkTreeSelection *tree_sel;
            GtkTreeIter it;
            if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(fv->model), &it))
                return;
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
            do
            {
                if(gtk_tree_selection_iter_is_selected(tree_sel, &it))
                    gtk_tree_selection_unselect_iter(tree_sel, &it);
                else
                    gtk_tree_selection_select_iter(tree_sel, &it);
            }while( gtk_tree_model_iter_next(GTK_TREE_MODEL(fv->model), &it ));
            break;
        }
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        {
            GtkTreePath* path;
            int i, n;
            n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(fv->model), NULL);
            if(n == 0)
                return;
            path = gtk_tree_path_new_first();
            for( i=0; i<n; ++i, gtk_tree_path_next(path) )
            {
                if ( exo_icon_view_path_is_selected(EXO_ICON_VIEW(fv->view), path))
                    exo_icon_view_unselect_path(EXO_ICON_VIEW(fv->view), path);
                else
                    exo_icon_view_select_path(EXO_ICON_VIEW(fv->view), path);
            }
            gtk_tree_path_free(path);
            break;
        }
    }
}

/**
 * fm_folder_view_select_file_path
 * @fv: a widget to apply
 * @path: a file path to select
 *
 * Selects a file in the folder.
 *
 * Since: 0.1.0
 */
void fm_folder_view_select_file_path(FmFolderView* fv, FmPath* path)
{
    FmPath* cwd = fm_folder_view_get_cwd(fv);
    if(cwd && fm_path_equal(fm_path_get_parent(path), cwd))
    {
        FmFolderModel* model = fv->model;
        GtkTreeIter it;
        if(fm_folder_model_find_iter_by_filename(model, &it, path->name))
        {
            switch(fv->mode)
            {
            case FM_FV_LIST_VIEW:
                {
                    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
                    gtk_tree_selection_select_iter(sel, &it);
                }
                break;
            case FM_FV_ICON_VIEW:
            case FM_FV_COMPACT_VIEW:
            case FM_FV_THUMBNAIL_VIEW:
                {
                    GtkTreePath* tp = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &it);
                    if(tp)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(fv->view), tp);
                        gtk_tree_path_free(tp);
                    }
                }
                break;
            }
        }
    }
}

/**
 * fm_folder_view_select_file_paths
 * @fv: a widget to apply
 * @paths: list of files to select
 *
 * Selects few files in the folder.
 *
 * Since: 0.1.0
 */
void fm_folder_view_select_file_paths(FmFolderView* fv, FmPathList* paths)
{
    GList* l;
    for(l = fm_path_list_peek_head_link(paths);l; l=l->next)
    {
        FmPath* path = FM_PATH(l->data);
        fm_folder_view_select_file_path(fv, path);
    }
}

/**
 * fm_folder_view_get_cwd_info
 * @fv: a widget to inspect
 *
 * Retrieves file info of the folder shown by @fv. Returned data are
 * owned by @fv and should not be freed by caller.
 *
 * Returns: (transfer none): file info descriptor of the folder.
 *
 * Since: 0.1.0
 */
FmFileInfo* fm_folder_view_get_cwd_info(FmFolderView* fv)
{
    FmFolder* folder = fm_folder_view_get_folder(fv);
    return folder ? fm_folder_get_info(folder) : NULL;
}

/**
 * fm_folder_view_get_cwd
 * @fv: a widget to inspect
 *
 * Retrieves file path of the folder shown by @fv. Returned data are
 * owned by @fv and should not be freed by caller.
 *
 * Returns: (transfer none): file path of the folder.
 *
 * Since: 0.1.0
 */
FmPath* fm_folder_view_get_cwd(FmFolderView* fv)
{
    FmFolder* folder = fm_folder_view_get_folder(fv);
    return folder ? fm_folder_get_path(folder) : NULL;
}

#if 0
gboolean fm_folder_view_is_loaded(FmFolderView* fv)
{
    return fv->folder && fm_folder_is_loaded(fv->folder);
}
#endif

static void cancel_pending_row_activated(FmFolderView* fv)
{
    if(fv->row_activated_idle)
    {
        g_source_remove(fv->row_activated_idle);
        fv->row_activated_idle = 0;
        gtk_tree_row_reference_free(fv->activated_row_ref);
        fv->activated_row_ref = NULL;
    }
}

/**
 * fm_folder_view_get_model
 * @fv: a widget to inspect
 *
 * Retrieves the model used by @fv. Returned data are owned by @fv and
 * should not be freed by caller.
 *
 * Returns: (transfer none): the model of view.
 *
 * Since: 0.1.16
 */
FmFolderModel* fm_folder_view_get_model(FmFolderView* fv)
{
    return fv->model;
}

/**
 * fm_folder_view_get_folder
 * @fv: a widget to inspect
 *
 * Retrieves the folder shown by @fv. Returned data are owned by @fv and
 * should not be freed by caller.
 *
 * Returns: (transfer none): the folder of view.
 *
 * Since: 1.0.0
 */
FmFolder* fm_folder_view_get_folder(FmFolderView* fv)
{
    return fv->model ? fm_folder_model_get_folder(fv->model) : NULL;
}

/**
 * fm_folder_view_set_model
 * @fv: a widget to apply
 * @model: (allow-none): new view model
 *
 * Changes model for the @fv.
 *
 * Since: 1.0.0
 */
void fm_folder_view_set_model(FmFolderView* fv, FmFolderModel* model)
{
    int icon_size;
    unset_model(fv);
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        cancel_pending_row_activated(fv);
        if(model)
        {
            icon_size = fm_config->small_icon_size;
            fm_folder_model_set_icon_size(model, icon_size);
        }
        gtk_tree_view_set_model(GTK_TREE_VIEW(fv->view), GTK_TREE_MODEL(model));
        break;
    case FM_FV_ICON_VIEW:
        icon_size = fm_config->big_icon_size;
        if(model)
            fm_folder_model_set_icon_size(model, icon_size);
        exo_icon_view_set_model(EXO_ICON_VIEW(fv->view), GTK_TREE_MODEL(model));
        break;
    case FM_FV_COMPACT_VIEW:
        if(model)
        {
            icon_size = fm_config->small_icon_size;
            fm_folder_model_set_icon_size(model, icon_size);
        }
        exo_icon_view_set_model(EXO_ICON_VIEW(fv->view), GTK_TREE_MODEL(model));
        break;
    case FM_FV_THUMBNAIL_VIEW:
        if(model)
        {
            icon_size = fm_config->thumbnail_size;
            fm_folder_model_set_icon_size(model, icon_size);
        }
        exo_icon_view_set_model(EXO_ICON_VIEW(fv->view), GTK_TREE_MODEL(model));
        break;
    }

    if(model)
    {
        fv->model = (FmFolderModel*)g_object_ref(model);
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), fv->sort_by, fv->sort_type);
        g_signal_connect(model, "sort-column-changed", G_CALLBACK(on_sort_col_changed), fv);
    }
    else
        fv->model = NULL;
}
