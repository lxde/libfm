//      fm-dir-tree-view.c
//
//      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//      Copyright 2012 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

/**
 * SECTION:fm-dir-tree-view
 * @short_description: A directory tree view widget.
 * @title: FmDirTreeView
 *
 * @include: libfm/fm-dir-tree-view.h
 *
 * The #FmDirTreeView represents view of filesystem as ierarchical tree
 * of folders where each node can be expanded or collapsed when required.
 */

#include "fm-dir-tree-view.h"
#include "fm-dir-tree-model.h"
#include "../gtk-compat.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>

enum
{
    CHDIR,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static void fm_dir_tree_view_dispose            (GObject *object);
static void on_row_loaded(FmDirTreeModel*, GtkTreePath*, FmDirTreeView*);

G_DEFINE_TYPE(FmDirTreeView, fm_dir_tree_view, GTK_TYPE_TREE_VIEW)

static void cancel_pending_chdir(GtkTreeModel* model, FmDirTreeView *view)
{
    if(view->paths_to_expand)
    {
        if(G_LIKELY(view->current_row)) /* FIXME: else data are corrupted */
        {
            g_signal_handlers_disconnect_by_func(model, on_row_loaded, view);
            gtk_tree_row_reference_free(view->current_row);
            view->current_row = NULL;
        }
        g_slist_foreach(view->paths_to_expand, (GFunc)fm_path_unref, NULL);
        g_slist_free(view->paths_to_expand);
        view->paths_to_expand = NULL;
    }
}

static gboolean on_test_expand_row(GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path)
{
    FmDirTreeModel* model = FM_DIR_TREE_MODEL(gtk_tree_view_get_model(tree_view));

    fm_dir_tree_model_load_row(model, iter, path);

    return FALSE;
}

static gboolean find_iter_by_path(GtkTreeModel* model, GtkTreeIter* it, GtkTreePath* tp, FmPath* path)
{
    GtkTreeIter parent;

    /* tp NULL means we need to find some root path */
    if(tp)
        gtk_tree_model_get_iter(model, &parent, tp);
    if(gtk_tree_model_iter_children(model, it, tp ? &parent : NULL))
    {
        do{
            FmPath* path2;
            path2 = fm_dir_tree_row_get_file_path(FM_DIR_TREE_MODEL(model), it);
            if(path2 && fm_path_equal(path, path2))
                return TRUE;
        }while(gtk_tree_model_iter_next(model, it));
    }
    return FALSE;
}

/*
static void on_row_expanded(GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path)
{
    FmDirTreeView* view = FM_DIR_TREE_VIEW(tree_view);
}
*/

static void on_row_collapsed(GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path)
{
    FmDirTreeModel* model = FM_DIR_TREE_MODEL(gtk_tree_view_get_model(tree_view));
    fm_dir_tree_model_unload_row(model, iter, path);
}

static void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *col)
{
    if(gtk_tree_view_row_expanded(tree_view, path))
        gtk_tree_view_collapse_row(tree_view, path);
    else
        gtk_tree_view_expand_row(tree_view, path, FALSE);
}

static gboolean on_key_press_event(GtkWidget* widget, GdkEventKey* evt)
{
    GtkTreeView* tree_view = GTK_TREE_VIEW(widget);
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model;
    GtkTreeIter it;
    GtkTreePath* tp;

    switch(evt->keyval)
    {
    case GDK_KEY_Left:
        tree_sel = gtk_tree_view_get_selection(tree_view);
        if(gtk_tree_selection_get_selected(tree_sel, &model, &it))
        {
            tp = gtk_tree_model_get_path(model, &it);
            if(gtk_tree_view_row_expanded(tree_view, tp))
                gtk_tree_view_collapse_row(tree_view, tp);
            else
            {
                gtk_tree_path_up(tp);
                gtk_tree_view_set_cursor(tree_view, tp, NULL, FALSE);
                gtk_tree_selection_select_path(tree_sel, tp);
            }
            gtk_tree_path_free(tp);
        }

        break;
    case GDK_KEY_Right:
        tree_sel = gtk_tree_view_get_selection(tree_view);
        if(gtk_tree_selection_get_selected(tree_sel, &model, &it))
        {
            tp = gtk_tree_model_get_path(model, &it);
            gtk_tree_view_expand_row(tree_view, tp, FALSE);
            gtk_tree_path_free(tp);
        }
        break;
    }
    return GTK_WIDGET_CLASS(fm_dir_tree_view_parent_class)->key_press_event(widget, evt);
}

