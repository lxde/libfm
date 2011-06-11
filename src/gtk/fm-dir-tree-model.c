//      fm-dir-tree-model.c
//
//      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-dir-tree-model.h"
#include "fm-folder.h"
#include "fm-icon-pixbuf.h"

#include <glib/gi18n-lib.h>
#include <string.h>

typedef struct _FmDirTreeItem FmDirTreeItem;
struct _FmDirTreeItem
{
    FmDirTreeModel* model; /* FIXME: storing model pointer in every item is a waste */
    FmFileInfo* fi;
    FmFolder* folder;
    GdkPixbuf* icon;
    guint n_expand;
    GList* parent; /* parent node */
    GList* children; /* child items */
    GList* hidden_children;
};

static GType column_types[N_FM_DIR_TREE_MODEL_COLS];

static void fm_dir_tree_model_finalize            (GObject *object);
static void fm_dir_tree_model_tree_model_init(GtkTreeModelIface *iface);
static GtkTreePath *fm_dir_tree_model_get_path ( GtkTreeModel *tree_model, GtkTreeIter *iter );

G_DEFINE_TYPE_WITH_CODE( FmDirTreeModel, fm_dir_tree_model, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, fm_dir_tree_model_tree_model_init)
                        /* G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_DRAG_SOURCE, fm_dir_tree_model_drag_source_init)
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_DRAG_DEST, fm_dir_tree_model_drag_dest_init) */
                        )

/* a varient of g_list_foreach which does the same thing, but pass GList* element
 * itself as the first parameter to func(), not the element data. */
static inline void _g_list_foreach_l(GList* list, GFunc func, gpointer user_data)
{
    while (list)
    {
        GList *next = list->next;
        (*func)(list, user_data);
        list = next;
    }
}

/*
static void item_queue_subdir_check(FmDirTreeModel* model, GList* item_l);
*/

static void item_reload_icon(FmDirTreeModel* model, FmDirTreeItem* item, GtkTreePath* tp)
{
    GtkTreeIter it;
    GList* l;
    FmDirTreeItem* child;

    if(item->icon)
    {
        g_object_unref(item->icon);
        item->icon = NULL;
        gtk_tree_model_row_changed(GTK_TREE_MODEL(model), tp, &it);
    }

    if(item->children)
    {
        gtk_tree_path_append_index(tp, 0);
        for(l = item->children; l; l=l->next)
        {
            child = (FmDirTreeItem*)l->data;
            item_reload_icon(model, l, tp);
            gtk_tree_path_next(tp);
        }
        gtk_tree_path_up(tp);
    }

    for(l = item->hidden_children; l; l=l->next)
    {
        child = (FmDirTreeItem*)l->data;
        if(child->icon)
        {
            g_object_unref(child->icon);
            child->icon = NULL;
        }
    }
}

static void fm_dir_tree_model_class_init(FmDirTreeModelClass *klass)
{
    GObjectClass *g_object_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_dir_tree_model_finalize;
}


static inline FmDirTreeItem* fm_dir_tree_item_new(FmDirTreeModel* model, GList* parent_l)
{
    FmDirTreeItem* item = g_slice_new0(FmDirTreeItem);
    item->model = model;
    item->parent = parent_l;
    return item;
}

static inline void item_free_folder(GList* item_l);
static void fm_dir_tree_item_free_l(GList* item_l);

/* Most of time fm_dir_tree_item_free_l() should be called instead. */
static inline void fm_dir_tree_item_free(FmDirTreeItem* item)
{
    if(item->fi)
        fm_file_info_unref(item->fi);
    if(item->icon)
        g_object_unref(item->icon);

    if(item->folder) /* most of cases this should have been freed in item_free_folder() */
        g_object_unref(item->folder);

    if(item->children)
    {
        _g_list_foreach_l(item->children, (GFunc)fm_dir_tree_item_free_l, NULL);
        g_list_free(item->children);
    }
    if(item->hidden_children)
    {
        g_list_foreach(item->hidden_children, (GFunc)fm_dir_tree_item_free, NULL);
        g_list_free(item->hidden_children);
    }
    g_slice_free(FmDirTreeItem, item);
}

/* Free the GList* element along with its associated FmDirTreeItem */
static void fm_dir_tree_item_free_l(GList* item_l)
{
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    item_free_folder(item_l);
    fm_dir_tree_item_free(item);
}

static inline void item_to_tree_iter(FmDirTreeModel* model, GList* item_l, GtkTreeIter* it)
{
    it->stamp = model->stamp;
    /* We simply store a GList pointer in the iter */
    it->user_data = item_l;
    it->user_data2 = it->user_data3 = NULL;
}

