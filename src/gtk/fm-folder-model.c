/*
 *      fm-folder-model.c
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

#include "fm-folder-model.h"
#include "fm-file-info.h"
#include "fm-icon-pixbuf.h"

#include <gdk/gdk.h>

#include <string.h>
#include <gio/gio.h>

enum{
	LOADED,
    N_SIGNALS
};

typedef struct _FmFolderItem FmFolderItem;
struct _FmFolderItem
{
	FmFileInfo* inf;
	GdkPixbuf* big_icon;
	GdkPixbuf* small_icon;
};

static void fm_folder_model_tree_model_init ( GtkTreeModelIface *iface );
static void fm_folder_model_tree_sortable_init ( GtkTreeSortableIface *iface );
static void fm_folder_model_drag_source_init ( GtkTreeDragSourceIface *iface );
static void fm_folder_model_drag_dest_init ( GtkTreeDragDestIface *iface );

static void fm_folder_model_finalize  			(GObject *object);
G_DEFINE_TYPE_WITH_CODE(FmFolderModel, fm_folder_model, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, fm_folder_model_tree_model_init)
	G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_SORTABLE, fm_folder_model_tree_sortable_init)
	G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_DRAG_SOURCE, fm_folder_model_drag_source_init)
	G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_DRAG_DEST, fm_folder_model_drag_dest_init) )

static GtkTreeModelFlags fm_folder_model_get_flags ( GtkTreeModel *tree_model );
static gint fm_folder_model_get_n_columns ( GtkTreeModel *tree_model );
static GType fm_folder_model_get_column_type ( GtkTreeModel *tree_model,
                                             gint index );
static gboolean fm_folder_model_get_iter ( GtkTreeModel *tree_model,
                                         GtkTreeIter *iter,
                                         GtkTreePath *path );
static GtkTreePath *fm_folder_model_get_path ( GtkTreeModel *tree_model,
                                             GtkTreeIter *iter );
static void fm_folder_model_get_value ( GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      gint column,
                                      GValue *value );
static gboolean fm_folder_model_iter_next ( GtkTreeModel *tree_model,
                                          GtkTreeIter *iter );
static gboolean fm_folder_model_iter_children ( GtkTreeModel *tree_model,
                                              GtkTreeIter *iter,
                                              GtkTreeIter *parent );
static gboolean fm_folder_model_iter_has_child ( GtkTreeModel *tree_model,
                                               GtkTreeIter *iter );
static gint fm_folder_model_iter_n_children ( GtkTreeModel *tree_model,
                                            GtkTreeIter *iter );
static gboolean fm_folder_model_iter_nth_child ( GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent,
                                               gint n );
static gboolean fm_folder_model_iter_parent ( GtkTreeModel *tree_model,
                                            GtkTreeIter *iter,
                                            GtkTreeIter *child );
static gboolean fm_folder_model_get_sort_column_id( GtkTreeSortable* sortable,
                                                  gint* sort_column_id,
                                                  GtkSortType* order );
static void fm_folder_model_set_sort_column_id( GtkTreeSortable* sortable,
                                              gint sort_column_id,
                                              GtkSortType order );
static void fm_folder_model_set_sort_func( GtkTreeSortable *sortable,
                                         gint sort_column_id,
                                         GtkTreeIterCompareFunc sort_func,
                                         gpointer user_data,
                                         GtkDestroyNotify destroy );
static void fm_folder_model_set_default_sort_func( GtkTreeSortable *sortable,
                                                 GtkTreeIterCompareFunc sort_func,
                                                 gpointer user_data,
                                                 GtkDestroyNotify destroy );
static void fm_folder_model_sort ( FmFolderModel* model );

static void fm_folder_model_set_folder( FmFolderModel* model, FmFolder* dir );

/* signal handlers */
static void on_folder_loaded(FmFolder* folder, FmFolderModel* model);

//static void on_thumbnail_loaded( FmFolder* dir, VFSFileInfo* file, FmFolderModel* model );

