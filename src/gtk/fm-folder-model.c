/*
*  C Implementation: ptk-file-list
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "fm-folder-model.h"
#include "fm-file-info.h"

#include <gdk/gdk.h>

#include <string.h>
#include <gio/gio.h>

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
static void fm_folder_model_sort ( FmFolderModel* list );

static void fm_folder_model_set_folder( FmFolderModel* list, FmFolder* dir );

/* signal handlers */

//static void on_thumbnail_loaded( FmFolder* dir, VFSFileInfo* file, FmFolderModel* list );

static GType column_types[ N_FOLDER_MODEL_COLS ];


void fm_folder_model_init ( FmFolderModel *list )
{
    list->n_items = 0;
    list->items = NULL;
    list->sort_order = -1;
    list->sort_col = -1;
    /* Random int to check whether an iter belongs to our model */
    list->stamp = g_random_int();
}

void fm_folder_model_class_init ( FmFolderModelClass *klass )
{
    GObjectClass * object_class;

    fm_folder_model_parent_class = ( GObjectClass* ) g_type_class_peek_parent ( klass );
    object_class = ( GObjectClass* ) klass;

    object_class->finalize = fm_folder_model_finalize;
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
    FmFolderModel *list = ( FmFolderModel* ) object;

    fm_folder_model_set_folder( list, NULL );
    /* must chain up - finalize parent */
    (*G_OBJECT_CLASS(fm_folder_model_parent_class)->finalize)( object );
}

FmFolderModel *fm_folder_model_new ( FmFolder* dir, gboolean show_hidden )
{
    FmFolderModel * list;
    list = ( FmFolderModel* ) g_object_new ( FM_TYPE_FOLDER_MODEL, NULL );
    list->show_hidden = show_hidden;
    fm_folder_model_set_folder( list, dir );
    return list;
}

inline FmFolderItem* fm_folder_item_new(FmFileInfo* inf)
{
	FmFolderItem* item = g_slice_new0(FmFolderItem);
	item->inf = fm_file_info_ref(inf);
	return item;
}

inline void fm_folder_item_free(FmFolderItem* item)
{
	fm_file_info_unref(item->inf);
	g_slice_free(FmFolderItem, item);
}

static void _fm_folder_model_insert_item( FmFolder* dir,
                                 FmFolderItem* new_item,
                                 FmFolderModel* list );

static void _fm_folder_model_files_changed( FmFolder* dir, GSList* files,
                                        FmFolderModel* list )
{
	GSList* l;
	for(l = files; l; l=l->next )
		fm_folder_model_file_changed( dir, l->data, list );
}

static void _fm_folder_model_files_added( FmFolder* dir, GSList* files,
                                        FmFolderModel* list )
{
	GSList* l;
	FmFileInfo* file;
	for(l = files; l; l=l->next )
	{
		file = (FmFileInfo*)l->data;
		if( !list->show_hidden && file->path->name[0] == '.')
		{
			list->hidden = g_list_prepend(list->hidden, fm_folder_item_new(file));
			continue;
		}
		fm_folder_model_file_created( dir, file, list );
	}
}


static void _fm_folder_model_files_removed( FmFolder* dir, GSList* files,
                                            FmFolderModel* list )
{
	GSList* l;
	for(l = files; l; l=l->next )
		fm_folder_model_file_deleted( dir, (FmFileInfo*)l->data, list );
}

