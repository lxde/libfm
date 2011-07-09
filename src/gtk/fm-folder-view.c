/*
 *      folder-view.c
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
    CHDIR,
    LOADED,
    CLICKED,
    SEL_CHANGED,
    SORT_CHANGED,
    N_SIGNALS
};

#define SINGLE_CLICK_TIMEOUT    600

static guint signals[N_SIGNALS];

static void fm_folder_view_finalize              (GObject *object);
G_DEFINE_TYPE(FmFolderView, fm_folder_view, GTK_TYPE_SCROLLED_WINDOW);

static GList* fm_folder_view_get_selected_tree_paths(FmFolderView* fv);

static gboolean on_folder_view_focus_in(GtkWidget* widget, GdkEventFocus* evt);
static void on_chdir(FmFolderView* fv, FmPath* dir_path);
static void on_loaded(FmFolderView* fv, FmPath* dir_path);
static void on_model_loaded(FmFolderModel* model, FmFolderView* fv);
static FmJobErrorAction on_folder_err(FmFolder* folder, GError* err, FmJobErrorSeverity severity, FmFolderView* fv);

static gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmFolderView* fv);
static void on_sel_changed(GObject* obj, FmFolderView* fv);
static void on_sort_col_changed(GtkTreeSortable* sortable, FmFolderView* fv);

static void on_dnd_src_data_get(FmDndSrc* ds, FmFolderView* fv);

static void on_single_click_changed(FmConfig* cfg, FmFolderView* fv);
static void on_big_icon_size_changed(FmConfig* cfg, FmFolderView* fv);
static void on_small_icon_size_changed(FmConfig* cfg, FmFolderView* fv);
static void on_thumbnail_size_changed(FmConfig* cfg, FmFolderView* fv);

static void cancel_pending_row_activated(FmFolderView* fv);

static void fm_folder_view_class_init(FmFolderViewClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass *widget_class;
    FmFolderViewClass *fv_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_folder_view_finalize;
    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->focus_in_event = on_folder_view_focus_in;
    fv_class = FM_FOLDER_VIEW_CLASS(klass);
    fv_class->chdir = on_chdir;
    fv_class->loaded = on_loaded;

    fm_folder_view_parent_class = (GtkScrolledWindowClass*)g_type_class_peek(GTK_TYPE_SCROLLED_WINDOW);

    signals[CHDIR]=
        g_signal_new("chdir",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewClass, chdir),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[LOADED]=
        g_signal_new("loaded",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewClass, loaded),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[CLICKED]=
        g_signal_new("clicked",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewClass, clicked),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__UINT_POINTER,
                     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);

    /* Emitted when selection of the view got changed.
     * Currently selected files are passed as the parameter.
     * If there is no file selected, NULL is passed instead. */
    signals[SEL_CHANGED]=
        g_signal_new("sel-changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewClass, sel_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);

    /* Emitted when sorting of the view got changed. */
    signals[SORT_CHANGED]=
        g_signal_new("sort-changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewClass, sort_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

gboolean on_folder_view_focus_in(GtkWidget* widget, GdkEventFocus* evt)
{
    FmFolderView* fv = (FmFolderView*)widget;
    if( fv->view )
    {
        gtk_widget_grab_focus(fv->view);
        return TRUE;
    }
    return FALSE;
}

void on_chdir(FmFolderView* fv, FmPath* dir_path)
{
    GtkWidget* toplevel = gtk_widget_get_toplevel((GtkWidget*)fv);
    if(GTK_WIDGET_REALIZED(toplevel))
    {
        GdkCursor* cursor = gdk_cursor_new(GDK_WATCH);
        gdk_window_set_cursor(toplevel->window, cursor);
    }
}

void on_loaded(FmFolderView* fv, FmPath* dir_path)
{
    GtkWidget* toplevel = gtk_widget_get_toplevel((GtkWidget*)fv);
    if(GTK_WIDGET_REALIZED(toplevel))
        gdk_window_set_cursor(toplevel->window, NULL);
}

void on_model_loaded(FmFolderModel* model, FmFolderView* fv)
{
    FmFolder* folder = model->dir;
    char* msg;
    /* FIXME: prevent direct access to data members */
    g_signal_emit(fv, signals[LOADED], 0, folder->dir_path);
}

FmJobErrorAction on_folder_err(FmFolder* folder, GError* err, FmJobErrorSeverity severity, FmFolderView* fv)
{
    GtkWindow* parent = (GtkWindow*)gtk_widget_get_toplevel((GtkWidget*)fv);
    if( err->domain == G_IO_ERROR )
    {
        if( err->code == G_IO_ERROR_NOT_MOUNTED && severity < FM_JOB_ERROR_CRITICAL )
            if(fm_mount_path(parent, folder->dir_path, TRUE))
                return FM_JOB_RETRY;
        else if(err->code == G_IO_ERROR_FAILED_HANDLED)
            return FM_JOB_CONTINUE;
    }
    fm_show_error(parent, NULL, err->message);
    return FM_JOB_CONTINUE;
}

void on_single_click_changed(FmConfig* cfg, FmFolderView* fv)
{
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        exo_tree_view_set_single_click((ExoTreeView*)fv->view, cfg->single_click);
        break;
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        exo_icon_view_set_single_click((ExoIconView*)fv->view, cfg->single_click);
        break;
    }
}

static void item_clicked( FmFolderView* fv, GtkTreePath* path, FmFolderViewClickType type )
{
    GtkTreeIter it;
    if(path)
    {
        if(gtk_tree_model_get_iter(fv->model, &it, path))
        {
            FmFileInfo* fi;
            gtk_tree_model_get(fv->model, &it, COL_FILE_INFO, &fi, -1);
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

static gboolean on_idle_tree_view_row_activated(FmFolderView* fv)
{
    GtkTreePath* path;
    if(gtk_tree_row_reference_valid(fv->activated_row_ref))
    {
        path = gtk_tree_row_reference_get_path(fv->activated_row_ref);
        item_clicked(fv, path, FM_FV_ACTIVATED);
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
        g_idle_add((GSourceFunc)on_idle_tree_view_row_activated, fv);
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


GtkWidget* fm_folder_view_new(FmFolderViewMode mode)
{
    FmFolderView* fv = (FmFolderView*)g_object_new(FM_FOLDER_VIEW_TYPE, NULL);
    fm_folder_view_set_mode(fv, mode);
    return (GtkWidget*)fv;
}


static void fm_folder_view_finalize(GObject *object)
{
    FmFolderView *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_FM_FOLDER_VIEW(object));

    self = FM_FOLDER_VIEW(object);
    if(self->folder)
    {
        g_object_unref(self->folder);
        if( self->model )
            g_object_unref(self->model);
    }
    g_object_unref(self->dnd_src);
    g_object_unref(self->dnd_dest);

    if(self->cwd)
        fm_path_unref(self->cwd);

    g_signal_handlers_disconnect_by_func(fm_config, on_single_click_changed, object);

    cancel_pending_row_activated(self);

    if(self->icon_size_changed_handler)
        g_signal_handler_disconnect(fm_config, self->icon_size_changed_handler);

    if (G_OBJECT_CLASS(fm_folder_view_parent_class)->finalize)
        (* G_OBJECT_CLASS(fm_folder_view_parent_class)->finalize)(object);
}


static void set_icon_size(FmFolderView* fv, guint icon_size)
{
    FmCellRendererPixbuf* render = (FmCellRendererPixbuf*)fv->renderer_pixbuf;
    fm_cell_renderer_pixbuf_set_fixed_size(render, icon_size, icon_size);

    if(!fv->model)
        return;

    fm_folder_model_set_icon_size(FM_FOLDER_MODEL(fv->model), icon_size);

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
    gboolean droppable = TRUE;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        {
            GtkTreeViewDropPosition pos;
            GtkTreeViewColumn* col;
            gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(fv->view), x, y, &x, &y);
            /* if(gtk_tree_view_get_dest_row_at_pos((GtkTreeView*)fv->view, x, y, &tp, NULL)) */
            if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(fv->view), x, y, &tp, &col, NULL, NULL))
            {
                if(gtk_tree_view_column_get_sort_column_id(col)!=COL_FILE_NAME)
                {
                    gtk_tree_path_free(tp);
                    tp = NULL;
                }
            }
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(fv->view), tp, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
            break;
        }
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        {
            tp = exo_icon_view_get_path_at_pos((const struct ExoIconView *)(const struct ExoIconView *)fv->view, x, y);
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
            if(gtk_tree_model_get_iter(fv->model, &it, tp))
            {
                FmFileInfo* fi;
                gtk_tree_model_get(fv->model, &it, COL_FILE_INFO, &fi, -1);
                fm_dnd_dest_set_dest_file(fv->dnd_dest, fi);
            }
            gtk_tree_path_free(tp);
        }
        else
        {
            /* FIXME: prevent direct access to data members. */
            FmFolderModel* model = (FmFolderModel*)fv->model;
            FmPath* dir_path =  model->dir->dir_path;
            fm_dnd_dest_set_dest_file(fv->dnd_dest, model->dir->dir_fi);
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
    GtkTreeViewColumn* col;
    GtkTreeSelection* ts;
    GList *l;
    GtkCellRenderer* render;
    FmFolderModel* model = (FmFolderModel*)fv->model;
    int icon_size = 0;

    fv->view = exo_icon_view_new();

    render = fm_cell_renderer_pixbuf_new();
    fv->renderer_pixbuf = render;

    g_object_set((GObject*)render, "follow-state", TRUE, NULL );
    gtk_cell_layout_pack_start((GtkCellLayout*)fv->view, render, TRUE);
    gtk_cell_layout_add_attribute((GtkCellLayout*)fv->view, render, "pixbuf", COL_FILE_ICON );
    gtk_cell_layout_add_attribute((GtkCellLayout*)fv->view, render, "info", COL_FILE_INFO );

    if(fv->mode == FM_FV_COMPACT_VIEW) /* compact view */
    {
        fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_small_icon_size_changed), fv);
        icon_size = fm_config->small_icon_size;
        fm_cell_renderer_pixbuf_set_fixed_size(FM_CELL_RENDERER_PIXBUF(fv->renderer_pixbuf), icon_size, icon_size);
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
            fm_cell_renderer_pixbuf_set_fixed_size(FM_CELL_RENDERER_PIXBUF(fv->renderer_pixbuf), icon_size, icon_size);
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
            fm_cell_renderer_pixbuf_set_fixed_size(FM_CELL_RENDERER_PIXBUF(fv->renderer_pixbuf), icon_size, icon_size);
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
    gtk_cell_layout_pack_start((GtkCellLayout*)fv->view, render, TRUE);
    gtk_cell_layout_add_attribute((GtkCellLayout*)fv->view, render,
                                "text", COL_FILE_NAME );
    exo_icon_view_set_item_width((ExoIconView*)fv->view, 96);
    exo_icon_view_set_search_column((ExoIconView*)fv->view, COL_FILE_NAME);
    g_signal_connect(fv->view, "item-activated", G_CALLBACK(on_icon_view_item_activated), fv);
    g_signal_connect(fv->view, "selection-changed", G_CALLBACK(on_sel_changed), fv);
    exo_icon_view_set_model((ExoIconView*)fv->view, fv->model);
    exo_icon_view_set_selection_mode((ExoIconView*)fv->view, fv->sel_mode);
    exo_icon_view_set_single_click((ExoIconView*)fv->view, fm_config->single_click);
    exo_icon_view_set_single_click_timeout((ExoIconView*)fv->view, SINGLE_CLICK_TIMEOUT);

    for(l = sels;l;l=l->next)
        exo_icon_view_select_path((ExoIconView*)fv->view, (GtkTreePath*)l->data);
}

static inline void create_list_view(FmFolderView* fv, GList* sels)
{
    GtkTreeViewColumn* col;
    GtkTreeSelection* ts;
    GList *l;
    GtkCellRenderer* render;
    FmFolderModel* model = (FmFolderModel*)fv->model;
    int icon_size = 0;
    fv->view = exo_tree_view_new();

    render = fm_cell_renderer_pixbuf_new();
    fv->renderer_pixbuf = render;
    fv->icon_size_changed_handler = g_signal_connect(fm_config, "changed::small_icon_size", G_CALLBACK(on_small_icon_size_changed), fv);
    icon_size = fm_config->small_icon_size;
    fm_cell_renderer_pixbuf_set_fixed_size(FM_CELL_RENDERER_PIXBUF(fv->renderer_pixbuf), icon_size, icon_size);
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
    gtk_tree_view_append_column((GtkTreeView*)fv->view, col);
    /* only this column is activable */
    exo_tree_view_set_activable_column((ExoTreeView*)fv->view, col);

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(_("Description"), render, "text", COL_FILE_DESC, NULL);
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_sort_column_id(col, COL_FILE_DESC);
    gtk_tree_view_append_column((GtkTreeView*)fv->view, col);

    render = gtk_cell_renderer_text_new();
    g_object_set(render, "xalign", 1.0, NULL);
    col = gtk_tree_view_column_new_with_attributes(_("Size"), render, "text", COL_FILE_SIZE, NULL);
    gtk_tree_view_column_set_sort_column_id(col, COL_FILE_SIZE);
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_append_column((GtkTreeView*)fv->view, col);

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(_("Modified"), render, "text", COL_FILE_MTIME, NULL);
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_sort_column_id(col, COL_FILE_MTIME);
    gtk_tree_view_append_column((GtkTreeView*)fv->view, col);

    gtk_tree_view_set_search_column((GtkTreeView*)fv->view, COL_FILE_NAME);

    gtk_tree_view_set_rubber_banding((GtkTreeView*)fv->view, TRUE);
    exo_tree_view_set_single_click((ExoTreeView*)fv->view, fm_config->single_click);
    exo_tree_view_set_single_click_timeout((ExoTreeView*)fv->view, SINGLE_CLICK_TIMEOUT);

    ts = gtk_tree_view_get_selection((GtkTreeView*)fv->view);
    g_signal_connect(fv->view, "row-activated", G_CALLBACK(on_tree_view_row_activated), fv);
    g_signal_connect(ts, "changed", G_CALLBACK(on_sel_changed), fv);
    /*cancel_pending_row_activated(fv);*/ /* FIXME: is this needed? */
    gtk_tree_view_set_model((GtkTreeView*)fv->view, fv->model);
    gtk_tree_selection_set_mode(ts, fv->sel_mode);
    for(l = sels;l;l=l->next)
        gtk_tree_selection_select_path(ts, (GtkTreePath*)l->data);
}

void fm_folder_view_set_mode(FmFolderView* fv, FmFolderViewMode mode)
{
    if( mode != fv->mode )
    {
        GtkTreeSelection* ts;
        GList *sels, *cells;
        FmFolderModel* model = (FmFolderModel*)fv->model;
        gboolean has_focus;

        if( G_LIKELY(fv->view) )
        {
            has_focus = GTK_WIDGET_HAS_FOCUS(fv->view);
            /* preserve old selections */
            sels = fm_folder_view_get_selected_tree_paths(fv);

            g_signal_handlers_disconnect_by_func(fv->view, on_drag_motion, fv);
            g_signal_handlers_disconnect_by_func(fv->view, on_drag_leave, fv);
            g_signal_handlers_disconnect_by_func(fv->view, on_drag_drop, fv);
            g_signal_handlers_disconnect_by_func(fv->view, on_drag_data_received, fv);

            fm_dnd_unset_dest_auto_scroll(fv->view);

            gtk_widget_destroy(fv->view );
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
        switch(fv->mode)
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

        fm_dnd_set_dest_auto_scroll(fv->view, gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(fv)), gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(fv)));

        gtk_widget_show(fv->view);
        gtk_container_add((GtkContainer*)fv, fv->view);

        if(has_focus) /* restore the focus if needed. */
            gtk_widget_grab_focus(fv->view);
    }
    else
    {
        /* g_debug("same mode"); */
    }
}

FmFolderViewMode fm_folder_view_get_mode(FmFolderView* fv)
{
    return fv->mode;
}

void fm_folder_view_set_selection_mode(FmFolderView* fv, GtkSelectionMode mode)
{
    if(fv->sel_mode != mode)
    {
        fv->sel_mode = mode;
        switch(fv->mode)
        {
        case FM_FV_LIST_VIEW:
        {
            GtkTreeSelection* sel = gtk_tree_view_get_selection((GtkTreeView*)fv->view);
            gtk_tree_selection_set_mode(sel, mode);
            break;
        }
        case FM_FV_ICON_VIEW:
        case FM_FV_COMPACT_VIEW:
        case FM_FV_THUMBNAIL_VIEW:
            exo_icon_view_set_selection_mode((ExoIconView*)fv->view, mode);
            break;
        }
    }
}

GtkSelectionMode fm_folder_view_get_selection_mode(FmFolderView* fv)
{
    return fv->sel_mode;
}

void fm_folder_view_sort(FmFolderView* fv, GtkSortType type, int by)
{
    /* (int) is needed here since enum seems to be treated as unsigned int so -1 becomes > 0 */
    if((int)type >=0)
        fv->sort_type = type;
    if(by >=0)
        fv->sort_by = by;
    if(fv->model)
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fv->model),
                                             fv->sort_by, fv->sort_type);
}