static GType column_types[ N_FOLDER_MODEL_COLS ];
static guint signals[N_SIGNALS];

void fm_folder_model_init ( FmFolderModel* model )
{
    model->n_items = 0;
    model->items = NULL;
    model->sort_order = -1;
    model->sort_col = -1;
    /* Random int to check whether an iter belongs to our model */
    model->stamp = g_random_int();
}

void fm_folder_model_class_init ( FmFolderModelClass *klass )
{
    GObjectClass * object_class;

    fm_folder_model_parent_class = ( GObjectClass* ) g_type_class_peek_parent ( klass );
    object_class = ( GObjectClass* ) klass;

    object_class->finalize = fm_folder_model_finalize;

    signals[LOADED]=
        g_signal_new("loaded",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderModelClass, loaded),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

void fm_folder_model_tree_model_init ( GtkTreeModelIface *iface )
{
    iface->get_flags = fm_folder_model_get_flags;
    iface->get_n_columns = fm_folder_model_get_n_columns;
    iface->get_column_type = fm_folder_model_get_column_type;
    iface->get_iter = fm_folder_model_get_iter;
    iface->get_path = fm_folder_model_get_path;
    iface->get_value = fm_folder_model_get_value;
    iface->iter_next = fm_folder_model_iter_next;
    iface->iter_children = fm_folder_model_iter_children;
    iface->iter_has_child = fm_folder_model_iter_has_child;
    iface->iter_n_children = fm_folder_model_iter_n_children;
    iface->iter_nth_child = fm_folder_model_iter_nth_child;
    iface->iter_parent = fm_folder_model_iter_parent;

    column_types [ COL_FILE_BIG_ICON ] = GDK_TYPE_PIXBUF;
    column_types [ COL_FILE_SMALL_ICON ] = GDK_TYPE_PIXBUF;
    column_types [ COL_FILE_NAME ] = G_TYPE_STRING;
    column_types [ COL_FILE_DESC ] = G_TYPE_STRING;
    column_types [ COL_FILE_SIZE ] = G_TYPE_STRING;
    column_types [ COL_FILE_DESC ] = G_TYPE_STRING;
    column_types [ COL_FILE_PERM ] = G_TYPE_STRING;
    column_types [ COL_FILE_OWNER ] = G_TYPE_STRING;
    column_types [ COL_FILE_MTIME ] = G_TYPE_STRING;
    column_types [ COL_FILE_INFO ] = G_TYPE_POINTER;
    column_types [ COL_FILE_GICON ] = G_TYPE_ICON;
}

void fm_folder_model_tree_sortable_init ( GtkTreeSortableIface *iface )
{
    /* iface->sort_column_changed = fm_folder_model_sort_column_changed; */
    iface->get_sort_column_id = fm_folder_model_get_sort_column_id;
    iface->set_sort_column_id = fm_folder_model_set_sort_column_id;
    iface->set_sort_func = fm_folder_model_set_sort_func;
    iface->set_default_sort_func = fm_folder_model_set_default_sort_func;
    iface->has_default_sort_func = (gboolean(*)(GtkTreeSortable *))gtk_false;
}

void fm_folder_model_drag_source_init ( GtkTreeDragSourceIface *iface )
{
    /* FIXME: Unused. Will this cause any problem? */
}

void fm_folder_model_drag_dest_init ( GtkTreeDragDestIface *iface )
{
    /* FIXME: Unused. Will this cause any problem? */
}

void fm_folder_model_finalize ( GObject *object )
{
    FmFolderModel* model = ( FmFolderModel* ) object;

    fm_folder_model_set_folder( model, NULL );
    /* must chain up - finalize parent */
    (*G_OBJECT_CLASS(fm_folder_model_parent_class)->finalize)( object );
}

FmFolderModel *fm_folder_model_new ( FmFolder* dir, gboolean show_hidden )
{
    FmFolderModel* model;
    model = ( FmFolderModel* ) g_object_new ( FM_TYPE_FOLDER_MODEL, NULL );
    model->show_hidden = show_hidden;
    fm_folder_model_set_folder( model, dir );
    return model;
}

inline FmFolderItem* fm_folder_item_new(FmFileInfo* inf)
{
	FmFolderItem* item = g_slice_new0(FmFolderItem);
	item->inf = fm_file_info_ref(inf);
	return item;
}

inline void fm_folder_item_free(FmFolderItem* item)
{
    if(item->big_icon)
        g_object_unref(item->big_icon);
    if(item->small_icon)
        g_object_unref(item->small_icon);
	fm_file_info_unref(item->inf);
	g_slice_free(FmFolderItem, item);
}

static void _fm_folder_model_insert_item( FmFolder* dir,
                                 FmFolderItem* new_item,
                                 FmFolderModel* model );

static void _fm_folder_model_files_changed( FmFolder* dir, GSList* files,
                                        FmFolderModel* model )
{
	GSList* l;
	for(l = files; l; l=l->next )
		fm_folder_model_file_changed( model, l->data );
}

static void _fm_folder_model_add_file( FmFolderModel* model, FmFileInfo* file )
{
	if( !model->show_hidden && file->path->name[0] == '.')
	{
		model->hidden = g_list_prepend(model->hidden, fm_folder_item_new(file));
		return;
	}
	fm_folder_model_file_created( model, file);
}

static void _fm_folder_model_files_added( FmFolder* dir, GSList* files,
                                        FmFolderModel* model )
{
	GSList* l;
	FmFileInfo* file;
	for(l = files; l; l=l->next )
		_fm_folder_model_add_file(model, (FmFileInfo*)l->data);
}


static void _fm_folder_model_files_removed( FmFolder* dir, GSList* files,
                                            FmFolderModel* model )
{
	GSList* l;
	for(l = files; l; l=l->next )
		fm_folder_model_file_deleted( model, (FmFileInfo*)l->data);
}

void fm_folder_model_set_folder( FmFolderModel* model, FmFolder* dir )
{
    GList* l;
    if( model->dir == dir )
        return;
    if ( model->dir )
    {
        g_signal_handlers_disconnect_by_func( model->dir,
                                              _fm_folder_model_files_added, model);
        g_signal_handlers_disconnect_by_func( model->dir,
                                              _fm_folder_model_files_removed, model);
        g_signal_handlers_disconnect_by_func( model->dir,
                                              _fm_folder_model_files_changed, model);
        g_signal_handlers_disconnect_by_func( model->dir,
                                              on_folder_loaded, model);

		for(l=model->items;l;l=l->next)
			fm_folder_item_free((FmFolderItem*)l->data);
        g_list_free( model->items );
        g_object_unref( model->dir );
    }
    model->dir = dir;
    model->items = NULL;
    model->n_items = 0;
    if( ! dir )
        return;

    model->dir = (FmFolder*)g_object_ref(model->dir);

    g_signal_connect( model->dir, "files-added",
                      G_CALLBACK(_fm_folder_model_files_added),
                      model);
    g_signal_connect( model->dir, "files-removed",
                      G_CALLBACK(_fm_folder_model_files_removed),
                      model);
    g_signal_connect( model->dir, "files-changed",
                      G_CALLBACK(_fm_folder_model_files_changed),
                      model);
    g_signal_connect( model->dir, "loaded",
                      G_CALLBACK(on_folder_loaded), model);

    if( !fm_list_is_empty(dir->files) )
    {
        for( l = fm_list_peek_head_link(dir->files); l; l = l->next )
			_fm_folder_model_add_file( model, (FmFileInfo*)l->data );
    }

    if( !fm_folder_get_is_loading(model->dir) ) /* if it's already loaded */
        on_folder_loaded(model->dir, model); /* emit 'loaded' signal */
}

gboolean fm_folder_model_get_is_loading(FmFolderModel* model)
{
    return fm_folder_get_is_loading(model->dir);
}

GtkTreeModelFlags fm_folder_model_get_flags ( GtkTreeModel *tree_model )
{
    g_return_val_if_fail( FM_IS_FOLDER_MODEL( tree_model ), ( GtkTreeModelFlags ) 0 );
    return ( GTK_TREE_MODEL_LIST_ONLY | GTK_TREE_MODEL_ITERS_PERSIST );
}

gint fm_folder_model_get_n_columns ( GtkTreeModel *tree_model )
{
    return N_FOLDER_MODEL_COLS;
}

GType fm_folder_model_get_column_type ( GtkTreeModel *tree_model,
                                      gint index )
{
    g_return_val_if_fail ( FM_IS_FOLDER_MODEL( tree_model ), G_TYPE_INVALID );
    g_return_val_if_fail ( index < G_N_ELEMENTS( column_types ) && index >= 0, G_TYPE_INVALID );
    return column_types[ index ];
}

gboolean fm_folder_model_get_iter ( GtkTreeModel *tree_model,
                                  GtkTreeIter *iter,
                                  GtkTreePath *path )
{
    FmFolderModel* model;
    gint *indices, n, depth;
    GList* l;

    g_assert(FM_IS_FOLDER_MODEL(tree_model));
    g_assert(path!=NULL);

    model = FM_FOLDER_MODEL(tree_model);

    indices = gtk_tree_path_get_indices(path);
    depth   = gtk_tree_path_get_depth(path);

    /* we do not allow children */
    g_assert(depth == 1); /* depth 1 = top level; a list only has top level nodes and no children */

    n = indices[0]; /* the n-th top level row */

    if ( n >= model->n_items || n < 0 )
        return FALSE;

    l = g_list_nth( model->items, n );

    g_assert(l != NULL);

    /* We simply store a pointer in the iter */
    iter->stamp = model->stamp;
    iter->user_data  = l;
    iter->user_data2 = l->data;
    iter->user_data3 = NULL;   /* unused */

    return TRUE;
}

GtkTreePath *fm_folder_model_get_path ( GtkTreeModel *tree_model,
                                      GtkTreeIter *iter )
{
    GtkTreePath* path;
    GList* l;
    FmFolderModel* model = FM_FOLDER_MODEL(tree_model);

    g_return_val_if_fail (model, NULL);
    g_return_val_if_fail (iter->stamp == model->stamp, NULL);
    g_return_val_if_fail (iter != NULL, NULL);
    g_return_val_if_fail (iter->user_data != NULL, NULL);

    l = (GList*) iter->user_data;

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, g_list_index(model->items, l->data) );
    return path;
}