void fm_folder_model_set_folder( FmFolderModel* list, FmFolder* dir )
{
    GList* l;

    if( list->dir == dir )
        return;

    if ( list->dir )
    {
/*
        g_signal_handlers_disconnect_by_func( list->dir,
                                              on_folder_loaded, list );
*/
/*
        g_signal_handlers_disconnect_by_func( list->dir,
                                              _fm_folder_model_files_added, list );
        g_signal_handlers_disconnect_by_func( list->dir,
                                              fm_folder_model_file_deleted, list );
        g_signal_ahandlers_disconnect_by_func( list->dir,
                                              _fm_folder_model_files_changed, list );
        g_signal_handlers_disconnect_by_func( list->dir,
                                              on_thumbnail_loaded, list );
*/
		for(l=list->items;l;l=l->next)
			fm_folder_item_free((FmFolderItem*)l->data);
        g_list_free( list->items );
        g_object_unref( list->dir );
    }

    list->dir = dir;
    list->items = NULL;
    list->n_items = 0;
    if( ! dir )
        return;

    list->dir = (FmFolder*)g_object_ref(list->dir);

    g_signal_connect( list->dir, "files-added",
                      G_CALLBACK(_fm_folder_model_files_added),
                      list );
    g_signal_connect( list->dir, "files-removed",
                      G_CALLBACK(_fm_folder_model_files_removed),
                      list );
    g_signal_connect( list->dir, "files-changed",
                      G_CALLBACK(_fm_folder_model_files_changed),
                      list );
    if( dir && dir->files )
    {
        for( l = dir->files; l; l = l->next )
        {
			if(((FmFileInfo*)l->data)->path->name[0] == '.') /* a hidden file */
			{
				if( ! list->show_hidden )
				{
					list->hidden = g_list_prepend(list->hidden, fm_folder_item_new((FmFileInfo*)l->data));
					continue;
				}
			}
			list->items = g_list_prepend( list->items, fm_folder_item_new((FmFileInfo*)l->data) );
			++list->n_items;
        }
    }
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
    FmFolderModel *list;
    gint *indices, n, depth;
    GList* l;

    g_assert(FM_IS_FOLDER_MODEL(tree_model));
    g_assert(path!=NULL);

    list = FM_FOLDER_MODEL(tree_model);

    indices = gtk_tree_path_get_indices(path);
    depth   = gtk_tree_path_get_depth(path);

    /* we do not allow children */
    g_assert(depth == 1); /* depth 1 = top level; a list only has top level nodes and no children */

    n = indices[0]; /* the n-th top level row */

    if ( n >= list->n_items || n < 0 )
        return FALSE;

    l = g_list_nth( list->items, n );

    g_assert(l != NULL);

    /* We simply store a pointer in the iter */
    iter->stamp = list->stamp;
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
    FmFolderModel* list = FM_FOLDER_MODEL(tree_model);

    g_return_val_if_fail (list, NULL);
    g_return_val_if_fail (iter->stamp == list->stamp, NULL);
    g_return_val_if_fail (iter != NULL, NULL);
    g_return_val_if_fail (iter->user_data != NULL, NULL);

    l = (GList*) iter->user_data;

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, g_list_index(list->items, l->data) );
    return path;
}

void fm_folder_model_get_value ( GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               gint column,
                               GValue *value )
{
    GList* l;
    FmFolderModel* list = FM_FOLDER_MODEL(tree_model);

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
		g_value_set_object(value, info->icon);
		break;
    case COL_FILE_BIG_ICON:
	{
		if( G_UNLIKELY(!item->big_icon) )
		{
			char* names;
			GdkPixbuf* pix;
			if(!info->icon) return;

			GtkIconInfo* ii = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), info->icon, 48, 0);
			if(ii)
			{
				pix = gtk_icon_info_load_icon(ii, NULL);
				gtk_icon_info_free(ii);
				item->big_icon = pix;
			}
		}
		g_value_set_object(value, item->big_icon);
        break;
	}
    case COL_FILE_SMALL_ICON:
		if( G_UNLIKELY(!item->big_icon) )
		{
			char* names;
			GdkPixbuf* pix;
			if(!info->icon) return;

			GtkIconInfo* ii = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), info->icon, 24, 0);
			pix = gtk_icon_info_load_icon(ii, NULL);
			gtk_icon_info_free(ii);
			item->small_icon = pix;
		}
		g_value_set_object(value, item->big_icon);
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
    FmFolderModel* list;

    g_return_val_if_fail (FM_IS_FOLDER_MODEL (tree_model), FALSE);

    if (iter == NULL || iter->user_data == NULL)
        return FALSE;

    list = FM_FOLDER_MODEL(tree_model);
    l = (GList *) iter->user_data;

    /* Is this the last l in the list? */
    if ( ! l->next )
        return FALSE;

    iter->stamp = list->stamp;
    iter->user_data = l->next;
    iter->user_data2 = l->next->data;

    return TRUE;
}

