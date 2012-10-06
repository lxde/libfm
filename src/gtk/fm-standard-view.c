/*
 *      fm-standard-view.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
 * SECTION:fm-standard-view
 * @short_description: A folder view widget based on libexo.
 * @title: FmStandardView
 *
 * @include: libfm/fm-standard-view.h
 *
 * The #FmStandardView represents view of content of a folder with
 * support of drag & drop and other file/directory operations.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include "gtk-compat.h"

#include "fm-marshal.h"
#include "fm-config.h"
#include "fm-standard-view.h"
#include "fm-gtk-marshal.h"
#include "fm-cell-renderer-text.h"
#include "fm-cell-renderer-pixbuf.h"
#include "fm-gtk-utils.h"

#include "exo/exo-icon-view.h"
#include "exo/exo-tree-view.h"

#include "fm-dnd-src.h"
#include "fm-dnd-dest.h"
#include "fm-dnd-auto-scroll.h"

typedef struct _FmStandardViewColumnInfo
{
    FmFolderModelCol col_id;
    int width;
    int max_width;
    int min_width;
}FmStandardViewColumnInfo;

struct _FmStandardView
{
    GtkScrolledWindow parent;

    FmStandardViewMode mode;
    GtkSelectionMode sel_mode;
    GtkSortType sort_type;
    FmFolderModelCol sort_by;

    gboolean show_hidden;

    GtkWidget* view; /* either ExoIconView or ExoTreeView */
    FmFolderModel* model; /* FmStandardView doesn't use abstract GtkTreeModel! */
    FmCellRendererPixbuf* renderer_pixbuf; /* reference is bound to GtkWidget */
    guint icon_size_changed_handler;

    FmDndSrc* dnd_src; /* dnd source manager */
    FmDndDest* dnd_dest; /* dnd dest manager */

    /* wordarounds to fix new gtk+ bug introduced in gtk+ 2.20: #612802 */
    GtkTreeRowReference* activated_row_ref; /* for row-activated handler */
    guint row_activated_idle;

    /* for very large folder update */
    guint sel_changed_idle;
    gboolean sel_changed_pending;

    FmFileInfoList* cached_selected_files;
    FmPathList* cached_selected_file_paths;

    /* callbacks to creator */
    FmFolderViewUpdatePopup update_popup;
    FmLaunchFolderFunc open_folders;

    /* internal switches */
    void (*set_single_click)(GtkWidget* view, gboolean single_click);
    GtkTreePath* (*get_drop_path)(FmStandardView* fv, gint x, gint y);
    void (*select_all)(GtkWidget* view);
    void (*unselect_all)(GtkWidget* view);
    void (*select_invert)(FmFolderModel* model, GtkWidget* view);
    void (*select_path)(FmFolderModel* model, GtkWidget* view, GtkTreeIter* it);

    /* for columns */
    FmStandardViewColumnInfo* columns;
    guint n_columns;
};

#define SINGLE_CLICK_TIMEOUT    600

static const char* view_mode_names[FM_FV_N_VIEW_MODE] = {
    "icon", /* FM_FV_ICON_VIEW */
    "compact", /* FM_FV_COMPACT_VIEW */
    "thumbnail", /* FM_FV_THUMBNAIL_VIEW */
    "list" /* FM_FV_LIST_VIEW */
};

static void fm_standard_view_dispose(GObject *object);

static void fm_standard_view_view_init(FmFolderViewInterface* iface);

G_DEFINE_TYPE_WITH_CODE(FmStandardView, fm_standard_view, GTK_TYPE_SCROLLED_WINDOW,
                        G_IMPLEMENT_INTERFACE(FM_TYPE_FOLDER_VIEW, fm_standard_view_view_init))

static GList* fm_standard_view_get_selected_tree_paths(FmStandardView* fv);

static gboolean on_standard_view_focus_in(GtkWidget* widget, GdkEventFocus* evt);

static gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmStandardView* fv);
static void on_sel_changed(GObject* obj, FmStandardView* fv);

static void on_dnd_src_data_get(FmDndSrc* ds, FmStandardView* fv);

static void on_single_click_changed(FmConfig* cfg, FmStandardView* fv);
static void on_big_icon_size_changed(FmConfig* cfg, FmStandardView* fv);
static void on_small_icon_size_changed(FmConfig* cfg, FmStandardView* fv);
static void on_thumbnail_size_changed(FmConfig* cfg, FmStandardView* fv);

static void cancel_pending_row_activated(FmStandardView* fv);