void fm_folder_model_get_value ( GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               gint column,
                               GValue *value )
{
    GList* l;
    FmFolderModel* model = FM_FOLDER_MODEL(tree_model);

    g_return_if_fail (FM_IS_FOLDER_MODEL (tree_model));
    g_return_if_fail (iter != NULL);
    g_return_if_fail (column < G_N_ELEMENTS(column_types) );

    g_value_init(value, column_types[column] );

    l = (GList*) iter->user_data;
    g_return_if_fail ( l != NULL );

	FmFolderItem* item = (FmFolderItem*)iter->user_data2;
	FmFileInfo* info = item->inf;

    switch(column)
    {
	case COL_FILE_GICON:
		g_value_set_object(value, info->icon->gicon);
		break;
    case COL_FILE_BIG_ICON:
	{
		if( G_UNLIKELY(!item->big_icon) )
		{
			if(!info->icon)
                return;
            item->big_icon = fm_icon_get_pixbuf(info->icon, 48);
		}
		g_value_set_object(value, item->big_icon);
        break;
	}
    case COL_FILE_SMALL_ICON:
		if( G_UNLIKELY(!item->small_icon) )
		{
			if(!info->icon)
                return;
            item->small_icon = fm_icon_get_pixbuf(info->icon, 24);
		}
		g_value_set_object(value, item->small_icon);
		break;
    case COL_FILE_NAME:
        g_value_set_string( value, info->disp_name );
        break;
    case COL_FILE_SIZE:
        g_value_set_string( value, fm_file_info_get_disp_size(info) );
        break;
    case COL_FILE_DESC:
        g_value_set_string( value, fm_file_info_get_desc( info ) );
        break;
    case COL_FILE_PERM:
//        g_value_set_string( value, fm_file_info_get_disp_perm(info) );
        break;
    case COL_FILE_OWNER:
//        g_value_set_string( value, fm_file_info_get_disp_owner(info) );
        break;
    case COL_FILE_MTIME:
        g_value_set_string( value, fm_file_info_get_disp_mtime(info) );
        break;
    case COL_FILE_INFO:
        g_value_set_pointer( value, fm_file_info_ref( info ) );
        break;
    }
}