static inline GtkTreePath* item_to_tree_path(FmDirTreeModel* model, GList* item_l)
{
    GtkTreeIter it;
    item_to_tree_iter(model, item_l, &it);
    return fm_dir_tree_model_get_path(model, &it);
}

static void on_theme_changed(GtkIconTheme* theme, FmDirTreeModel* model)
{
    GList* l;
    GtkTreePath* tp = gtk_tree_path_new_first();
    for(l = model->roots; l; l=l->next)
    {
        item_reload_icon(model, l, tp);
        gtk_tree_path_next(tp);
    }
    gtk_tree_path_free(tp);
}

static void fm_dir_tree_model_finalize(GObject *object)
{
    FmDirTreeModel *model;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_DIR_TREE_MODEL(object));

    model = FM_DIR_TREE_MODEL(object);

    g_signal_handlers_disconnect_by_func(gtk_icon_theme_get_default(),
                                         on_theme_changed, model);

    _g_list_foreach_l(model->roots, (GFunc)fm_dir_tree_item_free_l, NULL);
    g_list_free(model->roots);

    /* TODO: g_object_unref(model->subdir_cancellable); */

    G_OBJECT_CLASS(fm_dir_tree_model_parent_class)->finalize(object);
}

static void fm_dir_tree_model_init(FmDirTreeModel *model)
{
    g_signal_connect(gtk_icon_theme_get_default(), "changed",
                     G_CALLBACK(on_theme_changed), model);
    model->icon_size = 16;
    model->stamp = g_random_int();
    /* TODO:
    g_queue_init(&model->subdir_checks);
    model->subdir_checks_mutex = g_mutex_new();
    model->subdir_cancellable = g_cancellable_new();
    */
}