//static void on_folder_reload(FmFolder* folder, FmFolderView* fv);
//static void on_folder_loaded(FmFolder* folder, FmFolderView* fv);
//static void on_folder_unmounted(FmFolder* folder, FmFolderView* fv);
//static void on_folder_removed(FmFolder* folder, FmFolderView* fv);

static void fm_standard_view_class_init(FmStandardViewClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass *widget_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_standard_view_dispose;
    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->focus_in_event = on_standard_view_focus_in;

    fm_standard_view_parent_class = (GtkScrolledWindowClass*)g_type_class_peek(GTK_TYPE_SCROLLED_WINDOW);
}

static gboolean on_standard_view_focus_in(GtkWidget* widget, GdkEventFocus* evt)
{
    FmStandardView* fv = FM_STANDARD_VIEW(widget);
    if( fv->view )
    {
        gtk_widget_grab_focus(fv->view);
        return TRUE;
    }
    return FALSE;
}

static void on_single_click_changed(FmConfig* cfg, FmStandardView* fv)
{
    if(fv->set_single_click)
        fv->set_single_click(fv->view, cfg->single_click);
}

static void on_icon_view_item_activated(ExoIconView* iv, GtkTreePath* path, FmStandardView* fv)
{
    fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), path, FM_FV_ACTIVATED);
}

static gboolean on_idle_tree_view_row_activated(gpointer user_data)
{
    FmStandardView* fv = (FmStandardView*)user_data;
    GtkTreePath* path;
    GDK_THREADS_ENTER();
    if(g_source_is_destroyed(g_main_current_source()))
        goto _end;
    if(gtk_tree_row_reference_valid(fv->activated_row_ref))
    {
        path = gtk_tree_row_reference_get_path(fv->activated_row_ref);
        fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), path, FM_FV_ACTIVATED);
        gtk_tree_path_free(path);
    }
    gtk_tree_row_reference_free(fv->activated_row_ref);
    fv->activated_row_ref = NULL;
    fv->row_activated_idle = 0;
_end:
    GDK_THREADS_LEAVE();
    return FALSE;
}

static void on_tree_view_row_activated(GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, FmStandardView* fv)
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
        fv->row_activated_idle = g_idle_add(on_idle_tree_view_row_activated, fv);
    }
}

static void fm_standard_view_init(FmStandardView *self)
{
    gtk_scrolled_window_set_hadjustment((GtkScrolledWindow*)self, NULL);
    gtk_scrolled_window_set_vadjustment((GtkScrolledWindow*)self, NULL);
    gtk_scrolled_window_set_policy((GtkScrolledWindow*)self, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* config change notifications */
    g_signal_connect(fm_config, "changed::single_click", G_CALLBACK(on_single_click_changed), self);

    /* dnd support */
    self->dnd_src = fm_dnd_src_new(NULL);
    g_signal_connect(self->dnd_src, "data-get", G_CALLBACK(on_dnd_src_data_get), self);

    self->dnd_dest = fm_dnd_dest_new_with_handlers(NULL);

    self->mode = -1;
    self->sort_type = GTK_SORT_ASCENDING;
    self->sort_by = FM_FOLDER_MODEL_COL_NAME;
}

/**
 * fm_folder_view_new
 * @mode: initial mode of view
 *
 * Returns: a new #FmFolderView widget.
 *
 * Since: 0.1.0
 * Deprecated: 1.0.1: Use fm_standard_view_new() instead.
 */
FmFolderView* fm_folder_view_new(guint mode)
{
    return (FmFolderView*)fm_standard_view_new(mode, NULL, NULL);
}

/**
 * fm_standard_view_new
 * @mode: initial mode of view
 * @update_popup: (allow-none): callback to update context menu for files
 * @open_folders: (allow-none): callback to open folder on activation
 *
 * Creates new folder view.
 *
 * Returns: a new #FmStandardView widget.
 *
 * Since: 1.0.1
 */
FmStandardView* fm_standard_view_new(FmStandardViewMode mode,
                                        FmFolderViewUpdatePopup update_popup,
                                        FmLaunchFolderFunc open_folders)
{
    FmStandardView* fv = (FmStandardView*)g_object_new(FM_STANDARD_VIEW_TYPE, NULL);
    FmFolderModelCol cols[] = {
        FM_FOLDER_MODEL_COL_NAME,
        FM_FOLDER_MODEL_COL_DESC,
        FM_FOLDER_MODEL_COL_SIZE,
        FM_FOLDER_MODEL_COL_MTIME};
    /* Set default columns to show in detailed list mode.
     * FIXME: cols should be passed to fm_standard_view_new() as a parameter instead.
     * This breaks API/ABI though. Let's do it later. */
    fm_standard_view_set_columns(fv, cols, G_N_ELEMENTS(cols));
    fm_standard_view_set_mode(fv, mode);
    fv->update_popup = update_popup;
    fv->open_folders = open_folders;
    return fv;
}

static void unset_model(FmStandardView* fv)
{
    if(fv->model)
    {
        FmFolderModel* model = fv->model;
        /* g_debug("unset_model: %p, n_ref = %d", model, G_OBJECT(model)->ref_count); */
        g_object_unref(model);
        fv->model = NULL;
    }
}

static void unset_view(FmStandardView* fv);
static void fm_standard_view_dispose(GObject *object)
{
    FmStandardView *self;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_STANDARD_VIEW(object));
    self = (FmStandardView*)object;
    /* g_debug("fm_standard_view_dispose: %p", self); */

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

    if(self->sel_changed_idle)
    {
        g_source_remove(self->sel_changed_idle);
        self->sel_changed_idle = 0;
    }

    if(self->icon_size_changed_handler)
    {
        g_signal_handler_disconnect(fm_config, self->icon_size_changed_handler);
        self->icon_size_changed_handler = 0;
    }
    (* G_OBJECT_CLASS(fm_standard_view_parent_class)->dispose)(object);
}