gboolean fm_folder_model_iter_next ( GtkTreeModel *tree_model,
                                   GtkTreeIter *iter )
{
    GList* l;
    FmFolderModel* model;

    g_return_val_if_fail (FM_IS_FOLDER_MODEL (tree_model), FALSE);

    if (iter == NULL || iter->user_data == NULL)
        return FALSE;

    model = FM_FOLDER_MODEL(tree_model);
    l = (GList *) iter->user_data;

    /* Is this the last l in the list? */
    if ( ! l->next )
        return FALSE;

    iter->stamp = model->stamp;
    iter->user_data = l->next;
    iter->user_data2 = l->next->data;

    return TRUE;
}

gboolean fm_folder_model_iter_children ( GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       GtkTreeIter *parent )
{
    FmFolderModel* model;
    g_return_val_if_fail ( parent == NULL || parent->user_data != NULL, FALSE );

    /* this is a list, nodes have no children */
    if ( parent )
        return FALSE;

    /* parent == NULL is a special case; we need to return the first top-level row */
    g_return_val_if_fail ( FM_IS_FOLDER_MODEL ( tree_model ), FALSE );
    model = FM_FOLDER_MODEL( tree_model );

    /* No rows => no first row */
//    if ( model->dir->n_items == 0 )
//        return FALSE;

    /* Set iter to first item in list */
    iter->stamp = model->stamp;
    iter->user_data = model->items;
    iter->user_data2 = model->items->data;
    return TRUE;
}