GtkSortType fm_folder_view_get_sort_type(FmFolderView* fv)
{
    return fv->sort_type;
}

int fm_folder_view_get_sort_by(FmFolderView* fv)
{
    return fv->sort_by;
}

void fm_folder_view_set_show_hidden(FmFolderView* fv, gboolean show)
{
    if(show != fv->show_hidden )
    {
        fv->show_hidden = show;
        if(G_LIKELY(fv->model))
            fm_folder_model_set_show_hidden(FM_FOLDER_MODEL(fv->model), show);
    }
}

gboolean fm_folder_view_get_show_hidden(FmFolderView* fv)
{
    return fv->show_hidden;
}

gboolean fm_folder_view_chdir_by_name(FmFolderView* fv, const char* path_str)
{
    gboolean ret;
    FmPath* path;

    if( G_UNLIKELY( !path_str ) )
        return FALSE;

    path = fm_path_new_for_str(path_str);
    if(!path) /* might be a malformed path */
        return FALSE;
    ret = fm_folder_view_chdir(fv, path);
    fm_path_unref(path);
    return ret;
}

static void on_folder_unmounted(FmFolder* folder, FmFolderView* fv)
{
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        cancel_pending_row_activated(fv);
        gtk_tree_view_set_model(GTK_TREE_VIEW(fv->view), NULL);
        break;
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        exo_icon_view_set_model(EXO_ICON_VIEW(fv->view), NULL);
        break;
    }
    if(fv->model)
    {
        g_signal_handlers_disconnect_by_func(fv->model, on_sort_col_changed, fv);
        g_object_unref(fv->model);
        fv->model = NULL;
    }
}