static void set_icon_size(FmStandardView* fv, guint icon_size)
{
    FmCellRendererPixbuf* render = fv->renderer_pixbuf;

    fm_cell_renderer_pixbuf_set_fixed_size(render, icon_size, icon_size);

    if(!fv->model)
        return;

    fm_folder_model_set_icon_size(fv->model, icon_size);

    if( fv->mode != FM_FV_LIST_VIEW ) /* this is an ExoIconView */
    {
        /* FIXME: reset ExoIconView item sizes */
        /* set row spacing in range 2...12 pixels */
        gint c_size = MIN(12, 2 + icon_size / 8);
        exo_icon_view_set_row_spacing(EXO_ICON_VIEW(fv->view), c_size);
    }
}

static void on_big_icon_size_changed(FmConfig* cfg, FmStandardView* fv)
{
    set_icon_size(fv, cfg->big_icon_size);
}

static void on_small_icon_size_changed(FmConfig* cfg, FmStandardView* fv)
{
    set_icon_size(fv, cfg->small_icon_size);
}

static void on_thumbnail_size_changed(FmConfig* cfg, FmStandardView* fv)
{
    /* FIXME: thumbnail and icons should have different sizes */
    /* maybe a separate API: fm_folder_model_set_thumbnail_size() */
    set_icon_size(fv, cfg->thumbnail_size);
}

static GtkTreePath* get_drop_path_list_view(FmStandardView* fv, gint x, gint y)
{
    GtkTreePath* tp = NULL;
    GtkTreeViewColumn* col;

    gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(fv->view), x, y, &x, &y);
    if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(fv->view), x, y, &tp, &col, NULL, NULL))
    {
        if(gtk_tree_view_column_get_sort_column_id(col)!=FM_FOLDER_MODEL_COL_NAME)
        {
            gtk_tree_path_free(tp);
            tp = NULL;
        }
    }
    if(tp)
        gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(fv->view), tp,
                                        GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
    return tp;
}

static GtkTreePath* get_drop_path_icon_view(FmStandardView* fv, gint x, gint y)
{
    GtkTreePath* tp;

    tp = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(fv->view), x, y);
    exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(fv->view), tp, EXO_ICON_VIEW_DROP_INTO);
    return tp;
}