gboolean fm_folder_model_iter_has_child ( GtkTreeModel *tree_model,
                                        GtkTreeIter *iter )
{
    return FALSE;
}

gint fm_folder_model_iter_n_children ( GtkTreeModel *tree_model,
                                     GtkTreeIter *iter )
{
    FmFolderModel* model;
    g_return_val_if_fail ( FM_IS_FOLDER_MODEL ( tree_model ), -1 );
    g_return_val_if_fail ( iter == NULL || iter->user_data != NULL, FALSE );
    model = FM_FOLDER_MODEL( tree_model );
    /* special case: if iter == NULL, return number of top-level rows */
    if ( !iter )
        return model->n_items;
    return 0; /* otherwise, this is easy again for a list */
}

gboolean fm_folder_model_iter_nth_child ( GtkTreeModel *tree_model,
                                        GtkTreeIter *iter,
                                        GtkTreeIter *parent,
                                        gint n )
{
    GList* l;
    FmFolderModel* model;

    g_return_val_if_fail (FM_IS_FOLDER_MODEL (tree_model), FALSE);
    model = FM_FOLDER_MODEL(tree_model);

    /* a list has only top-level rows */
    if(parent)
        return FALSE;

    /* special case: if parent == NULL, set iter to n-th top-level row */
    if( n >= model->n_items || n < 0 )
        return FALSE;

    l = g_list_nth( model->items, n );
    g_assert( l != NULL );

    iter->stamp = model->stamp;
    iter->user_data = l;
    iter->user_data2 = l->data;

    return TRUE;
}

gboolean fm_folder_model_iter_parent ( GtkTreeModel *tree_model,
                                     GtkTreeIter *iter,
                                     GtkTreeIter *child )
{
    return FALSE;
}

