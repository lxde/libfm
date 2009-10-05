/*
 *      fm-path-entry.c
 *      
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
 *      Copyright 2009 Jürgen Hötzel <juergen@archlinux.org>
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

#include "fm-path-entry.h"
/* for completion */
#include "fm-folder-model.h"
#include <string.h>
#include <gio/gio.h>

#define FM_PATH_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FM_TYPE_PATH_ENTRY, FmPathEntryPrivate))

typedef struct _FmPathEntryPrivate FmPathEntryPrivate;

struct _FmPathEntryPrivate 
{
    /* associated with a folder model */
    FmFolderModel* model;
    /* initialized once on folder model change for faster completion */
    const gchar *model_path_str;
    /* Current len of the completion string */
    gint completion_len;	
    GtkEntryCompletion* completion;
};

G_DEFINE_TYPE (FmPathEntry, fm_path_entry, GTK_TYPE_ENTRY)

static void      fm_path_entry_activate         (GtkEntry *entry);
static void 	 fm_path_entry_class_init 	(FmPathEntryClass *klass);
static void 	 fm_path_entry_init 		(FmPathEntry *entry);
static void 	 fm_path_entry_finalize 	(GObject *object);
static gboolean  fm_path_entry_match_func 	(GtkEntryCompletion   *completion,
						 const gchar          *key,
						 GtkTreeIter          *iter,
						 gpointer              user_data);
static gboolean  fm_path_entry_match_selected	(GtkEntryCompletion *widget,
						 GtkTreeModel       *model,
						 GtkTreeIter        *iter,
						 gpointer            user_data);
static void fm_path_entry_completion_render_func (GtkCellLayout *cell_layout, 
						  GtkCellRenderer *cell, 
						  GtkTreeModel *model,
						  GtkTreeIter *iter, 
						  gpointer data);

static void  fm_path_entry_activate (GtkEntry *entry )
{
    /* Chain up so that entry->activates_default is honored */
    GTK_ENTRY_CLASS (fm_path_entry_parent_class)->activate (entry);
}

static void fm_path_entry_class_init (FmPathEntryClass *klass)
{      
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS( klass );
    GObjectClass* object_class = G_OBJECT_CLASS( klass );
    GtkEntryClass* entry_class = GTK_ENTRY_CLASS( klass );
    
    object_class->finalize = fm_path_entry_finalize;
    entry_class->activate = fm_path_entry_activate;
    
    g_type_class_add_private (klass, sizeof (FmPathEntryPrivate));
}

static void
fm_path_entry_init (FmPathEntry *entry)
{
    FmPathEntryPrivate *private = FM_PATH_ENTRY_GET_PRIVATE(entry);
    GtkEntryCompletion* completion = gtk_entry_completion_new();
    GtkCellRenderer* render;

    private->model = NULL;
    private->model_path_str = NULL;
    private->completion_len = 0;
    private->completion = completion;
    
    gtk_entry_completion_set_minimum_key_length( completion, 1 );
    gtk_entry_completion_set_match_func( completion, fm_path_entry_match_func, NULL, NULL );
    g_signal_connect(G_OBJECT (completion), "match-selected", G_CALLBACK(fm_path_entry_match_selected), (gpointer)  NULL);
    g_object_set( completion, "text-column", COL_FILE_NAME, NULL );
    render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( (GtkCellLayout*)completion, render, TRUE );
    gtk_cell_layout_set_cell_data_func( GTK_CELL_LAYOUT(completion), render, fm_path_entry_completion_render_func, entry, NULL );
    
    gtk_cell_layout_add_attribute( (GtkCellLayout*)completion, render, "markup", COL_FILE_NAME );
    gtk_entry_completion_set_inline_completion( completion, TRUE );
    gtk_entry_completion_set_popup_set_width( completion, TRUE );
    gtk_entry_set_completion( GTK_ENTRY(entry), completion );
    g_object_unref (G_OBJECT (completion));
}