static gboolean on_drag_motion(GtkWidget *dest_widget,
                                 GdkDragContext *drag_context,
                                 gint x,
                                 gint y,
                                 guint time,
                                 FmStandardView* fv)
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
        GtkTreePath* tp = fv->get_drop_path(fv, x, y);
        if(tp)
        {
            GtkTreeIter it;
            if(gtk_tree_model_get_iter(GTK_TREE_MODEL(fv->model), &it, tp))
            {
                FmFileInfo* fi;
                gtk_tree_model_get(GTK_TREE_MODEL(fv->model), &it, FM_FOLDER_MODEL_COL_INFO, &fi, -1);
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

static inline void create_icon_view(FmStandardView* fv, GList* sels)
{
    GList *l;
    GtkCellRenderer* render;
    FmFolderModel* model = fv->model;
    int icon_size = 0;

    fv->view = exo_icon_view_new();

    fv->renderer_pixbuf = fm_cell_renderer_pixbuf_new();
    render = (GtkCellRenderer*)fv->renderer_pixbuf;

    g_object_set((GObject*)render, "follow-state", TRUE, NULL );
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(fv->view), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render, "pixbuf", FM_FOLDER_MODEL_COL_ICON );
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render, "info", FM_FOLDER_MODEL_COL_INFO );

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
                         "max-height", 70,
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
                         "max-height", 90,
                         "alignment", PANGO_ALIGN_CENTER,
                         "xalign", 0.5,
                         "yalign", 0.0,
                         NULL );
            exo_icon_view_set_column_spacing( (ExoIconView*)fv->view, 8 );
            exo_icon_view_set_item_width ( (ExoIconView*)fv->view, 200 );
        }
    }
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(fv->view), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(fv->view), render,
                                "text", FM_FOLDER_MODEL_COL_NAME );
    exo_icon_view_set_item_width((ExoIconView*)fv->view, 96);
    exo_icon_view_set_search_column((ExoIconView*)fv->view, FM_FOLDER_MODEL_COL_NAME);
    g_signal_connect(fv->view, "item-activated", G_CALLBACK(on_icon_view_item_activated), fv);
    g_signal_connect(fv->view, "selection-changed", G_CALLBACK(on_sel_changed), fv);
    exo_icon_view_set_model((ExoIconView*)fv->view, (GtkTreeModel*)fv->model);
    exo_icon_view_set_selection_mode((ExoIconView*)fv->view, fv->sel_mode);
    exo_icon_view_set_single_click((ExoIconView*)fv->view, fm_config->single_click);
    exo_icon_view_set_single_click_timeout((ExoIconView*)fv->view, SINGLE_CLICK_TIMEOUT);

    for(l = sels;l;l=l->next)
        exo_icon_view_select_path((ExoIconView*)fv->view, l->data);
}

static void on_column_width_changed(GtkTreeViewColumn* col, FmStandardView* view)
{
    g_print("column width changed: %p, %d\n", col, gtk_tree_view_column_get_width(col));
    return;
#if 0
    int width;
    GList* cols = gtk_tree_view_get_columns(GTK_TREE_VIEW(view->view));
    int pos = g_list_index(cols, col);
    g_list_free(cols);
    width = gtk_tree_view_column_get_width(col);

    /* only handle it if the width really changed */
    if(width != view->column_widths[pos])
    {
        // FIXME: what if column_widths = NULL?
        view->column_widths[pos] = width;
        // g_signal_emit(view, signals[COLUMN_WIDTH_CHANGED], 0);
    }
#endif
}

static GtkTreeViewColumn* create_list_view_column(FmStandardView* fv, FmFolderModelCol col_id)
{
    GtkTreeViewColumn* col = gtk_tree_view_column_new();
    GtkCellRenderer* render = gtk_cell_renderer_text_new();
    const char* title = fm_folder_model_col_get_title(col_id);

    gtk_tree_view_column_set_title(col, title);

    if(G_UNLIKELY(col_id == FM_FOLDER_MODEL_COL_NAME)) /* special handling for Name column */
    {
        gtk_tree_view_column_pack_start(col, GTK_CELL_RENDERER(fv->renderer_pixbuf), FALSE);
        gtk_tree_view_column_set_attributes(col, GTK_CELL_RENDERER(fv->renderer_pixbuf),
                                            "pixbuf", FM_FOLDER_MODEL_COL_ICON,
                                            "info", FM_FOLDER_MODEL_COL_INFO, NULL);
        g_object_set(render, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_column_set_expand(col, TRUE);
        gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_fixed_width(col, 200);
    }
    else
    {
        if(G_UNLIKELY(col_id == FM_FOLDER_MODEL_COL_SIZE))
            g_object_set(render, "xalign", 1.0, NULL);
    }

    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_set_attributes(col, render, "text", col_id, NULL);
    gtk_tree_view_column_set_resizable(col, TRUE);
    if(fm_folder_model_col_is_sortable(col_id))
        gtk_tree_view_column_set_sort_column_id(col, col_id);

    g_signal_connect(col, "notify::width", G_CALLBACK(on_column_width_changed), fv);

    return col;
}

static inline void create_list_view(FmStandardView* fv, GList* sels)
{
    GtkTreeViewColumn* col;
    GtkTreeSelection* ts;
    GList *l;
    GtkCellRenderer* render;
    FmFolderModel* model = fv->model;
    int icon_size = 0;
    int i;

    fv->view = exo_tree_view_new();

    fv->renderer_pixbuf = fm_cell_renderer_pixbuf_new();
    fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_small_icon_size_changed), fv);
    icon_size = fm_config->small_icon_size;
    fm_cell_renderer_pixbuf_set_fixed_size(fv->renderer_pixbuf, icon_size, icon_size);
    if(model)
        fm_folder_model_set_icon_size(model, icon_size);

    /* create columns */
    if(fv->columns)
    {
        for(i = 0; i < fv->n_columns; ++i)
        {
            FmStandardViewColumnInfo* info = &fv->columns[i];
            /* if one is cached already, use it. otherwise, create a new one */
            col = create_list_view_column(fv, info->col_id);
            gtk_tree_view_append_column(fv->view, col);
            if(i == 0)
            {
                /* only this column is activable */
                exo_tree_view_set_activable_column((ExoTreeView*)fv->view, col);
                /* FIXME: should we use columns[0] here or FM_FOLDER_MODEL_COL_NAME? */
            }
        }
    }

    gtk_tree_view_set_search_column(GTK_TREE_VIEW(fv->view), FM_FOLDER_MODEL_COL_NAME);

    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(fv->view), TRUE);
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