static GtkTreeModelFlags fm_dir_tree_model_get_flags (GtkTreeModel *tree_model)
{
    return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint fm_dir_tree_model_get_n_columns(GtkTreeModel *tree_model)
{
    return N_FM_DIR_TREE_MODEL_COLS;
}

static GType fm_dir_tree_model_get_column_type(GtkTreeModel *tree_model, gint index)
{
    g_return_val_if_fail( index < G_N_ELEMENTS(column_types) && index >= 0, G_TYPE_INVALID );
    return column_types[index];
}

static gboolean fm_dir_tree_model_get_iter(GtkTreeModel *tree_model,
                                    GtkTreeIter *iter,
                                    GtkTreePath *path )
{
    FmDirTreeModel *model;
    gint *indices, i, depth;
    GList *children, *child;

    g_assert(FM_IS_DIR_TREE_MODEL(tree_model));
    g_assert(path!=NULL);

    model = FM_DIR_TREE_MODEL(tree_model);
    if( G_UNLIKELY(!model || !model->roots) )
        return FALSE;

    indices = gtk_tree_path_get_indices(path);
    depth   = gtk_tree_path_get_depth(path);

    children = model->roots;
    for( i = 0; i < depth; ++i )
    {
        FmDirTreeItem* item;
        child = g_list_nth(children, indices[i]);
        if( !child )
            return FALSE;
        item = (FmDirTreeItem*)child->data;
        children = item->children;
    }
    item_to_tree_iter(model, child, iter);
    return TRUE;
}

static GtkTreePath *fm_dir_tree_model_get_path(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter )
{
    GList* item_l;
    GList* children;
    FmDirTreeItem* item;
    GtkTreePath* path;
    int i;
    FmDirTreeModel* model = FM_DIR_TREE_MODEL(tree_model);

    g_return_val_if_fail (model, NULL);
    g_return_val_if_fail (iter->stamp == model->stamp, NULL);
    g_return_val_if_fail (iter != NULL, NULL);
    g_return_val_if_fail (iter->user_data != NULL, NULL);

    item_l = (GList*)iter->user_data;
    item = (FmDirTreeItem*)item_l->data;

    if(item->parent == NULL) /* root item */
    {
        i = g_list_position(model->roots, item_l);
        path = gtk_tree_path_new_first();
        gtk_tree_path_get_indices(path)[0] = i;
    }
    else
    {
        path = gtk_tree_path_new();
        do
        {
            FmDirTreeItem* parent_item = (FmDirTreeItem*)item->parent->data;
            children = parent_item->children;
            i = g_list_position(children, item_l);
            if(G_UNLIKELY(i == -1)) /* bug? the item is not a child of its parent? */
            {
                gtk_tree_path_free(path);
                return NULL;
            }
            /* FIXME: gtk_tree_path_prepend_index() is inefficient */
            gtk_tree_path_prepend_index(path, i);
            item_l = item->parent; /* go up a level */
            item = (FmDirTreeItem*)item_l->data;
        }while(G_UNLIKELY(item->parent)); /* we're not at toplevel yet */

        /* we have reached toplevel */
        children = model->roots;
        i = g_list_position(children, item_l);
        gtk_tree_path_prepend_index(path, i);
    }
    return path;
}

static void fm_dir_tree_model_get_value ( GtkTreeModel *tree_model,
                              GtkTreeIter *iter,
                              gint column,
                              GValue *value )
{
    FmDirTreeModel* model = FM_DIR_TREE_MODEL(tree_model);
    GList* item_l;
    FmDirTreeItem* item;

    g_return_if_fail (iter->stamp == model->stamp);

    g_value_init (value, column_types[column] );
    item_l = (GList*)iter->user_data;
    item = (FmDirTreeItem*)item_l->data;

    switch(column)
    {
    case FM_DIR_TREE_MODEL_COL_ICON:
        if(item->fi && item->fi->icon)
        {
            if(!item->icon)
                item->icon = fm_icon_get_pixbuf(item->fi->icon, model->icon_size);
            g_value_set_object(value, item->icon);
        }
        else
            g_value_set_object(value, NULL);
        break;
    case FM_DIR_TREE_MODEL_COL_DISP_NAME:
        if(item->fi)
            g_value_set_string( value, fm_file_info_get_disp_name(item->fi));
        else /* this is a place holder item */
        {
            /* parent is always non NULL. otherwise it's a bug. */
            FmDirTreeItem* parent = (FmDirTreeItem*)item->parent->data;
            if(parent->folder && fm_folder_get_is_loaded(parent->folder))
                g_value_set_string( value, _("<No Sub Folder>"));
            else
                g_value_set_string( value, _("Loading..."));
        }
        break;
    case FM_DIR_TREE_MODEL_COL_INFO:
        g_value_set_pointer( value, item->fi);
        break;
    case FM_DIR_TREE_MODEL_COL_PATH:
        g_value_set_pointer( value, item->fi ? item->fi->path : NULL);
        break;
    case FM_DIR_TREE_MODEL_COL_FOLDER:
        g_value_set_pointer( value, item->folder);
        break;
    }
}

static gboolean fm_dir_tree_model_iter_next(GtkTreeModel *tree_model,
                                            GtkTreeIter *iter)
{
    FmDirTreeModel* model;
    GList* item_l;
    g_return_val_if_fail (FM_IS_DIR_TREE_MODEL (tree_model), FALSE);
    if (iter == NULL || iter->user_data == NULL)
        return FALSE;

    item_l = (GList*)iter->user_data;
    /* Is this the last child in the parent node? */
    if(!item_l->next)
        return FALSE;

    model = FM_DIR_TREE_MODEL(tree_model);
    item_to_tree_iter(model, item_l->next, iter);
    return TRUE;
}

static gboolean fm_dir_tree_model_iter_children(GtkTreeModel *tree_model,
                                                GtkTreeIter *iter,
                                                GtkTreeIter *parent)
{
    FmDirTreeModel* model;
    GList *first_child;

    g_return_val_if_fail(parent == NULL || parent->user_data != NULL, FALSE);
    g_return_val_if_fail(FM_IS_DIR_TREE_MODEL(tree_model), FALSE);
    model = FM_DIR_TREE_MODEL(tree_model);

    if(parent)
    {
        GList* parent_l = (GList*)parent->user_data;
        FmDirTreeItem *parent_item = (FmDirTreeItem*)parent_l->data;
        first_child = parent_item->children;
    }
    else /* toplevel item */
    {
        /* parent == NULL is a special case; we need to return the first top-level row */
        first_child = model->roots;
    }
    if(!first_child)
        return FALSE;

    /* Set iter to first item in model */
    item_to_tree_iter(model, first_child, iter);
    return TRUE;
}

static gboolean fm_dir_tree_model_iter_has_child(GtkTreeModel *tree_model,
                                                 GtkTreeIter *iter)
{
    GList* item_l;
    FmDirTreeItem* item;
    /* FIXME: is NULL iter allowed here? */
    g_return_val_if_fail( iter != NULL, FALSE );
    g_return_val_if_fail( iter->stamp == FM_DIR_TREE_MODEL(tree_model)->stamp, FALSE );

    item_l = (GList*)iter->user_data;
    item = (FmDirTreeItem*)item_l->data;
    return (item->children != NULL);
}

static gint fm_dir_tree_model_iter_n_children(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter)
{
    FmDirTreeModel* model;
    GList* children;
    g_return_val_if_fail(FM_IS_DIR_TREE_MODEL(tree_model), -1);

    model = FM_DIR_TREE_MODEL(tree_model);
    /* special case: if iter == NULL, return number of top-level rows */
    if(!iter)
        children = model->roots;
    else
    {
        GList* item_l = (GList*)iter->user_data;
        FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
        children = item->children;
    }
    return g_list_length(children);
}

static gboolean fm_dir_tree_model_iter_nth_child(GtkTreeModel *tree_model,
                                                 GtkTreeIter *iter,
                                                 GtkTreeIter *parent,
                                                 gint n)
{
    FmDirTreeModel *model;
    GList* children;
    GList *child_l;

    g_return_val_if_fail (FM_IS_DIR_TREE_MODEL (tree_model), FALSE);
    model = FM_DIR_TREE_MODEL(tree_model);

    if(G_LIKELY(parent))
    {
        GList* parent_l = (GList*)parent->user_data;
        FmDirTreeItem* item = (FmDirTreeItem*)parent_l->data;
        children = item->children;
    }
    else /* special case: if parent == NULL, set iter to n-th top-level row */
        children = model->roots;
    child_l = g_list_nth(children, n);
    if(!child_l)
        return FALSE;

    item_to_tree_iter(model, child_l, iter);
    return TRUE;
}

static gboolean fm_dir_tree_model_iter_parent(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter,
                                              GtkTreeIter *child)
{
    GList* child_l;
    FmDirTreeItem* child_item;
    FmDirTreeModel* model;
    g_return_val_if_fail( iter != NULL && child != NULL, FALSE );

    model = FM_DIR_TREE_MODEL( tree_model );
    child_l = (GList*)child->user_data;
    child_item = (FmDirTreeItem*)child_l->data;

    if(G_LIKELY(child_item->parent))
    {
        item_to_tree_iter(model, child_item->parent, iter);
        return TRUE;
    }
    return FALSE;
}

static void fm_dir_tree_model_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = fm_dir_tree_model_get_flags;
    iface->get_n_columns = fm_dir_tree_model_get_n_columns;
    iface->get_column_type = fm_dir_tree_model_get_column_type;
    iface->get_iter = fm_dir_tree_model_get_iter;
    iface->get_path = fm_dir_tree_model_get_path;
    iface->get_value = fm_dir_tree_model_get_value;
    iface->iter_next = fm_dir_tree_model_iter_next;
    iface->iter_children = fm_dir_tree_model_iter_children;
    iface->iter_has_child = fm_dir_tree_model_iter_has_child;
    iface->iter_n_children = fm_dir_tree_model_iter_n_children;
    iface->iter_nth_child = fm_dir_tree_model_iter_nth_child;
    iface->iter_parent = fm_dir_tree_model_iter_parent;

    column_types[FM_DIR_TREE_MODEL_COL_ICON] = GDK_TYPE_PIXBUF;
    column_types[FM_DIR_TREE_MODEL_COL_DISP_NAME] = G_TYPE_STRING;
    column_types[FM_DIR_TREE_MODEL_COL_INFO] = G_TYPE_POINTER;
    column_types[FM_DIR_TREE_MODEL_COL_PATH] = G_TYPE_POINTER;
    column_types[FM_DIR_TREE_MODEL_COL_FOLDER] = G_TYPE_POINTER;
}