static void on_folder_loaded(FmFolder* folder, FmFolderView* fv)
{
    FmFolderModel* model;
    guint icon_size = 0;

    model = fm_folder_model_new(folder, fv->show_hidden);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), fv->sort_by, fv->sort_type);
    g_signal_connect(model, "sort-column-changed", G_CALLBACK(on_sort_col_changed), fv);

    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        cancel_pending_row_activated(fv);
        gtk_tree_view_set_model(GTK_TREE_VIEW(fv->view), model);
        icon_size = fm_config->small_icon_size;
        fm_folder_model_set_icon_size(model, icon_size);
        break;
    case FM_FV_ICON_VIEW:
        icon_size = fm_config->big_icon_size;
        fm_folder_model_set_icon_size(model, icon_size);
        exo_icon_view_set_model(EXO_ICON_VIEW(fv->view), model);
        break;
    case FM_FV_COMPACT_VIEW:
        icon_size = fm_config->small_icon_size;
        fm_folder_model_set_icon_size(model, icon_size);
        exo_icon_view_set_model(EXO_ICON_VIEW(fv->view), model);
        break;
    case FM_FV_THUMBNAIL_VIEW:
        icon_size = fm_config->thumbnail_size;
        fm_folder_model_set_icon_size(model, icon_size);
        exo_icon_view_set_model(EXO_ICON_VIEW(fv->view), model);
        break;
    }
    fv->model = model;
    on_model_loaded(model, fv);
}

