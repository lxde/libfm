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
#include <gdk/gdkkeysyms.h>
/* properties */
enum
{
  PROP_0,
  PROP_HIGHLIGHT_COMPLETION_MATCH
};

#define FM_PATH_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FM_TYPE_PATH_ENTRY, FmPathEntryPrivate))

typedef struct _FmPathEntryPrivate FmPathEntryPrivate;

struct _FmPathEntryPrivate 
{
    /* associated with a folder model */
    FmFolderModel* model;
    /* current model used for completion */
    FmFolderModel* completion_model;
    /* Current len of the completion string */
    gint completion_len;	
    gboolean highlight_completion_match;
    GtkEntryCompletion* completion;
};

static void      fm_path_entry_activate         (GtkEntry *entry);
static gboolean	 fm_path_entry_key_press 	(GtkWidget   *widget, GdkEventKey *event);
static void 	 fm_path_entry_class_init 	(FmPathEntryClass *klass);
static void	 fm_path_entry_editable_init	(GtkEditableClass *iface);
static void 	 fm_path_entry_changed 		(GtkEditable *editable);
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
static void fm_path_entry_set_property 		(GObject *object, 
						 guint prop_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void fm_path_entry_get_property 		(GObject *object, 
						 guint prop_id,
						 GValue *value,
						 GParamSpec *pspec);

G_DEFINE_TYPE_EXTENDED (FmPathEntry, fm_path_entry, GTK_TYPE_ENTRY, 
			0, G_IMPLEMENT_INTERFACE( GTK_TYPE_EDITABLE, fm_path_entry_editable_init ) );

static GtkEditableClass *parent_editable_interface = NULL;

static gboolean fm_path_entry_key_press (GtkWidget   *widget, GdkEventKey *event) {
    FmPathEntry *entry = FM_PATH_ENTRY (widget);
    switch (event->keyval)
    {
    case GDK_Tab:
	/* complete selection */
	if (gtk_editable_get_selection_bounds (GTK_EDITABLE(widget), NULL, NULL)) {
	    gint position = strlen (gtk_entry_get_text (GTK_ENTRY (widget)));
	    gtk_editable_select_region (GTK_EDITABLE(widget), position, position);
	    return TRUE;
	}
	break;
    }
    return GTK_WIDGET_CLASS (fm_path_entry_parent_class)->key_press_event (widget, event);
}

static void  fm_path_entry_activate (GtkEntry *entry )
{
    /* Chain up so that entry->activates_default is honored */
    GTK_ENTRY_CLASS (fm_path_entry_parent_class)->activate( entry);
}

static void fm_path_entry_class_init (FmPathEntryClass *klass)
{      
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS( klass );
    GObjectClass* object_class = G_OBJECT_CLASS( klass );
    GtkEntryClass* entry_class = GTK_ENTRY_CLASS( klass );

    object_class->get_property = fm_path_entry_get_property;
    object_class->set_property = fm_path_entry_set_property;
    g_object_class_install_property (object_class,
				     PROP_HIGHLIGHT_COMPLETION_MATCH,
				     g_param_spec_boolean ("highlight-completion-match",
							   "Highlight completion match",
							   "Wheather to highlight the completion match",
							   TRUE, G_PARAM_READWRITE));       
    object_class->finalize = fm_path_entry_finalize;

    widget_class->key_press_event = fm_path_entry_key_press;
    entry_class->activate = fm_path_entry_activate;

    g_type_class_add_private (klass, sizeof (FmPathEntryPrivate));
}

static void fm_path_entry_editable_init ( GtkEditableClass *iface )
{
    parent_editable_interface = g_type_interface_peek_parent (iface);
    iface->changed = fm_path_entry_changed; 
}

static void fm_path_entry_changed (GtkEditable *editable)
{
    FmPathEntry *entry = FM_PATH_ENTRY (editable);
    FmPathEntryPrivate *private  = FM_PATH_ENTRY_GET_PRIVATE( entry );
    const gchar *original_key = gtk_entry_get_text( GTK_ENTRY(entry) ) ;
    /* len of directory part */
    gint key_dir_len;
    gchar *last_slash = strrchr( original_key, G_DIR_SEPARATOR );
    
    /* not path -> keep current completion model */
    if ( last_slash == NULL ) 
	return;
    
    /* Check if path entry is not part of current completion folder model */
    key_dir_len = last_slash - original_key;
    if (!fm_path_equal_str( private->completion_model->dir->dir_path, original_key, key_dir_len )) 
    {
	gchar* new_path = g_path_get_dirname (original_key);
	FmPath *new_fm_path = fm_path_new( new_path );
	g_free(new_path);
	if ( new_fm_path != NULL ) 
	{
	    /* set hidden parameter based on prev. model */
	    gboolean show_hidden = fm_folder_model_get_show_hidden( private->completion_model );
	    FmFolder *new_fm_folder = fm_folder_get_for_path( new_fm_path );
	    FmFolderModel *new_fm = fm_folder_model_new( new_fm_folder, show_hidden );
	    g_object_unref( private->completion_model );
	    private->completion_model = new_fm;
	    gtk_entry_completion_set_model( private->completion, GTK_TREE_MODEL(new_fm) );
	}
	else 
	{
	    /* FIXME: Handle invalid Paths */
	    g_warning( "Invalid Path: %s", new_path );
	}
    }
}

static void fm_path_entry_set_property (GObject *object, 
					guint prop_id,
					const GValue *value,
					GParamSpec *pspec)
{
    FmPathEntry *entry = FM_PATH_ENTRY (object);
    FmPathEntryPrivate *private  = FM_PATH_ENTRY_GET_PRIVATE( entry );
    
    switch (prop_id) 
    {
    case PROP_HIGHLIGHT_COMPLETION_MATCH:
	private->highlight_completion_match = g_value_get_boolean(value);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void fm_path_entry_get_property (GObject *object, 
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
    FmPathEntry *entry = FM_PATH_ENTRY (object);
    FmPathEntryPrivate *private  = FM_PATH_ENTRY_GET_PRIVATE( entry );
    
    switch (prop_id) {
    case PROP_HIGHLIGHT_COMPLETION_MATCH:
	g_value_set_boolean( value, private->highlight_completion_match );
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
fm_path_entry_init (FmPathEntry *entry)
{
    FmPathEntryPrivate *private = FM_PATH_ENTRY_GET_PRIVATE(entry);
    GtkEntryCompletion* completion = gtk_entry_completion_new();
    GtkCellRenderer* render;

    private->model = NULL;
    private->completion_model = NULL;
    private->completion_len = 0;
    private->completion = completion;
    private->highlight_completion_match = TRUE;
    gtk_entry_completion_set_minimum_key_length( completion, 1 );
    gtk_entry_completion_set_match_func( completion, fm_path_entry_match_func, NULL, NULL );
    g_signal_connect(G_OBJECT (completion), "match-selected", G_CALLBACK(fm_path_entry_match_selected), (gpointer)  NULL);
    g_object_set( completion, "text-column", COL_FILE_NAME, NULL );
    render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( (GtkCellLayout*)completion, render, TRUE );
    gtk_cell_layout_set_cell_data_func( GTK_CELL_LAYOUT(completion), render, fm_path_entry_completion_render_func, entry, NULL );
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

    gchar *model_file_name;
    FmPathEntryPrivate *private = FM_PATH_ENTRY_GET_PRIVATE( FM_PATH_ENTRY ( data ) );
    gtk_tree_model_get( GTK_TREE_MODEL( model ), iter, 
			COL_FILE_NAME, &model_file_name, -1 );
    if (private->highlight_completion_match) 
    {
	gchar markup[PATH_MAX];
	gchar *trail = g_stpcpy( markup, "<b><u>" );
	trail = strncpy( trail, model_file_name, private->completion_len ) + private->completion_len;
	trail = g_stpcpy( trail, "</u></b>" );
	trail = g_stpcpy( trail, model_file_name + private->completion_len );
	g_object_set(cell, "markup", markup, NULL );
    }
    /* FIXME: We don't need a custom render func if we don't hightlight */
    else
	g_object_set(cell, "text", model_file_name, NULL);
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
    gchar *path_str = fm_path_to_str( model->dir->dir_path );
    if (private->model)  
	g_object_unref ( G_OBJECT(private->model) );    
    private->model = g_object_ref( model );
    private->completion_model = g_object_ref ( model );
    gtk_entry_completion_set_model( private->completion, GTK_TREE_MODEL(private->completion_model) );
    gtk_entry_set_text(GTK_ENTRY(entry), path_str);
    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
    g_free( path_str );
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
    gboolean is_dir;
    /* find sep in key */
    gchar *key_last_slash = strrchr( original_key, G_DIR_SEPARATOR );

    /* no model based completion possible */
    if ((!model) || ( key_last_slash == NULL) )
	return FALSE;
    
    private->completion_len = strlen( key_last_slash + 1 );
    
    /* get filename, info from model */
    gtk_tree_model_get( GTK_TREE_MODEL( model ), iter, 
			COL_FILE_NAME, &model_file_name,
			COL_FILE_INFO, &model_file_info,
			-1);
	
    return fm_file_info_is_dir( model_file_info ) && g_str_has_prefix( model_file_name, key_last_slash + 1);
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
    gchar *new_path;
    gtk_tree_model_get( GTK_TREE_MODEL( model ), iter, 
			COL_FILE_NAME, &model_file_name,
			-1);
    new_path = fm_path_to_str( private->completion_model->dir->dir_path );
    g_sprintf( new_text, "%s/%s", 
	       /* prevent leading double slash */
	       g_str_equal( new_path, "/" )? "":new_path,
	       model_file_name );
    g_free( new_path );
    private->completion_len = 0;
    gtk_entry_set_text( GTK_ENTRY(entry), new_text );
    /* move the cursor to the end of entry */
    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
    return TRUE;
}