gboolean fm_folder_model_get_sort_column_id( GtkTreeSortable* sortable,
                                           gint* sort_column_id,
                                           GtkSortType* order )
{
    FmFolderModel* model = (FmFolderModel*)sortable;
    if( sort_column_id )
        *sort_column_id = model->sort_col;
    if( order )
        *order = model->sort_order;
    return TRUE;
}

void fm_folder_model_set_sort_column_id( GtkTreeSortable* sortable,
                                       gint sort_column_id,
                                       GtkSortType order )
{
    FmFolderModel* model = (FmFolderModel*)sortable;
    if( model->sort_col == sort_column_id && model->sort_order == order )
        return;
    model->sort_col = sort_column_id;
    model->sort_order = order;
    gtk_tree_sortable_sort_column_changed(sortable);
    fm_folder_model_sort(model);
}

void fm_folder_model_set_sort_func( GtkTreeSortable *sortable,
                                  gint sort_column_id,
                                  GtkTreeIterCompareFunc sort_func,
                                  gpointer user_data,
                                  GtkDestroyNotify destroy )
{
    g_warning( "fm_folder_model_set_sort_func: Not supported\n" );
}

void fm_folder_model_set_default_sort_func( GtkTreeSortable *sortable,
                                          GtkTreeIterCompareFunc sort_func,
                                          gpointer user_data,
                                          GtkDestroyNotify destroy )
{
    g_warning( "fm_folder_model_set_default_sort_func: Not supported\n" );
}

static gint fm_folder_model_compare( FmFolderItem* item1,
                                     FmFolderItem* item2,
                                     FmFolderModel* model)
{
    FmFileInfo* file1 = item1->inf;
    FmFileInfo* file2 = item2->inf;
    int ret = 0;

    /* put folders before files */
    ret = fm_file_info_is_dir(file1) - fm_file_info_is_dir(file2);
    if( ret )
        return -ret;

    switch( model->sort_col )
    {
    case COL_FILE_NAME:
	{
		const char* key1 = fm_file_info_get_collate_key(file1);
		const char* key2 = fm_file_info_get_collate_key(file2);
		ret = g_ascii_strcasecmp( key1, key2 );
        break;
	}
    case COL_FILE_SIZE:
        ret = file1->size - file2->size;
        break;
    case COL_FILE_MTIME:
        ret = file1->mtime - file2->mtime;
        break;
    }
    return model->sort_order == GTK_SORT_ASCENDING ? -ret : ret;
}

void fm_folder_model_sort ( FmFolderModel* model )
{
    GHashTable* old_order;
    gint *new_order;
    GtkTreePath *path;
    GList* l;
    int i;

	/* if there is only one item */
    if( ! model->items || ! model->items->next )
		return;

    old_order = g_hash_table_new( g_direct_hash, g_direct_equal );
    /* save old order */
    for( i = 0, l = model->items; l; l = l->next, ++i )
        g_hash_table_insert( old_order, l, GINT_TO_POINTER(i) );

    /* sort the list */
    model->items = g_list_sort_with_data( model->items,
                                         fm_folder_model_compare, model);
    /* save new order */
    new_order = g_new( int, model->n_items );
    for( i = 0, l = model->items; l; l = l->next, ++i )
        new_order[i] = (guint)g_hash_table_lookup( old_order, l );
    g_hash_table_destroy( old_order );
    path = gtk_tree_path_new();
    gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model),
                                   path, NULL, new_order);
    gtk_tree_path_free (path);
    g_free( new_order );
}

void fm_folder_model_file_created( FmFolderModel* model, FmFileInfo* file )
{
	FmFolderItem* new_item = fm_folder_item_new(file);
	_fm_folder_model_insert_item(model->dir, new_item, model);
}