gboolean fm_folder_view_chdir(FmFolderView* fv, FmPath* path)
{
    FmFolderModel* model;
    FmFolder* folder;

    if(fv->folder)
    {
        g_signal_handlers_disconnect_by_func(fv->folder, on_folder_loaded, fv);
        g_signal_handlers_disconnect_by_func(fv->folder, on_folder_unmounted, fv);
        g_signal_handlers_disconnect_by_func(fv->folder, on_folder_err, fv);
        g_object_unref(fv->folder);
        fv->folder = NULL;
        if(fv->model)
        {
            model = FM_FOLDER_MODEL(fv->model);
            g_signal_handlers_disconnect_by_func(model, on_sort_col_changed, fv);
            if(model->dir)
                g_signal_handlers_disconnect_by_func(model->dir, on_folder_err, fv);
            g_object_unref(model);
            fv->model = NULL;
        }
    }

    /* FIXME: the signal handler should be able to cancel the loading. */
    g_signal_emit(fv, signals[CHDIR], 0, path);
    if(fv->cwd)
        fm_path_unref(fv->cwd);
    fv->cwd = fm_path_ref(path);

    fv->folder = folder = fm_folder_get(path);
    if(folder)
    {
        /* connect error handler */
        g_signal_connect(folder, "loaded", on_folder_loaded, fv);
        g_signal_connect(folder, "unmount", on_folder_unmounted, fv);
        g_signal_connect(folder, "error", on_folder_err, fv);
        if(fm_folder_get_is_loaded(folder))
            on_folder_loaded(folder, fv);
        else
        {
            switch(fv->mode)
            {
            case FM_FV_LIST_VIEW:
                cancel_pending_row_activated(fv);
                gtk_tree_view_set_model(GTK_TREE_VIEW(fv->view), NULL);
                break;
            case FM_FV_ICON_VIEW:
            case FM_FV_COMPACT_VIEW:
            case FM_FV_THUMBNAIL_VIEW:
                exo_icon_view_set_model(EXO_ICON_VIEW(fv->view), NULL);
                break;
            }
            fv->model = NULL;
        }
    }
    return TRUE;
}