FmDirTreeModel *fm_dir_tree_model_new(void)
{
    return (FmDirTreeModel*)g_object_new(FM_TYPE_DIR_TREE_MODEL, NULL);
}

static void add_place_holder_child_item(FmDirTreeModel* model, GList* parent_l, GtkTreePath* tp, gboolean emit_signal)
{
    FmDirTreeItem* parent_item = (FmDirTreeItem*)parent_l->data;
    FmDirTreeItem* item = fm_dir_tree_item_new(model, parent_l);
    parent_item->children = g_list_prepend(parent_item->children, item);

    if(emit_signal)
    {
        GtkTreeIter it;
        item_to_tree_iter(model, parent_item->children, &it);
        gtk_tree_path_append_index(tp, 0);
        gtk_tree_model_row_inserted(GTK_TREE_MODEL(model), tp, &it);
        gtk_tree_path_up(tp);
    }
}

/* Add a new node to parent node to proper position.
 * GtkTreePath tp is the tree path of parent node.
 * Note that value of tp will be changed inside the function temporarily
 * to generate GtkTreePath for child nodes, and then restored to its
 * original value before returning from the function. */
static GList* insert_item(FmDirTreeModel* model, GList* parent_l, GtkTreePath* tp, FmDirTreeItem* new_item)
{
    GList* new_item_l;
    FmDirTreeItem* parent_item = (FmDirTreeItem*)parent_l->data;
    const char* new_key = fm_file_info_get_collate_key(new_item->fi);
    GList* item_l, *last_l;
    GtkTreeIter it;
    int n = 0;
    for(item_l = parent_item->children; item_l; item_l=item_l->next, ++n)
    {
        FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
        const char* key;
        last_l = item_l;
        if( G_UNLIKELY(!item->fi) )
            continue;
        key = fm_file_info_get_collate_key(item->fi);
        if(strcmp(new_key, key) <= 0)
            break;
    }

    parent_item->children = g_list_insert_before(parent_item->children, item_l, new_item);
    /* get the GList* of the newly inserted item */
    if(!item_l) /* the new item becomes the last item of the list */
        new_item_l = last_l ? last_l->next : parent_item->children;
    else /* the new item is just previous item of its sibling. */
        new_item_l = item_l->prev;

    g_assert( new_item->fi != NULL );
    g_assert( new_item == new_item_l->data );
    g_assert( ((FmDirTreeItem*)new_item_l->data)->fi != NULL );

    /* emit row-inserted signal for the new item */
    item_to_tree_iter(model, new_item_l, &it);
    gtk_tree_path_append_index(tp, n);
    gtk_tree_model_row_inserted(model, tp, &it);

    /* add a placeholder child item to make the node expandable */
    add_place_holder_child_item(model, new_item_l, tp, TRUE);
    gtk_tree_path_up(tp);

    /* TODO: check if the dir has subdirs and make it expandable if needed. */
    /* item_queue_subdir_check(model, new_item_l); */

    return new_item_l;
}