gboolean fm_folder_model_iter_children ( GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       GtkTreeIter *parent )
{
    FmFolderModel* list;
    g_return_val_if_fail ( parent == NULL || parent->user_data != NULL, FALSE );

    /* this is a list, nodes have no children */
    if ( parent )
        return FALSE;

    /* parent == NULL is a special case; we need to return the first top-level row */
    g_return_val_if_fail ( FM_IS_FOLDER_MODEL ( tree_model ), FALSE );
    list = FM_FOLDER_MODEL( tree_model );

    /* No rows => no first row */
//    if ( list->dir->n_items == 0 )
//        return FALSE;

    /* Set iter to first item in list */
    iter->stamp = list->stamp;
    iter->user_data = list->items;
    iter->user_data2 = list->items->data;
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
    FmFolderModel* list;
    g_return_val_if_fail ( FM_IS_FOLDER_MODEL ( tree_model ), -1 );
    g_return_val_if_fail ( iter == NULL || iter->user_data != NULL, FALSE );
    list = FM_FOLDER_MODEL( tree_model );
    /* special case: if iter == NULL, return number of top-level rows */
    if ( !iter )
        return list->n_items;
    return 0; /* otherwise, this is easy again for a list */
}

gboolean fm_folder_model_iter_nth_child ( GtkTreeModel *tree_model,
                                        GtkTreeIter *iter,
                                        GtkTreeIter *parent,
                                        gint n )
{
    GList* l;
    FmFolderModel* list;

    g_return_val_if_fail (FM_IS_FOLDER_MODEL (tree_model), FALSE);
    list = FM_FOLDER_MODEL(tree_model);

    /* a list has only top-level rows */
    if(parent)
        return FALSE;

    /* special case: if parent == NULL, set iter to n-th top-level row */
    if( n >= list->n_items || n < 0 )
        return FALSE;

    l = g_list_nth( list->items, n );
    g_assert( l != NULL );

    iter->stamp = list->stamp;
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
    FmFolderModel* list = (FmFolderModel*)sortable;
    if( sort_column_id )
        *sort_column_id = list->sort_col;
    if( order )
        *order = list->sort_order;
    return TRUE;
}

void fm_folder_model_set_sort_column_id( GtkTreeSortable* sortable,
                                       gint sort_column_id,
                                       GtkSortType order )
{
    FmFolderModel* list = (FmFolderModel*)sortable;
    if( list->sort_col == sort_column_id && list->sort_order == order )
        return;
    list->sort_col = sort_column_id;
    list->sort_order = order;
    gtk_tree_sortable_sort_column_changed (sortable);
    fm_folder_model_sort (list);
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
                                     FmFolderModel* list)
{
    FmFileInfo* file1 = item1->inf;
    FmFileInfo* file2 = item2->inf;
    int ret = 0;

    /* put folders before files */
    ret = fm_file_info_is_dir(file1) - fm_file_info_is_dir(file2);
    if( ret )
        return -ret;

    /* FIXME: strings should not be treated as ASCII when sorted  */
    switch( list->sort_col )
    {
    case COL_FILE_NAME:
	{
		gchar* key1 = g_utf8_collate_key_for_filename(file1->disp_name, -1);
		gchar* key2 = g_utf8_collate_key_for_filename(file2->disp_name, -1);
		ret = strcmp ( key1, key2 );
		g_free( key1 );
		g_free( key2 );
        break;
	}
    case COL_FILE_SIZE:
        ret = file1->size - file2->size;
        break;
#if 0
    case COL_FILE_DESC:
	{
/*
		char* key1 = g_utf8_collate_key(file1->disp_name, -1);
		char* key2 = g_utf8_collate_key(file2->disp_name, -1);
        ret = g_utf8_collate( key1, key2 );
		g_free( key1 );
		g_free( key2 );
*/
        break;
	}
    case COL_FILE_PERM:
        ret = g_ascii_strcasecmp( vfs_file_info_get_disp_perm(file1),
                                  vfs_file_info_get_disp_perm(file2) );
        break;
    case COL_FILE_OWNER:
        ret = g_ascii_strcasecmp( vfs_file_info_get_disp_owner(file1),
                                  vfs_file_info_get_disp_owner(file2) );
        break;
#endif
    case COL_FILE_MTIME:
        ret = file1->mtime - file2->mtime;
        break;
    }
    return list->sort_order == GTK_SORT_ASCENDING ? ret : -ret;
}