FmPath* fm_folder_view_get_cwd(FmFolderView* fv)
{
    return fv->cwd;
}

GList* fm_folder_view_get_selected_tree_paths(FmFolderView* fv)
{
    GList *sels = NULL;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
    {
        GtkTreeSelection* sel;
        sel = gtk_tree_view_get_selection((GtkTreeView*)fv->view);
        sels = gtk_tree_selection_get_selected_rows(sel, NULL);
        break;
    }
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        sels = exo_icon_view_get_selected_items((ExoIconView*)fv->view);
        break;
    }
    return sels;
}

FmFileInfoList* fm_folder_view_get_selected_files(FmFolderView* fv)
{
    FmFileInfoList* fis;
    GList *sels = fm_folder_view_get_selected_tree_paths(fv);
    GList *l, *next;
    if(!sels)
        return NULL;
    fis = fm_file_info_list_new();
    for(l = sels;l;l=next)
    {
        FmFileInfo* fi;
        GtkTreeIter it;
        GtkTreePath* tp = (GtkTreePath*)l->data;
        gtk_tree_model_get_iter(fv->model, &it, l->data);
        gtk_tree_model_get(fv->model, &it, COL_FILE_INFO, &fi, -1);
        gtk_tree_path_free(tp);
        next = l->next;
        l->data = fm_file_info_ref( fi );
        l->prev = l->next = NULL;
        fm_list_push_tail_link(fis, l);
    }
    return fis;
}