/* Add file info to parent node to proper position.
 * GtkTreePath tp is the tree path of parent node.
 * Note that value of tp will be changed inside the function temporarily
 * to generate GtkTreePath for child nodes, and then restored to its
 * original value before returning from the function. */
static GList* insert_file_info(FmDirTreeModel* model, GList* parent_l, GtkTreePath* tp, FmFileInfo* fi)
{
    GtkTreeIter it;
    GList* item_l;
    FmDirTreeItem* parent_item = (FmDirTreeItem*)parent_l->data;
    FmDirTreeItem* item = fm_dir_tree_item_new(model, parent_l);
    item->fi = fm_file_info_ref(fi);

    /* fm_file_info_is_hidden() is slower here*/
    if(!model->show_hidden && fi->path->name[0] == '.') /* hidden folder */
    {
        parent_item->hidden_children = g_list_prepend(parent_item->hidden_children, item);
        item_l = parent_item->hidden_children;
    }
    else
        item_l = insert_item(model, parent_l, tp, item);
    return item_l;
}

static void remove_item(FmDirTreeModel* model, GList* item_l)
{
    GtkTreePath* tp = item_to_tree_path(model, item_l);
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    FmDirTreeItem* parent_item = (FmDirTreeItem*)item->parent ? item->parent->data : NULL;
    fm_dir_tree_item_free_l(item_l);
    if(parent_item)
        parent_item->children = g_list_delete_link(parent_item->children, item_l);
    /* signal the view that we removed the placeholder item. */
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(model), tp);
    gtk_tree_path_free(tp);
}

/* find child item by filename, and retrive its index if idx is not NULL. */
static GList* children_by_name(FmDirTreeModel* model, GList* children, const char* name, int* idx)
{
    GList* l;
    int i = 0;
    for(l = children; l; l=l->next, ++i)
    {
        FmDirTreeItem* item = (FmDirTreeItem*)l->data;
        if(G_LIKELY(item->fi) &&
           G_UNLIKELY(strcmp(item->fi->path->name, name) == 0))
        {
            if(idx)
                *idx = i;
            return l;
        }
    }
    return NULL;
}

static void remove_all_children(FmDirTreeModel* model, GList* item_l, GtkTreePath* tp)
{
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    if(G_UNLIKELY(!item->children))
        return;
    gtk_tree_path_append_index(tp, 0);
    /* FIXME: How to improve performance?
     * TODO: study the horrible source code of GtkTreeView */
    while(item->children)
    {
        fm_dir_tree_item_free_l(item->children);
        item->children = g_list_delete_link(item->children, item->children);
        /* signal the view that we removed the placeholder item. */
        gtk_tree_model_row_deleted(GTK_TREE_MODEL(model), tp);
        /* everytime we remove the first item, its next item became the
         * first item, so there is no need to update tp. */
    }

    if(item->hidden_children)
    {
        g_list_foreach(item->hidden_children, (GFunc)fm_dir_tree_item_free, NULL);
        g_list_free(item->hidden_children);
        item->hidden_children = NULL;
    }
    gtk_tree_path_up(tp);
}

void fm_dir_tree_model_add_root(FmDirTreeModel* model, FmFileInfo* root, GtkTreeIter* iter)
{
    GtkTreeIter it;
    GtkTreePath* tp;
    GList* item_l;
    FmDirTreeItem* item = fm_dir_tree_item_new(model, NULL);
    item->fi = fm_file_info_ref(root);
    model->roots = g_list_append(model->roots, item);
    item_l = g_list_last(model->roots); /* FIXME: this is inefficient */
    add_place_holder_child_item(model, item_l, NULL, FALSE);

    /* emit row-inserted signal for the new root item */
    item_to_tree_iter(model, item_l, &it);
    tp = item_to_tree_path(model, item_l);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(model), tp, &it);

    if(iter)
        *iter = it;
    gtk_tree_path_free(tp);
}