void _fm_folder_model_insert_item( FmFolder* dir,
                                 FmFolderItem* new_item,
                                 FmFolderModel* model )
{
    GList* l;
    GtkTreeIter it;
    GtkTreePath* path;
	FmFolderItem* item;
	FmFileInfo* file = new_item->inf;

    for( l = model->items; l; l = l->next )
    {
        item = (FmFolderItem*)l->data;
        if( G_UNLIKELY( file == item->inf || strcmp(file->path->name, item->inf->path->name) == 0) )
        {
            /* The file is already in the list */
			fm_folder_item_free(new_item);
            return;
        }
        if( fm_folder_model_compare( item, new_item, model) > 0 )
        {
            break;
        }
    }
    model->items = g_list_insert_before( model->items, l, new_item );
    ++model->n_items;

    if( l )
        l = l->prev;
    else
        l = g_list_last( model->items );

    it.stamp = model->stamp;
    it.user_data = l;
    it.user_data2 = new_item;
	it.user_data3 = new_item->inf;

    path = gtk_tree_path_new_from_indices( g_list_index(model->items, l->data), -1 );
    gtk_tree_model_row_inserted( GTK_TREE_MODEL(model), path, &it );
    gtk_tree_path_free( path );
}


void fm_folder_model_file_deleted( FmFolderModel* model, FmFileInfo* file)
{
    GList* l;
    GtkTreePath* path;
    FmFolderItem* item;
#if 0
    /* If there is no file info, that means the dir itself was deleted. */
    if( G_UNLIKELY( ! file ) )
    {
        /* Clear the whole list */
        path = gtk_tree_path_new_from_indices(0, -1);
        for( l = model->items; l; l = model->items )
        {
            gtk_tree_model_row_deleted( GTK_TREE_MODEL(model), path );
            file = (VFSFileInfo*)l->data;
            model->items = g_list_delete_link( model->items, l );
            vfs_file_info_unref( file );
            --model->n_items;
        }
        gtk_tree_path_free( path );
        return;
    }
#endif

    if( !model->show_hidden && file->path->name[0] == '.' ) /* if this is a hidden file */
    {
        for( l = model->hidden; l; l = l->next )
        {
            item = (FmFolderItem*)l->data;
            if(item->inf == file)
                break;
        }
        if( ! l )
            return;
        model->hidden = g_list_delete_link( model->hidden, l );
        fm_folder_item_free( item );        
    }

    for( l = model->items; l; l = l->next )
    {
        item = (FmFolderItem*)l->data;
        if(item->inf == file)
            break;
    }
    if( ! l )
        return;

    path = gtk_tree_path_new_from_indices( g_list_index(model->items, l->data), -1 );
    gtk_tree_model_row_deleted( GTK_TREE_MODEL(model), path );
    gtk_tree_path_free( path );

    model->items = g_list_delete_link( model->items, l );
    fm_folder_item_free( item );
    --model->n_items;
}

void fm_folder_model_file_changed( FmFolderModel* model, FmFileInfo* file )
{
	FmFolderItem* item;
    GList* l;
    GtkTreeIter it;
    GtkTreePath* path;

    if( !model->show_hidden && file->path->name[0] == '.' )
        return;

    for( l = model->items; l; l = l->next )
    {
        item = (FmFolderItem*)l->data;
        if(item->inf == file)
            break;
    }
    if( ! l )
        return;

	/* update the icon */
	if(item->big_icon)
	{
		g_object_unref(item->big_icon);
		item->big_icon = NULL;
	}
	if(item->small_icon)
	{
		g_object_unref(item->small_icon);
		item->small_icon = NULL;
	}

    it.stamp = model->stamp;
    it.user_data = l;
    it.user_data2 = item;

    path = gtk_tree_path_new_from_indices( g_list_index(model->items, l->data), -1 );
    gtk_tree_model_row_changed( GTK_TREE_MODEL(model), path, &it );
    gtk_tree_path_free( path );
}


#if 0