FmPathList* fm_folder_view_get_selected_file_paths(FmFolderView* fv)
{
    FmFileInfoList* files = fm_folder_view_get_selected_files(fv);
    FmPathList* list;
    if(files)
    {
        list = fm_path_list_new_from_file_info_list(files);
        fm_list_unref(files);
    }
    else
        list = NULL;
    return list;
}

void on_sel_changed(GObject* obj, FmFolderView* fv)
{
    /* FIXME: this is inefficient, but currently there is no better way */
    FmFileInfo* files = fm_folder_view_get_selected_files(fv);
    g_signal_emit(fv, signals[SEL_CHANGED], 0, files);
    if(files)
        fm_list_unref(files);
}

void on_sort_col_changed(GtkTreeSortable* sortable, FmFolderView* fv)
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

gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmFolderView* fv)
{
    GList* sels;
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
                        sels = exo_icon_view_get_selected_items((const struct ExoIconView *)(const struct ExoIconView *)view);
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

void fm_folder_view_select_all(FmFolderView* fv)
{
    GtkTreeSelection * tree_sel;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        tree_sel = gtk_tree_view_get_selection((GtkTreeView*)fv->view);
        gtk_tree_selection_select_all(tree_sel);
        break;
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        exo_icon_view_select_all((ExoIconView*)fv->view);
        break;
    }
}