static void on_folder_loaded(FmFolder* folder, GList* item_l)
{
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    FmDirTreeModel* model = item->model;
    GList* place_holder_l;

    place_holder_l = item->children;
    if(item->children->next) /* if we have loaded sub dirs, remove the place holder */
    {
        /* remove the fake placeholder item showing "Loading..." */
        remove_item(model, place_holder_l);
    }
    else /* if we have no sub dirs, leave the place holder and let it show "Empty" */
    {
        GtkTreeIter it;
        GtkTreePath* tp = item_to_tree_path(model, place_holder_l);
        item_to_tree_iter(model, place_holder_l, &it);
        /* if the folder is empty, the place holder item
         * shows "<Empty>" instead of "Loading..." */
        gtk_tree_model_row_changed(model, tp, &it);
        gtk_tree_path_free(tp);
    }
}

static void on_folder_files_added(FmFolder* folder, GSList* files, GList* item_l)
{
    GSList* l;
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    FmDirTreeModel* model = item->model;
    GtkTreePath* tp = item_to_tree_path(model, item_l);
    for(l = files; l; l = l->next)
    {
        FmFileInfo* fi = FM_FILE_INFO(l->data);
        /* FIXME: should FmFolder generate "files-added" signal on
         * its first-time loading? Isn't "loaded" signal enough? */
        if(fm_file_info_is_dir(fi)) /* TODO: maybe adding files can be allowed later */
        {
            /* ensure that the file is not yet in our model */
            GList* new_item_l = children_by_name(model, item->children, fi->path->name, NULL);
            if(!new_item_l)
                new_item_l = insert_file_info(model, item_l, tp, fi);
        }
    }
    gtk_tree_path_free(tp);
}

static void on_folder_files_removed(FmFolder* folder, GSList* files, GList* item_l)
{
    GSList* l;
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    FmDirTreeModel* model = item->model;
    /* GtkTreePath* tp = item_to_tree_path(model, item_l); */
    for(l = files; l; l = l->next)
    {
        FmFileInfo* fi = FM_FILE_INFO(l->data);
        GList* rm_item_l = children_by_name(model, item->children, fi->path->name, NULL);
        if(rm_item_l)
            remove_item(model, rm_item_l);
    }
    /* gtk_tree_path_free(tp); */
}

static void on_folder_files_changed(FmFolder* folder, GSList* files, GList* item_l)
{
    GSList* l;
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    FmDirTreeModel* model = item->model;
    GtkTreePath* tp = item_to_tree_path(model, item_l);

    /* g_debug("files changed!!"); */

    for(l = files; l; l = l->next)
    {
        FmFileInfo* fi = FM_FILE_INFO(l->data);
        int idx;
        GList* changed_item_l = children_by_name(model, item->children, fi->path->name, &idx);
        /* g_debug("changed file: %s", fi->path->name); */
        if(changed_item_l)
        {
            FmDirTreeItem* changed_item = (FmDirTreeItem*)changed_item_l->data;
            if(changed_item->fi)
                fm_file_info_unref(changed_item->fi);
            changed_item->fi = fm_file_info_ref(fi);
            /* FIXME: inform gtk tree view about the change */

            /* FIXME and TODO: check if we have sub folder */
            /* item_queue_subdir_check(model, changed_item_l); */
        }
    }
    gtk_tree_path_free(tp);
}

static inline void item_free_folder(GList* item_l)
{
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    if(item->folder)
    {
        FmFolder* folder = item->folder;
        g_signal_handlers_disconnect_by_func(folder, on_folder_loaded, item_l);
        g_signal_handlers_disconnect_by_func(folder, on_folder_files_added, item_l);
        g_signal_handlers_disconnect_by_func(folder, on_folder_files_removed, item_l);
        g_signal_handlers_disconnect_by_func(folder, on_folder_files_changed, item_l);
        g_object_unref(folder);
        item->folder = NULL;
    }
}