void fm_folder_model_sort ( FmFolderModel* list )
{
    GHashTable* old_order;
    gint *new_order;
    GtkTreePath *path;
    GList* l;
    int i;

	/* if there is only one item */
    if( ! list->items || ! list->items->next )
		return;

    old_order = g_hash_table_new( g_direct_hash, g_direct_equal );
    /* save old order */
    for( i = 0, l = list->items; l; l = l->next, ++i )
        g_hash_table_insert( old_order, l, GINT_TO_POINTER(i) );

g_debug("HERE0:%p", list->items);
    /* sort the list */
    list->items = g_list_sort_with_data( list->items,
                                         fm_folder_model_compare, list );
g_debug("HERE1:%p", list->items);
    /* save new order */
    new_order = g_new( int, list->n_items );
    for( i = 0, l = list->items; l; l = l->next, ++i )
        new_order[i] = (guint)g_hash_table_lookup( old_order, l );
    g_hash_table_destroy( old_order );
    path = gtk_tree_path_new();
    gtk_tree_model_rows_reordered (GTK_TREE_MODEL (list),
                                   path, NULL, new_order);
    gtk_tree_path_free (path);
    g_free( new_order );
}

void fm_folder_model_file_created( FmFolder* dir,
                                 FmFileInfo* file,
                                 FmFolderModel* list )
{
	FmFolderItem* new_item = fm_folder_item_new(file);
	_fm_folder_model_insert_item(dir, new_item, list);
}

void _fm_folder_model_insert_item( FmFolder* dir,
                                 FmFolderItem* new_item,
                                 FmFolderModel* list )
{
    GList* l;
    GtkTreeIter it;
    GtkTreePath* path;
	FmFolderItem* item;
	FmFileInfo* file = new_item->inf;

    for( l = list->items; l; l = l->next )
    {
        item = (FmFolderItem*)l->data;
        if( G_UNLIKELY( file == item->inf || strcmp(file->path->name, item->inf->path->name) == 0) )
        {
            /* The file is already in the list */
			fm_folder_item_free(new_item);
            return;
        }
        if( fm_folder_model_compare( item, new_item, list ) > 0 )
        {
            break;
        }
    }
    list->items = g_list_insert_before( list->items, l, new_item );
    ++list->n_items;

    if( l )
        l = l->prev;
    else
        l = g_list_last( list->items );

    it.stamp = list->stamp;
    it.user_data = l;
    it.user_data2 = new_item;
	it.user_data3 = new_item->inf;

    path = gtk_tree_path_new_from_indices( g_list_index(list->items, l->data), -1 );
    gtk_tree_model_row_inserted( GTK_TREE_MODEL(list), path, &it );
    gtk_tree_path_free( path );
}