static void unset_view(FmStandardView* fv)
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
    /* these signals connected by fm_standard_view_set_mode() */
    g_signal_handlers_disconnect_by_func(fv->view, on_drag_motion, fv);
    g_signal_handlers_disconnect_by_func(fv->view, on_btn_pressed, fv);

    fm_dnd_unset_dest_auto_scroll(fv->view);
    gtk_widget_destroy(GTK_WIDGET(fv->view));
    fv->view = NULL;
}

static void select_all_list_view(GtkWidget* view)
{
    GtkTreeSelection * tree_sel;
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_select_all(tree_sel);
}

static void unselect_all_list_view(GtkWidget* view)
{
    GtkTreeSelection * tree_sel;
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_unselect_all(tree_sel);
}

static void select_invert_list_view(FmFolderModel* model, GtkWidget* view)
{
    GtkTreeSelection *tree_sel;
    GtkTreeIter it;
    if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it))
        return;
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    do
    {
        if(gtk_tree_selection_iter_is_selected(tree_sel, &it))
            gtk_tree_selection_unselect_iter(tree_sel, &it);
        else
            gtk_tree_selection_select_iter(tree_sel, &it);
    }while( gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it ));
}

static void select_invert_icon_view(FmFolderModel* model, GtkWidget* view)
{
    GtkTreePath* path;
    int i, n;
    n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model), NULL);
    if(n == 0)
        return;
    path = gtk_tree_path_new_first();
    for( i=0; i<n; ++i, gtk_tree_path_next(path) )
    {
        if ( exo_icon_view_path_is_selected(EXO_ICON_VIEW(view), path))
            exo_icon_view_unselect_path(EXO_ICON_VIEW(view), path);
        else
            exo_icon_view_select_path(EXO_ICON_VIEW(view), path);
    }
    gtk_tree_path_free(path);
}

static void select_path_list_view(FmFolderModel* model, GtkWidget* view, GtkTreeIter* it)
{
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_select_iter(sel, it);
}

static void select_path_icon_view(FmFolderModel* model, GtkWidget* view, GtkTreeIter* it)
{
    GtkTreePath* tp = gtk_tree_model_get_path(GTK_TREE_MODEL(model), it);
    if(tp)
    {
        exo_icon_view_select_path(EXO_ICON_VIEW(view), tp);
        gtk_tree_path_free(tp);
    }
}

/**
 * fm_folder_view_set_mode
 * @fv: a widget to apply
 * @mode: new mode of view
 *
 * Since: 0.1.0
 * Deprecated: 1.0.1: Use fm_standard_view_set_mode() instead.
 */
void fm_folder_view_set_mode(FmFolderView* fv, guint mode)
{
    g_return_if_fail(FM_IS_STANDARD_VIEW(fv));

    fm_standard_view_set_mode((FmStandardView*)fv, mode);
}

/**
 * fm_standard_view_set_mode
 * @fv: a widget to apply
 * @mode: new mode of view
 *
 * Before 1.0.1 this API had name fm_folder_view_set_mode.
 *
 * Changes current view mode for folder in @fv.
 *
 * Since: 0.1.0
 */