void fm_dir_tree_model_expand_row(FmDirTreeModel* model, GtkTreeIter* it, GtkTreePath* tp)
{
    GList* item_l = (GList*)it->user_data;
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    g_return_if_fail(item != NULL);
    if(item->n_expand == 0)
    {
        /* dynamically load content of the folder. */
        FmFolder* folder = fm_folder_get(item->fi->path);
        item->folder = folder;

        /* associate the data with loaded handler */
        g_signal_connect(folder, "loaded", G_CALLBACK(on_folder_loaded), item_l);
        g_signal_connect(folder, "files-added", G_CALLBACK(on_folder_files_added), item_l);
        g_signal_connect(folder, "files-removed", G_CALLBACK(on_folder_files_removed), item_l);
        g_signal_connect(folder, "files-changed", G_CALLBACK(on_folder_files_changed), item_l);

        /* if the folder is already loaded, call "loaded" handler ourselves */
        if(fm_folder_get_is_loaded(folder)) /* already loaded */
        {
            FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
            FmDirTreeModel* model = item->model;
            GtkTreePath* tp = item_to_tree_path(model, item_l);
            GList* file_l;
            for(file_l = fm_list_peek_head_link(folder->files); file_l; file_l = file_l->next)
            {
                FmFileInfo* fi = file_l->data;
                if(fm_file_info_is_dir(fi))
                {
                    /* FIXME: later we can try to support adding
                     *        files to the tree, too so this model
                     *        can be even more useful. */
                    insert_file_info(model, item_l, tp, fi);
                    /* g_debug("insert: %s", fi->path->name); */
                }
            }
            gtk_tree_path_free(tp);
            on_folder_loaded(folder, item_l);
        }
    }
    ++item->n_expand;
}

void fm_dir_tree_model_collapse_row(FmDirTreeModel* model, GtkTreeIter* it, GtkTreePath* tp)
{
    GList* item_l = (GList*)it->user_data;
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    g_return_if_fail(item != NULL);
    --item->n_expand;
    if(item->n_expand == 0) /* do some cleanup */
    {
        /* remove all children, and replace them with a dummy child
         * item to keep expander in the tree view around. */
        remove_all_children(model, item_l, tp);

        /* now, GtkTreeView think that we have no child since all
         * child items are removed. So we add a place holder child
         * item to keep the expander around. */
        add_place_holder_child_item(model, item_l, tp, TRUE);
    }
}

void fm_dir_tree_model_set_icon_size(FmDirTreeModel* model, guint icon_size)
{
    if(model->icon_size != icon_size)
    {
        /* reload existing icons */
        GtkTreePath* tp = gtk_tree_path_new_first();
        GList* l;
        for(l = model->roots; l; l=l->next)
        {
            item_reload_icon(model, l, tp);
            gtk_tree_path_next(tp);
        }
        gtk_tree_path_free(tp);
    }
}

guint fm_dir_tree_get_icon_size(FmDirTreeModel* model)
{
    return model->icon_size;
}

/* for FmDirTreeView, called in fm_dir_tree_view_init() */
gboolean _fm_dir_tree_view_select_function(GtkTreeSelection *selection,
                                           GtkTreeModel *model,
                                           GtkTreePath *path,
                                           gboolean path_currently_selected,
                                           gpointer data)
{
    GtkTreeIter it;
    GList* l;
    FmDirTreeItem* item;
    fm_dir_tree_model_get_iter(model, &it, path);
    l = (GList*)it.user_data;
    if(!l)
        return FALSE;
    item = (FmDirTreeItem*)l->data;
    return item->fi != NULL;
}

static void item_show_hidden_children(FmDirTreeModel* model, GList* item_l, gboolean show_hidden)
{
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    GList* child_l;
    /* TODO: show hidden items */
    if(show_hidden)
    {
        while(item->hidden_children)
        {

        }
    }
    else
    {
        while(item->children)
        {

        }
    }
}

void fm_dir_tree_model_set_show_hidden(FmDirTreeModel* model, gboolean show_hidden)
{
    if(show_hidden != model->show_hidden)
    {
        /* filter the model to hide hidden folders */
        if(model->show_hidden)
        {

        }
        else
        {

        }
    }
}

gboolean fm_dir_tree_model_get_show_hidden(FmDirTreeModel* model)
{
    return model->show_hidden;
}

#if 0

/* TODO: check if dirs contain sub dir in another thread and make
 * the tree nodes expandable when needed.
 *
 * NOTE: Doing this can improve usability, but due to limitation of UNIX-
 * like systems, this can result in great waste of system resources.
 * This requires continuous monitoring of every dir listed in the tree.
 * With Linux, inotify supports this well, and GFileMonitor uses inotify.
 * However, in other UNIX-like systems, monitoring a file uses a file
 * descriptor. So the max number of files which can be monitored is limited
 * by number available file descriptors. This may potentially use up
 * all available file descriptors in the process when there are many
 * directories expanded in the dir tree.
 * So, after considering and experimenting with this, we decided not to
 * support this feature.
 **/