static void fm_path_entry_completion_render_func (GtkCellLayout *cell_layout, 
						  GtkCellRenderer *cell, 
						  GtkTreeModel *model,
						  GtkTreeIter *iter, 
						  gpointer data)
{
    gchar markup[PATH_MAX];
    gchar *model_file_name;
    FmPathEntryPrivate *private = FM_PATH_ENTRY_GET_PRIVATE( FM_PATH_ENTRY ( data ) );
    gchar *trail = g_stpcpy( markup, "<b><u>" );

    gtk_tree_model_get( GTK_TREE_MODEL( model ), iter, 
			COL_FILE_NAME, &model_file_name, -1 );
    trail = strncpy( trail, model_file_name, private->completion_len ) + private->completion_len;
    trail = g_stpcpy( trail, "</u></b>" );
    trail = g_stpcpy( trail, model_file_name + private->completion_len );

    g_object_set(cell, "markup", markup, NULL );
}

static void
fm_path_entry_finalize (GObject *object)
{
    FmPathEntryPrivate* private = FM_PATH_ENTRY_GET_PRIVATE(object);    
    /* release the folder model reference */
    if (G_LIKELY (private->model ) )
	g_object_unref (G_OBJECT (private->model));
}

GtkWidget* fm_path_entry_new()
{
    return g_object_new (FM_TYPE_PATH_ENTRY, NULL);
}

void fm_path_entry_set_model(FmPathEntry *entry, FmFolderModel* model) 
{
    FmPathEntryPrivate *private = FM_PATH_ENTRY_GET_PRIVATE(entry);

    if (private->model)
	g_object_unref ( private->model );    
    private->model = model;
    private->model_path_str = fm_path_to_str( FM_FOLDER_MODEL(model)->dir->dir_path );
    
    g_object_ref( private->model );
    gtk_entry_completion_set_model( private->completion, GTK_TREE_MODEL(private->model) );
}

static gboolean fm_path_entry_match_func (GtkEntryCompletion   *completion,
					  const gchar          *key,
					  GtkTreeIter          *iter,
					  gpointer              user_data) 
{
    GtkTreeModel *model = gtk_entry_completion_get_model( completion );
    FmPathEntry *pe = FM_PATH_ENTRY( gtk_entry_completion_get_entry( completion ) );
    FmPathEntryPrivate *private = FM_PATH_ENTRY_GET_PRIVATE( pe );
    FmFileInfo *model_file_info;
    gchar *model_file_name;
    /* get original key (case sensitive) */
    const gchar *original_key = gtk_entry_get_text( GTK_ENTRY(pe) ) ;
    /* find sep in key */
    gchar *key_file_name = strrchr( original_key, G_DIR_SEPARATOR ) + 1;					      
    gboolean is_dir;

    private->completion_len = strlen( key_file_name );
    
    /* no model loaded */
    if (!model)
	return FALSE;
    
    /* Check if path entry is part of folder model */
    if ( !g_str_has_prefix ( original_key, private->model_path_str ) )
	/* FIXME: Need fallback for folder based completion */
	return FALSE;

    /* get filename, info from model */
    gtk_tree_model_get( GTK_TREE_MODEL( model ), iter, 
			COL_FILE_NAME, &model_file_name,
			COL_FILE_INFO, &model_file_info,
			-1);
    is_dir = fm_file_info_is_dir( model_file_info );
    fm_file_info_unref ( model_file_info );
    
    return (g_str_has_prefix( model_file_name, key_file_name) && is_dir);

}

static gboolean  fm_path_entry_match_selected	(GtkEntryCompletion *widget,
						 GtkTreeModel       *model,
						 GtkTreeIter        *iter,
						 gpointer            user_data) 
{
    GtkWidget *entry = gtk_entry_completion_get_entry( widget );
    gchar new_text[PATH_MAX];
    FmPathEntryPrivate *private = FM_PATH_ENTRY_GET_PRIVATE( FM_PATH_ENTRY ( entry ) );
    gchar *model_file_name;
    gtk_tree_model_get( GTK_TREE_MODEL( model ), iter, 
			COL_FILE_NAME, &model_file_name,
			-1);
    g_sprintf( new_text, "%s/%s/", private->model_path_str, model_file_name );
    private->completion_len = 0;
    gtk_entry_set_text( GTK_ENTRY(entry), new_text );
    /* move the cursor to the end of entry */
    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
    return TRUE;
}