void fm_standard_view_set_mode(FmStandardView* fv, FmStandardViewMode mode)
{
    if( mode != fv->mode )
    {
        GList *sels;
        gboolean has_focus;

        if( G_LIKELY(fv->view) )
        {
            has_focus = gtk_widget_has_focus(fv->view);
            /* preserve old selections */
            sels = fm_standard_view_get_selected_tree_paths(fv);

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
            fv->set_single_click = (void(*)(GtkWidget*,gboolean))exo_icon_view_set_single_click;
            fv->get_drop_path = get_drop_path_icon_view;
            fv->select_all = (void(*)(GtkWidget*))exo_icon_view_select_all;
            fv->unselect_all = (void(*)(GtkWidget*))exo_icon_view_unselect_all;
            fv->select_invert = select_invert_icon_view;
            fv->select_path = select_path_icon_view;
            break;
        case FM_FV_LIST_VIEW: /* detailed list view */
            create_list_view(fv, sels);
            fv->set_single_click = (void(*)(GtkWidget*,gboolean))exo_tree_view_set_single_click;
            fv->get_drop_path = get_drop_path_list_view;
            fv->select_all = select_all_list_view;
            fv->unselect_all = unselect_all_list_view;
            fv->select_invert = select_invert_list_view;
            fv->select_path = select_path_list_view;
        default:
            break;
        }
        g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
        g_list_free(sels);

        /* FIXME: maybe calling set_icon_size here is a good idea */

        fm_dnd_src_set_widget(fv->dnd_src, fv->view);
        fm_dnd_dest_set_widget(fv->dnd_dest, fv->view);
        g_signal_connect_after(fv->view, "drag-motion", G_CALLBACK(on_drag_motion), fv);
        /* connecting it after sometimes conflicts with system configuration
           (bug #3559831) so we just hope here it will be handled in order
           of connecting, i.e. after ExoIconView or ExoTreeView handler */
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
 * Returns: current mode of view.
 *
 * Since: 0.1.0
 * Deprecated: 1.0.1: Use fm_standard_view_get_mode() instead.
 */
guint fm_folder_view_get_mode(FmFolderView* fv)
{
    g_return_val_if_fail(FM_IS_STANDARD_VIEW(fv), 0);

    return ((FmStandardView*)fv)->mode;
}

/**
 * fm_standard_view_get_mode
 * @fv: a widget to inspect
 *
 * Retrieves current view mode for folder in @fv.
 *
 * Before 1.0.1 this API had name fm_folder_view_get_mode.
 *
 * Returns: current mode of view.
 *
 * Since: 0.1.0
 */
FmStandardViewMode fm_standard_view_get_mode(FmStandardView* fv)
{
    return fv->mode;
}

static void fm_standard_view_set_selection_mode(FmFolderView* ffv, GtkSelectionMode mode)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
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

static GtkSelectionMode fm_standard_view_get_selection_mode(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fv->sel_mode;
}

static void fm_standard_view_set_sort(FmFolderView* ffv, GtkSortType type, FmFolderModelCol by)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    fv->sort_type = type;
    fv->sort_by = by;
}

static void fm_standard_view_get_sort(FmFolderView* ffv, GtkSortType* type, FmFolderModelCol* by)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    *type = fv->sort_type;
    *by = fv->sort_by;
}

static void fm_standard_view_set_show_hidden(FmFolderView* ffv, gboolean show)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    fv->show_hidden = show;
}

static gboolean fm_standard_view_get_show_hidden(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fv->show_hidden;
}

/* returned list should be freed with g_list_free_full(list, gtk_tree_path_free) */
static GList* fm_standard_view_get_selected_tree_paths(FmStandardView* fv)
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

static inline FmFileInfoList* fm_standard_view_get_selected_files(FmStandardView* fv)
{
    /* don't generate the data again if we have it cached. */
    if(!fv->cached_selected_files)
    {
        GList* sels = fm_standard_view_get_selected_tree_paths(fv);
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
                gtk_tree_model_get(GTK_TREE_MODEL(fv->model), &it, FM_FOLDER_MODEL_COL_INFO, &fi, -1);
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

static FmFileInfoList* fm_standard_view_dup_selected_files(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fm_file_info_list_ref(fm_standard_view_get_selected_files(fv));
}

static FmPathList* fm_standard_view_dup_selected_file_paths(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    if(!fv->cached_selected_file_paths)
    {
        FmFileInfoList* files = fm_standard_view_get_selected_files(fv);
        if(files)
            fv->cached_selected_file_paths = fm_path_list_new_from_file_info_list(files);
        else
            fv->cached_selected_file_paths = NULL;
    }
    return fm_path_list_ref(fv->cached_selected_file_paths);
}

static gint fm_standard_view_count_selected_files(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    gint count = 0;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
    {
        GtkTreeSelection* sel;
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fv->view));
        count = gtk_tree_selection_count_selected_rows(sel);
        break;
    }
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        count = exo_icon_view_count_selected_items(EXO_ICON_VIEW(fv->view));
        break;
    }
    return count;
}

static gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmStandardView* fv)
{
    GList* sels = NULL;
    FmFolderViewClickType type = 0;
    GtkTreePath* tp = NULL;

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
        sels = fm_standard_view_get_selected_tree_paths(fv);
        if( sels || type == FM_FV_CONTEXT_MENU )
        {
            fm_folder_view_item_clicked(FM_FOLDER_VIEW(fv), tp, type);
            if(sels)
            {
                g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
                g_list_free(sels);
            }
        }
    }
    if(tp)
        gtk_tree_path_free(tp);
    return FALSE;
}

/* TODO: select files by custom func, not yet implemented */
void fm_folder_view_select_custom(FmFolderView* fv, GFunc filter, gpointer user_data)
{
}

static void fm_standard_view_select_all(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    if(fv->select_all)
        fv->select_all(fv->view);
}

static void fm_standard_view_unselect_all(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    if(fv->unselect_all)
        fv->unselect_all(fv->view);
}

static void on_dnd_src_data_get(FmDndSrc* ds, FmStandardView* fv)
{
    FmFileInfoList* files = fm_standard_view_dup_selected_files(FM_FOLDER_VIEW(fv));
    fm_dnd_src_set_files(ds, files);
    if(files)
        fm_file_info_list_unref(files);
}

static gboolean on_sel_changed_real(FmStandardView* fv)
{
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
    fm_folder_view_sel_changed(NULL, FM_FOLDER_VIEW(fv));
    fv->sel_changed_pending = FALSE;
    return TRUE;
}

/*
 * We limit "sel-changed" emitting here:
 * - if no signal was in last 200ms then signal is emitted immidiately
 * - if there was < 200ms since last signal then it's marked as pending
 *   and signal will be emitted when that 200ms timeout ends
 */
static gboolean on_sel_changed_idle(gpointer user_data)
{
    FmStandardView* fv = (FmStandardView*)user_data;
    gboolean ret = FALSE;

    GDK_THREADS_ENTER();
    /* check if fv is destroyed already */
    if(g_source_is_destroyed(g_main_current_source()))
        goto _end;
    if(fv->sel_changed_pending) /* fast changing detected! continue... */
        ret = on_sel_changed_real(fv);
    fv->sel_changed_idle = 0;
_end:
    GDK_THREADS_LEAVE();
    return ret;
}

static void on_sel_changed(GObject* obj, FmStandardView* fv)
{
    if(!fv->sel_changed_idle)
    {
        fv->sel_changed_idle = g_timeout_add_full(G_PRIORITY_HIGH_IDLE, 200,
                                                  on_sel_changed_idle, fv, NULL);
        on_sel_changed_real(fv);
    }
    else
        fv->sel_changed_pending = TRUE;
}

static void fm_standard_view_select_invert(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    if(fv->select_invert)
        fv->select_invert(fv->model, fv->view);
}

static FmFolder* fm_standard_view_get_folder(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fv->model ? fm_folder_model_get_folder(fv->model) : NULL;
}

static void fm_standard_view_select_file_path(FmFolderView* ffv, FmPath* path)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    FmFolder* folder = fm_standard_view_get_folder(ffv);
    FmPath* cwd = folder ? fm_folder_get_path(folder) : NULL;
    if(cwd && fm_path_equal(fm_path_get_parent(path), cwd))
    {
        FmFolderModel* model = fv->model;
        GtkTreeIter it;
        if(fv->select_path &&
           fm_folder_model_find_iter_by_filename(model, &it, fm_path_get_basename(path)))
            fv->select_path(model, fv->view, &it);
    }
}

static void fm_standard_view_get_custom_menu_callbacks(FmFolderView* ffv,
        FmFolderViewUpdatePopup *update_popup, FmLaunchFolderFunc *open_folders)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    *update_popup = fv->update_popup;
    *open_folders = fv->open_folders;
}

#if 0
static gboolean fm_standard_view_is_loaded(FmStandardView* fv)
{
    return fv->folder && fm_folder_is_loaded(fv->folder);
}
#endif

static void cancel_pending_row_activated(FmStandardView* fv)
{
    if(fv->row_activated_idle)
    {
        g_source_remove(fv->row_activated_idle);
        fv->row_activated_idle = 0;
        gtk_tree_row_reference_free(fv->activated_row_ref);
        fv->activated_row_ref = NULL;
    }
}

static FmFolderModel* fm_standard_view_get_model(FmFolderView* ffv)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
    return fv->model;
}

static void fm_standard_view_set_model(FmFolderView* ffv, FmFolderModel* model)
{
    FmStandardView* fv = FM_STANDARD_VIEW(ffv);
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
        fv->model = (FmFolderModel*)g_object_ref(model);
    else
        fv->model = NULL;
}