static gboolean subdir_check_finish(FmDirTreeModel* model)
{
    model->current_subdir_check = NULL;
    if(g_queue_is_empty(&model->subdir_checks))
    {
        model->job_running = FALSE;
        g_debug("all subdir checks are finished!");
        return FALSE;
    }
    else /* still has queued items */
    {
        if(g_cancellable_is_cancelled(model->subdir_cancellable))
            g_cancellable_reset(model->subdir_cancellable);
    }
    return TRUE;
}

static gboolean subdir_check_finish_has_subdir(FmDirTreeModel* model)
{
    GList* item_l = model->current_subdir_check;
    if(!g_cancellable_is_cancelled(model->subdir_cancellable) && item_l)
    {
        GtkTreeIter it;
        FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
        GtkTreePath* tp = item_to_tree_path(model, item_l);
        add_place_holder_child_item(model, item_l, tp, TRUE);
        gtk_tree_path_free(tp);
        g_debug("finished for item with subdir: %s", fm_file_info_get_disp_name(item->fi));
    }
    return subdir_check_finish(model);
}

static gboolean subdir_check_finish_no_subdir(FmDirTreeModel* model)
{
    GList* item_l = model->current_subdir_check;
    if(!g_cancellable_is_cancelled(model->subdir_cancellable) && item_l)
    {
        GtkTreeIter it;
        FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
        if(item->children) /* remove existing subdirs or place holder item if needed. */
        {
            GtkTreePath* tp = item_to_tree_path(model, item_l);
            remove_all_children(model, item_l, tp);
            gtk_tree_path_free(tp);
            g_debug("finished for item with no subdir: %s", fm_file_info_get_disp_name(item->fi));
        }
    }
    return subdir_check_finish(model);
}

static gboolean subdir_check_job(GIOSchedulerJob *job, GCancellable* cancellable, gpointer user_data)
{
    FmDirTreeModel* model = FM_DIR_TREE_MODEL(user_data);
    GList* item_l;
    FmDirTreeItem* item;
    GFile* gf;
    GFileEnumerator* enu;
    gboolean has_subdir = FALSE;

    g_mutex_lock(model->subdir_checks_mutex);
    item_l = (GList*)g_queue_pop_head(&model->subdir_checks);
    item = (FmDirTreeItem*)item_l->data;
    model->current_subdir_check = item_l;
    /* check if this item has subdir */
    gf = fm_path_to_gfile(item->fi->path);
    g_mutex_unlock(model->subdir_checks_mutex);
    g_debug("check subdir for: %s", g_file_get_parse_name(gf));
    enu = g_file_enumerate_children(gf,
                            G_FILE_ATTRIBUTE_STANDARD_NAME","
                            G_FILE_ATTRIBUTE_STANDARD_TYPE","
                            G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                            0, cancellable, NULL);
    if(enu)
    {
        while(!g_cancellable_is_cancelled(cancellable))
        {
            GFileInfo* fi = g_file_enumerator_next_file(enu, cancellable, NULL);
            if(G_LIKELY(fi))
            {
                GFileType type = g_file_info_get_file_type(fi);
                gboolean is_hidden = g_file_info_get_is_hidden(fi);
                g_object_unref(fi);

                if(type == G_FILE_TYPE_DIRECTORY)
                {
                    if(model->show_hidden || !is_hidden)
                    {
                        has_subdir = TRUE;
                        break;
                    }
                }
            }
            else
                break;
        }
        g_file_enumerator_close(enu, cancellable, cancellable);
        g_object_unref(enu);
    }
    g_debug("check result - %s has_dir: %d", g_file_get_parse_name(gf), has_subdir);
    g_object_unref(gf);
    if(has_subdir)
        return g_io_scheduler_job_send_to_mainloop(job,
                        (GSourceFunc)subdir_check_finish_has_subdir,
                        model, NULL);

    return g_io_scheduler_job_send_to_mainloop(job,
                        (GSourceFunc)subdir_check_finish_no_subdir,
                        model, NULL);

}

static void item_queue_subdir_check(FmDirTreeModel* model, GList* item_l)
{
    FmDirTreeItem* item = (FmDirTreeItem*)item_l->data;
    g_return_if_fail(item->fi != NULL);

    g_mutex_lock(model->subdir_checks_mutex);
    g_queue_push_tail(&model->subdir_checks, item_l);
    g_debug("queue subdir check for %s", fm_file_info_get_disp_name(item->fi));
    if(!model->job_running)
    {
        model->job_running = TRUE;
        model->current_subdir_check = (GList*)g_queue_peek_head(&model->subdir_checks);
        g_cancellable_reset(model->subdir_cancellable);
        g_io_scheduler_push_job(subdir_check_job,
                                g_object_ref(model),
                                (GDestroyNotify)g_object_unref,
                                G_PRIORITY_DEFAULT,
                                model->subdir_cancellable);
        g_debug("push job");
    }
    g_mutex_unlock(model->subdir_checks_mutex);
}

#endif