static void fm_dir_tree_view_class_init(FmDirTreeViewClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GtkTreeViewClass* tree_view_class = GTK_TREE_VIEW_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_object_class->dispose = fm_dir_tree_view_dispose;
    /* use finalize from parent class */

    widget_class->key_press_event = on_key_press_event;
    // widget_class->button_press_event = on_button_press_event;

    tree_view_class->test_expand_row = on_test_expand_row;
    tree_view_class->row_collapsed = on_row_collapsed;
    /* tree_view_class->row_expanded = on_row_expanded; */
    tree_view_class->row_activated = on_row_activated;

    /**
     * FmDirTreeView::chdir:
     * @view: a view instance that emitted the signal
     * @button: always is 1
     * @path: (#FmPath *) new directory path
     *
     * The #FmDirTreeView::chdir signal is emitted when current selected
     * directory in view is changed.
     *
     * Since: 0.1.0
     */
    signals[CHDIR] =
        g_signal_new("chdir",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(FmDirTreeViewClass, chdir),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__UINT_POINTER,
                     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
}

static void emit_chdir_if_needed(FmDirTreeView* view, GtkTreeSelection* tree_sel, guint button)
{
    GtkTreeIter it;
    GtkTreeModel* model;
    if(gtk_tree_selection_get_selected(tree_sel, &model, &it))
    {
        FmPath* path = fm_dir_tree_row_get_file_path(FM_DIR_TREE_MODEL(model), &it);
        if(path && view->cwd && fm_path_equal(path, view->cwd))
            return;
        if(view->cwd)
            fm_path_unref(view->cwd);
        view->cwd = G_LIKELY(path) ? fm_path_ref(path) : NULL;
        g_signal_emit(view, signals[CHDIR], 0, button, view->cwd);
    }
}

static void on_sel_changed(GtkTreeSelection* tree_sel, FmDirTreeView* view)
{
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));

    /* if a pending selection via previous call to chdir is in progress, cancel it. */
    cancel_pending_chdir(model, view);

    emit_chdir_if_needed(view, tree_sel, 1);
}

static void fm_dir_tree_view_dispose(GObject *object)
{
    FmDirTreeView *view;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_DIR_TREE_VIEW(object));

    view = (FmDirTreeView*)object;
    if(G_UNLIKELY(view->paths_to_expand))
    {
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        cancel_pending_chdir(model, view);
    }
    if(view->current_row) /* this shouldn't be done if there are paths_to_expand */
    {
        gtk_tree_row_reference_free(view->current_row);
        view->current_row = NULL;
    }
    if(view->cwd)
    {
        fm_path_unref(view->cwd);
        view->cwd = NULL;
    }

    G_OBJECT_CLASS(fm_dir_tree_view_parent_class)->dispose(object);
}

static gboolean _fm_dir_tree_view_select_function(GtkTreeSelection *selection,
                                                  GtkTreeModel *model,
                                                  GtkTreePath *path,
                                                  gboolean path_currently_selected,
                                                  gpointer data)
{
    GtkTreeIter it;

    if(!gtk_tree_model_get_iter(model, &it, path))
        return FALSE;
    return (fm_dir_tree_row_get_file_info(FM_DIR_TREE_MODEL(model), &it) != NULL);
}