static void fm_standard_view_view_init(FmFolderViewInterface* iface)
{
    iface->set_sel_mode = fm_standard_view_set_selection_mode;
    iface->get_sel_mode = fm_standard_view_get_selection_mode;
    iface->set_sort = fm_standard_view_set_sort;
    iface->get_sort = fm_standard_view_get_sort;
    iface->set_show_hidden = fm_standard_view_set_show_hidden;
    iface->get_show_hidden = fm_standard_view_get_show_hidden;
    iface->get_folder = fm_standard_view_get_folder;
    iface->set_model = fm_standard_view_set_model;
    iface->get_model = fm_standard_view_get_model;
    iface->count_selected_files = fm_standard_view_count_selected_files;
    iface->dup_selected_files = fm_standard_view_dup_selected_files;
    iface->dup_selected_file_paths = fm_standard_view_dup_selected_file_paths;
    iface->select_all = fm_standard_view_select_all;
    iface->unselect_all = fm_standard_view_unselect_all;
    iface->select_invert = fm_standard_view_select_invert;
    iface->select_file_path = fm_standard_view_select_file_path;
    iface->get_custom_menu_callbacks = fm_standard_view_get_custom_menu_callbacks;
}

const char* fm_standard_view_mode_to_str(FmStandardViewMode mode)
{
    if(G_UNLIKELY(mode < 0 || mode >= FM_FV_N_VIEW_MODE))
        return NULL;
    return view_mode_names[mode];
}

FmStandardViewMode fm_standard_view_mode_from_str(const char* str)
{
    FmStandardViewMode mode;
    for(mode = 0; mode < FM_FV_N_VIEW_MODE; ++mode)
    {
        if(strcmp(str, view_mode_names[mode]) == 0)
            return mode;
    }
    return (FmStandardViewMode)-1;
}

void fm_standard_view_set_columns(FmStandardView* view, FmFolderModelCol* col_ids, int n)
{
    GtkTreeViewColumn* col;
    GtkTreeViewColumn* old_cols[FM_FOLDER_MODEL_N_VISIBLE_COLS];
    memset(old_cols, 0, sizeof(old_cols));
    int i;

    if(view->columns)
    {
        /* remove all existing columns in GtkTreeView, and cache them.
         * we cache existing GtkTreeViewColumn objects for later use
         * so if new column_ids are the same as the old ones, just the
         * order changes, we don't need to recreate any object. */
        FmStandardViewColumnInfo* info = view->columns;
        if(view->mode == FM_FV_LIST_VIEW && view->view)
        {
            /* after we removed the first column, the next one becomes 
             * the first and always has index 0. */
            while(col = gtk_tree_view_get_column(view->view, 0))
            {
                /* add a ref to the column object so we can keep it. */
                old_cols[info->col_id] = GTK_TREE_VIEW_COLUMN(g_object_ref(col));
                gtk_tree_view_remove_column(view->view, col);
                ++info;
            }
        }
        /* free old column infos */
        g_free(view->columns);
    }
    view->n_columns = n;
    view->columns = g_new0(FmStandardViewColumnInfo, n);
    for(i = 0; i < n; ++i)
    {
        FmStandardViewColumnInfo* info = &view->columns[i];
        info->col_id = col_ids[i];
    }

    /* add real columns to GtkTreeView if needed */
    if(view->mode == FM_FV_LIST_VIEW && view->view)
    {
        /* the tree view is already created so we need to add the columns to it. */
        for(i = 0; i < n; ++i)
        {
            FmStandardViewColumnInfo* info = &view->columns[i];
            FmFolderModelCol col_id = info->col_id;
            /* if one is cached already, use it. otherwise, create a new one */
            if(old_cols[col_id])
                col = old_cols[col_id];
            else
                col = create_list_view_column(view, col_id);
            gtk_tree_view_append_column(view->view, col);
            /* only one column is activable */
            if(i == 0)
                exo_tree_view_set_activable_column((ExoTreeView*)view->view, col);
        }
    }

    /* destroy cached column objects */
    for(i = 0; i < FM_FOLDER_MODEL_N_VISIBLE_COLS; ++i)
    {
        if((col = old_cols[i]))
            g_object_unref(col);
    }
}

/*
FmFolderModelCol* fm_standard_view_get_columns(FmStandardView* view)
{
    return view->columns;
}

int fm_standard_view_get_n_columns(FmStandardView* view)
{
    return view->n_columns;
}

guint* fm_standard_view_get_column_widths(FmStandardView* view)
{
    return view->column_widths;
}

*/