gboolean fm_folder_model_find_iter(  FmFolderModel* model, GtkTreeIter* it, VFSFileInfo* fi )
{
    GList* l;
    for( l = model->items; l; l = l->next )
    {
        VFSFileInfo* fi2 = (VFSFileInfo*)l->data;
        if( G_UNLIKELY( fi2 == fi
            || 0 == strcmp( vfs_file_info_get_name(fi), vfs_file_info_get_name(fi2) ) ) )
        {
            it->stamp = model->stamp;
            it->user_data = l;
            it->user_data2 = fi2;
            return TRUE;
        }
    }
    return FALSE;
}


void on_thumbnail_loaded( FmFolder* dir, VFSFileInfo* file, FmFolderModel* model )
{
    /* g_debug( "LOADED: %s", file->path->name ); */
    fm_folder_model_file_changed( dir, file, model);
}

void fm_folder_model_show_thumbnails( FmFolderModel* model, gboolean is_big,
                                    int max_file_size )
{
    GList* l;
    VFSFileInfo* file;
    int old_max_thumbnail;

    old_max_thumbnail = model->max_thumbnail;
    model->max_thumbnail = max_file_size;
    model->big_thumbnail = is_big;
    /* FIXME: This is buggy!!! Further testing might be needed.
    */
    if( 0 == max_file_size )
    {
        if( old_max_thumbnail > 0 ) /* cancel thumbnails */
        {
            vfs_thumbnail_loader_cancel_all_requests( model->dir, model->big_thumbnail );
            g_signal_handlers_disconnect_by_func( model->dir, on_thumbnail_loaded, model);

            for( l = model->items; l; l = l->next )
            {
                file = (VFSFileInfo*)l->data;
                if( vfs_file_info_is_image( file )
                    && vfs_file_info_is_thumbnail_loaded( file, is_big ) )
                {
                    /* update the model */
                    fm_folder_model_file_changed( model->dir, file, model);
                }
            }
        }
        return;
    }

    g_signal_connect( model->dir, "thumbnail-loaded",
                                    G_CALLBACK(on_thumbnail_loaded), model);

    for( l = model->items; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        if( vfs_file_info_is_image( file )
            && vfs_file_info_get_size( file ) < model->max_thumbnail )
        {
            if( vfs_file_info_is_thumbnail_loaded( file, is_big ) )
                fm_folder_model_file_changed( model->dir, file, model);
            else
            {
                vfs_thumbnail_loader_request( model->dir, file, is_big );
                /* g_debug( "REQUEST: %s", file->path->name ); */
            }
        }
    }
}

#endif

void fm_folder_model_set_show_hidden( FmFolderModel* model, gboolean show_hidden )
{
	FmFolderItem* item;
	GList *l, *next;
	if(model->show_hidden == show_hidden)
		return;

	model->show_hidden = show_hidden;
	if(show_hidden) /* add previously hidden items back to the list */
	{
		for(l = model->hidden; l; l=l->next )
		{
			item = (FmFolderItem*)l->data;
			if(item->inf->path->name[0]=='.') /* in the future there will be other filtered out files in the hidden list */
				_fm_folder_model_insert_item(model->dir, item, model);
		}
		g_list_free(model->hidden);
		model->hidden = NULL;
	}
	else /* move invisible items to hidden list */
	{
		for(l = model->items; l; l=next )
		{
			GtkTreePath* tp;
			next = l->next;
			item = (FmFolderItem*)l->data;
			if(item->inf->path->name[0] == '.')
			{
				model->hidden = g_list_prepend(model->hidden, item);

				tp = gtk_tree_path_new_from_indices( g_list_index(model->items, l->data), -1 );
				/* tell everybody that we removed an item */
    				gtk_tree_model_row_deleted( GTK_TREE_MODEL(model), tp );
				gtk_tree_path_free( tp );
				model->items = g_list_delete_link(model->items, l);
				--model->n_items;
			}
		}
	}
}

void on_folder_loaded(FmFolder* folder, FmFolderModel* model)
{
    g_signal_emit(model, signals[LOADED], 0);
}