void on_dnd_src_data_get(FmDndSrc* ds, FmFolderView* fv)
{
    FmFileInfoList* files = fm_folder_view_get_selected_files(fv);
    if(files)
    {
        fm_dnd_src_set_files(ds, files);
        fm_list_unref(files);
    }
}


void fm_folder_view_select_invert(FmFolderView* fv)
{
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        {
            GtkTreeSelection *tree_sel;
            GtkTreeIter it;
            if(!gtk_tree_model_get_iter_first(fv->model, &it))
                return;
            tree_sel = gtk_tree_view_get_selection((GtkTreeView*)fv->view);
            do
            {
                if(gtk_tree_selection_iter_is_selected(tree_sel, &it))
                    gtk_tree_selection_unselect_iter(tree_sel, &it);
                else
                    gtk_tree_selection_select_iter(tree_sel, &it);
            }while( gtk_tree_model_iter_next(fv->model, &it ));
            break;
        }
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
    case FM_FV_THUMBNAIL_VIEW:
        {
            GtkTreePath* path;
            int i, n;
            n = gtk_tree_model_iter_n_children(fv->model, NULL);
            if(n == 0)
                return;
            path = gtk_tree_path_new_first();
            for( i=0; i<n; ++i, gtk_tree_path_next(path) )
            {
                if ( exo_icon_view_path_is_selected((ExoIconView*)fv->view, path))
                    exo_icon_view_unselect_path((ExoIconView*)fv->view, path);
                else
                    exo_icon_view_select_path((ExoIconView*)fv->view, path);
            }
            break;
        }
    }
}

void fm_folder_view_select_file_path(FmFolderView* fv, FmPath* path)
{
    if(fm_path_equal(path->parent, fv->cwd))
    {
        FmFolderModel* model = (FmFolderModel*)fv->model;
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

void fm_folder_view_select_file_paths(FmFolderView* fv, FmPathList* paths)
{
    GList* l;
    for(l = fm_list_peek_head_link(paths);l; l=l->next)
    {
        FmPath* path = FM_PATH(l->data);
        fm_folder_view_select_file_path(fv, path);
    }
}

/* FIXME: select files by custom func, not yet implemented */
void fm_folder_view_custom_select(FmFolderView* fv, GFunc filter, gpointer user_data)
{

}

FmFileInfo* fm_folder_view_get_cwd_info(FmFolderView* fv)
{
    return FM_FOLDER_MODEL(fv->model)->dir->dir_fi;
}

gboolean fm_folder_view_get_is_loaded(FmFolderView* fv)
{
    return fv->folder && fm_folder_get_is_loaded(fv->folder);
}

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

FmFolderModel* fm_folder_view_get_model(FmFolderView* fv)
{
    return FM_FOLDER_MODEL(fv->model);
}

FmFolder* fm_folder_view_get_folder(FmFolderView* fv)
{
    return fv->folder;
}