static void fm_dir_tree_view_init(FmDirTreeView *view)
{
    GtkTreeSelection* tree_sel;
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
    /* gtk_tree_view_set_enable_tree_lines(view, TRUE); */

    col = gtk_tree_view_column_new();
    render = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_set_attributes(col, render, "pixbuf", FM_DIR_TREE_MODEL_COL_ICON, NULL);

    render = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_set_attributes(col, render, "text", FM_DIR_TREE_MODEL_COL_DISP_NAME, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_set_mode(tree_sel, GTK_SELECTION_BROWSE);
    gtk_tree_selection_set_select_function(tree_sel,
                        _fm_dir_tree_view_select_function, view, NULL);
    g_signal_connect(tree_sel, "changed", G_CALLBACK(on_sel_changed), view);
}

/**
 * fm_dir_tree_view_new
 *
 * Creates new #FmDirTreeView widget.
 *
 * Returns: a new #FmDirTreeView object.
 *
 * Since: 0.1.0
 */
FmDirTreeView *fm_dir_tree_view_new(void)
{
    return g_object_new(FM_TYPE_DIR_TREE_VIEW, NULL);
}

/**
 * fm_dir_tree_view_get_cwd
 * @view: the widget to retrieve info
 *
 * Retrieves current selected directory. Returned data are owned by @view
 * and should not be freed by caller.
 *
 * Returns: (transfer none): current directory path.
 *
 * Since: 0.1.0
 */
FmPath* fm_dir_tree_view_get_cwd(FmDirTreeView* view)
{
    return view->cwd;
}

static void expand_pending_path(FmDirTreeView* view, GtkTreeModel* model, GtkTreePath* tp)
{
    FmPath* path;
    GtkTreeIter it;
    g_return_if_fail(view->paths_to_expand);
    path = FM_PATH(view->paths_to_expand->data);

    if(find_iter_by_path(model, &it, tp, path))
    {
        gtk_tree_row_reference_free(view->current_row);

        /* after being expanded, the row now owns a FmFolder object. */
        g_signal_connect(model, "row-loaded", G_CALLBACK(on_row_loaded), view);

        tp = gtk_tree_model_get_path(model, &it); /* it now points to the item */
        view->current_row = gtk_tree_row_reference_new(model, tp);
        /* the path may be already expanded so "row-loaded" may never happen */
        if(!fm_dir_tree_row_is_loaded(FM_DIR_TREE_MODEL(model), &it))
            fm_dir_tree_model_load_row(FM_DIR_TREE_MODEL(model), &it, tp);
        else
            on_row_loaded(FM_DIR_TREE_MODEL(model), tp, view); /* recursion! */
        gtk_tree_path_free(tp);
    }
    /* FIXME: otherwise it's a bug */
}

static void on_row_loaded(FmDirTreeModel* fm_model, GtkTreePath* tp, FmDirTreeView* view)
{
    GtkTreeModel* model = GTK_TREE_MODEL(fm_model);
    GtkTreePath* ctp;

    g_return_if_fail(view->current_row);
    ctp = gtk_tree_row_reference_get_path(view->current_row);
    if(gtk_tree_path_compare(tp, ctp) != 0)
    {
        /* is it delayed previous expand? */
        gtk_tree_path_free(ctp);
        return;
    }
    gtk_tree_path_free(ctp);

    /* disconnect the handler since we only need it once */
    g_signal_handlers_disconnect_by_func(model, on_row_loaded, view);

    /* after the folder is loaded, the files should have been added to
     * the tree model */
    gtk_tree_view_expand_row(GTK_TREE_VIEW(view), tp, FALSE);

    /* remove the expanded path from pending list */
    fm_path_unref(FM_PATH(view->paths_to_expand->data));
    view->paths_to_expand = g_slist_delete_link(view->paths_to_expand, view->paths_to_expand);

    if(view->paths_to_expand)
    {
        /* continue expanding next pending path */
        expand_pending_path(view, model, tp);
    }
    else /* this is the last one and we're done, select the item */
    {
        GtkTreeSelection* ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
        gtk_tree_selection_select_path(ts, tp);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(view), tp, NULL, TRUE, 0.5, 0.5);
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), tp, NULL, FALSE);
    }
}

/**
 * fm_dir_tree_view_chdir
 * @view: the widget to change
 * @path: new directory
 *
 * Queries change selected directory in the @view to new @path. The
 * widget will expand nodes in the tree if that will be needed to
 * reach requested path.
 *
 * Since: 0.1.0
 */
void fm_dir_tree_view_chdir(FmDirTreeView* view, FmPath* path)
{
    GtkTreeIter it;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    FmPath* root;
    if(!model || fm_path_equal(view->cwd, path))
        return;
    if(!gtk_tree_model_get_iter_first(model, &it))
        return;

    /* find a root item containing this path */
    do{
        root = fm_dir_tree_row_get_file_path(FM_DIR_TREE_MODEL(model), &it);
        if(fm_path_has_prefix(path, root))
            break;
        root = NULL;
    }while(gtk_tree_model_iter_next(model, &it));
    /* root item is found */

    /* cancel previous pending tree expansion */
    cancel_pending_chdir(model, view);

    do { /* add path elements one by one to a list */
        view->paths_to_expand = g_slist_prepend(view->paths_to_expand, fm_path_ref(path));
        if(fm_path_equal(path, root))
            break;
        path = path->parent;
    }while(path);

    expand_pending_path(view, model, NULL);
}
