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
#include <gio/gio.h>
#include <string.h>

/* treestore columns */
enum
{
    COL_NAME,
    COL_PATH,
    N_COLS
};


#define FM_PATH_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FM_TYPE_PATH_ENTRY, FmPathEntryPrivate))

typedef struct _FmPathEntryPrivate FmPathEntryPrivate;

struct _FmPathEntryPrivate 
{
    GtkEntryCompletion* ec;
    GFilenameCompleter* fc;
};

G_DEFINE_TYPE (FmPathEntry, fm_path_entry, GTK_TYPE_ENTRY)

static void on_got_completion_data(GFilenameCompleter* fc, FmPathEntry* entry);

static void update_completion(FmPathEntry* pe, gboolean clear_old)
{
    FmPathEntryPrivate *private = FM_PATH_ENTRY_GET_PRIVATE(pe);
    GtkListStore* list = (GtkListStore*)gtk_entry_completion_get_model(private->ec);
    gchar** fns = g_filename_completer_get_completions(private->fc, gtk_entry_get_text(GTK_ENTRY(pe)));
    gchar** fn;

    if(clear_old)
        gtk_list_store_clear(list);

    if(fns)
    {
        for(fn=fns; *fn; ++fn)
        {
            GtkTreeIter it;
            char* basename = g_path_get_basename(*fn);
            gtk_list_store_append(list, &it);
            gtk_list_store_set(list, &it, COL_NAME, basename, COL_PATH, *fn, -1);
            g_free(basename);
        }
        g_strfreev(fns);
    }
}

static void on_changed( GtkEntry* entry, gpointer user_data )
{
    FmPathEntryPrivate* private = FM_PATH_ENTRY_GET_PRIVATE( entry );
		       
    if(private->ec)
        update_completion(FM_PATH_ENTRY(entry), TRUE);
}

static gboolean fm_path_entry_focus_in( GtkWidget *entry, GdkEventFocus* evt)
{
#if GLIB_CHECK_VERSION(2,20,4)
    FmPathEntry* pe = FM_PATH_ENTRY(entry);
    FmPathEntryPrivate* private = FM_PATH_ENTRY_GET_PRIVATE(entry);
    GtkEntryCompletion* completion = gtk_entry_completion_new();
    GtkListStore* list = gtk_list_store_new( N_COLS, G_TYPE_STRING, G_TYPE_STRING );
    GtkCellRenderer* render;

    gtk_entry_completion_set_minimum_key_length( completion, 1 );
    gtk_entry_completion_set_model( completion, GTK_TREE_MODEL(list) );
    g_object_unref( list );

    /* gtk_entry_completion_set_text_column( completion, COL_PATH ); */
    g_object_set( completion, "text-column", COL_PATH, NULL );
    render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( (GtkCellLayout*)completion, render, TRUE );
    gtk_cell_layout_add_attribute( (GtkCellLayout*)completion, render, "text", COL_NAME );

    gtk_entry_completion_set_inline_completion( completion, TRUE );
    gtk_entry_completion_set_popup_set_width( completion, TRUE );

    gtk_entry_set_completion( GTK_ENTRY(entry), completion );
    g_signal_connect( G_OBJECT(entry), "changed", G_CALLBACK(on_changed), pe );
    g_object_unref( completion );


    private->ec = completion;
    private->fc = g_filename_completer_new();
    g_signal_connect(private->fc, "got-completion-data", G_CALLBACK(on_got_completion_data), pe);
    g_filename_completer_set_dirs_only(private->fc, TRUE);
#endif
    return GTK_WIDGET_CLASS(fm_path_entry_parent_class)->focus_in_event(entry, evt);
}

static gboolean fm_path_entry_focus_out( GtkWidget *entry, GdkEventFocus* evt)
{
#if GLIB_CHECK_VERSION(2,20,4)
    FmPathEntry* pe = FM_PATH_ENTRY(entry);
    FmPathEntryPrivate* private = FM_PATH_ENTRY_GET_PRIVATE(entry );
    
    g_signal_handlers_disconnect_by_func( entry, on_changed, NULL );
    gtk_entry_set_completion( GTK_ENTRY(entry), NULL );

    private->ec = NULL;
    g_object_unref(private->fc);
    private->fc = NULL;
#endif    
    return GTK_WIDGET_CLASS(fm_path_entry_parent_class)->focus_out_event(entry, evt);
}

static void path_entry_free(FmPathEntry* pe)
{
    g_slice_free(FmPathEntry, pe);
}

static void fm_path_entry_class_init (FmPathEntryClass *klass)
{      
    GtkWidgetClass* widget_class = (GtkWidgetClass*) klass;

    widget_class->focus_in_event = fm_path_entry_focus_in;
    widget_class->focus_out_event = fm_path_entry_focus_out;

    g_type_class_add_private (klass, sizeof (FmPathEntryPrivate));
}

static void
fm_path_entry_init (FmPathEntry *entry)
{

}


GtkWidget* fm_path_entry_new()
{
    return GTK_WIDGET(g_object_new(fm_path_entry_get_type(), NULL));
}

void on_got_completion_data(GFilenameCompleter* fc, FmPathEntry* pe)
{
    update_completion(pe, FALSE);
}