void fm_folder_model_file_deleted( FmFolder* dir,
                                 FmFileInfo* file,
                                 FmFolderModel* list )
{
#if 0
    GList* l;
    GtkTreePath* path;

    /* If there is no file info, that means the dir itself was deleted. */
    if( G_UNLIKELY( ! file ) )
    {
        /* Clear the whole list */
        path = gtk_tree_path_new_from_indices(0, -1);
        for( l = list->items; l; l = list->items )
        {
            gtk_tree_model_row_deleted( GTK_TREE_MODEL(list), path );
            file = (VFSFileInfo*)l->data;
            list->items = g_list_delete_link( list->items, l );
            vfs_file_info_unref( file );
            --list->n_items;
        }
        gtk_tree_path_free( path );
        return;
    }

    if( ! list->show_hidden && vfs_file_info_get_name(file)[0] == '.' )
        return;

    l = g_list_find( list->items, file );
    if( ! l )
        return;

    path = gtk_tree_path_new_from_indices( g_list_index(list->items, l->data), -1 );

    gtk_tree_model_row_deleted( GTK_TREE_MODEL(list), path );

    gtk_tree_path_free( path );

    list->items = g_list_delete_link( list->items, l );
    vfs_file_info_unref( file );
    --list->n_items;
#endif
}

void fm_folder_model_file_changed( FmFolder* dir,
                                 FmFileInfo* file,
                                 FmFolderModel* list )
{
/*
    GList* l;
    GtkTreeIter it;
    GtkTreePath* path;

    if( ! list->show_hidden && vfs_file_info_get_name(file)[0] == '.' )
        return;
    l = g_list_find( list->items, file );

    if( ! l )
        return;

    it.stamp = list->stamp;
    it.user_data = l;
    it.user_data2 = l->data;

    path = gtk_tree_path_new_from_indices( g_list_index(list->items, l->data), -1 );

    gtk_tree_model_row_changed( GTK_TREE_MODEL(list), path, &it );

    gtk_tree_path_free( path );
*/
}


#if 0

gboolean fm_folder_model_find_iter(  FmFolderModel* list, GtkTreeIter* it, VFSFileInfo* fi )
{
    GList* l;
    for( l = list->items; l; l = l->next )
    {
        VFSFileInfo* fi2 = (VFSFileInfo*)l->data;
        if( G_UNLIKELY( fi2 == fi
            || 0 == strcmp( vfs_file_info_get_name(fi), vfs_file_info_get_name(fi2) ) ) )
        {
            it->stamp = list->stamp;
            it->user_data = l;
            it->user_data2 = fi2;
            return TRUE;
        }
    }
    return FALSE;
}


void on_thumbnail_loaded( FmFolder* dir, VFSFileInfo* file, FmFolderModel* list )
{
    /* g_debug( "LOADED: %s", file->path->name ); */
    fm_folder_model_file_changed( dir, file, list );
}

void fm_folder_model_show_thumbnails( FmFolderModel* list, gboolean is_big,
                                    int max_file_size )
{
    GList* l;
    VFSFileInfo* file;
    int old_max_thumbnail;

    old_max_thumbnail = list->max_thumbnail;
    list->max_thumbnail = max_file_size;
    list->big_thumbnail = is_big;
    /* FIXME: This is buggy!!! Further testing might be needed.
    */
    if( 0 == max_file_size )
    {
        if( old_max_thumbnail > 0 ) /* cancel thumbnails */
        {
            vfs_thumbnail_loader_cancel_all_requests( list->dir, list->big_thumbnail );
            g_signal_handlers_disconnect_by_func( list->dir, on_thumbnail_loaded, list );

            for( l = list->items; l; l = l->next )
            {
                file = (VFSFileInfo*)l->data;
                if( vfs_file_info_is_image( file )
                    && vfs_file_info_is_thumbnail_loaded( file, is_big ) )
                {
                    /* update the model */
                    fm_folder_model_file_changed( list->dir, file, list );
                }
            }
        }
        return;
    }

    g_signal_connect( list->dir, "thumbnail-loaded",
                                    G_CALLBACK(on_thumbnail_loaded), list );

    for( l = list->items; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        if( vfs_file_info_is_image( file )
            && vfs_file_info_get_size( file ) < list->max_thumbnail )
        {
            if( vfs_file_info_is_thumbnail_loaded( file, is_big ) )
                fm_folder_model_file_changed( list->dir, file, list );
            else
            {
                vfs_thumbnail_loader_request( list->dir, file, is_big );
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

