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

#include "fm-folder-view.h"

#include "fm-folder.h"
#include "fm-folder-model.h"

enum{
	FILE_CLICKED,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

static void fm_folder_view_finalize  			(GObject *object);
G_DEFINE_TYPE(FmFolderView, fm_folder_view, GTK_TYPE_SCROLLED_WINDOW);

static void fm_folder_view_class_init(FmFolderViewClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_folder_view_finalize;
	fm_folder_view_parent_class = (GtkScrolledWindowClass*)g_type_class_peek(GTK_TYPE_SCROLLED_WINDOW);

    signals[ FILE_CLICKED ] =
        g_signal_new ( "file-clicked",
                       G_TYPE_FROM_CLASS( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderViewClass, file_clicked ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__UINT_POINTER,
                       G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER );

}

static void item_clicked( FmFolderView* fv, GtkTreePath* path, int btn )
{
	GtkTreeIter it;
	if(gtk_tree_model_get_iter(fv->model, &it, path))
	{
		FmFileInfo* fi;
		gtk_tree_model_get(fv->model, &it, COL_FILE_INFO, &fi, -1);
		g_signal_emit(fv, signals[FILE_CLICKED], 0, btn, fi);
		fm_file_info_unref(fi);
	}
}

static void on_icon_view_item_activated(GtkIconView* iv, GtkTreePath* path, FmFolderView* fv)
{
	item_clicked(fv, path, GDK_2BUTTON_PRESS);
}

static void on_tree_view_row_activated(GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, FmFolderView* fv)
{
	item_clicked(fv, path, GDK_2BUTTON_PRESS);
}

static void fm_folder_view_init(FmFolderView *self)
{
	gtk_scrolled_window_set_hadjustment(self, NULL);
	gtk_scrolled_window_set_vadjustment(self, NULL);
	gtk_scrolled_window_set_policy(self, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
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
	if( self->model )
		g_object_unref(self->model);

	g_free(self->cwd);

	if (G_OBJECT_CLASS(fm_folder_view_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_folder_view_parent_class)->finalize)(object);
}


void fm_folder_view_set_mode(FmFolderView* fv, FmFolderViewMode mode)
{
	GtkTreeViewColumn* col;
	g_debug("old = %d, new=%d", fv->mode, mode);
	if( mode != fv->mode )
	{
		fv->mode = mode;
		if( G_LIKELY(fv->view) )
			gtk_widget_destroy(fv->view );
		switch(fv->mode)
		{
		case FM_FV_COMPACT_VIEW:
		case FM_FV_ICON_VIEW:
			fv->view = gtk_icon_view_new();
			gtk_icon_view_set_pixbuf_column(fv->view, COL_FILE_BIG_ICON);
			gtk_icon_view_set_text_column(fv->view, COL_FILE_NAME);
			gtk_icon_view_set_item_width(fv->view, 96);
			g_signal_connect(fv->view, "item-activated", G_CALLBACK(on_icon_view_item_activated), fv);
			gtk_icon_view_set_model(fv->view, fv->model);
			break;
		case FM_FV_LIST_VIEW:
			fv->view = gtk_tree_view_new();
			col = gtk_tree_view_column_new_with_attributes(NULL, gtk_cell_renderer_pixbuf_new(), "gicon", COL_FILE_GICON, NULL);
			gtk_tree_view_append_column(fv->view, col);
			col = gtk_tree_view_column_new_with_attributes("Name", gtk_cell_renderer_text_new(), "text", COL_FILE_NAME, NULL);
			gtk_tree_view_append_column(fv->view, col);
			g_signal_connect(fv->view, "row-activated", G_CALLBACK(on_tree_view_row_activated), fv);
			gtk_tree_view_set_model(fv->view, fv->model);
			break;
		}
		fm_folder_view_set_selection_mode(fv, fv->sel_mode);
		gtk_widget_show(fv->view);
		gtk_container_add(fv, fv->view);
	}
	else
		g_debug("same mode");
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
			gtk_icon_view_set_selection_mode((GtkIconView*)fv->view, mode);
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
		/* re-filter the model */
	}
}

gboolean fm_folder_view_get_show_hidden(FmFolderView* fv)
{
	return fv->show_hidden;
}

gboolean fm_folder_view_chdir(FmFolderView* fv, const char* path)
{
	GFile* gf;
	if( G_UNLIKELY( !path ) )
		return FALSE;
	if( path[0] == '/' )
		gf = g_file_new_for_path(path);
	else
		gf = g_file_new_for_uri(path);

	FmFolder* folder = fm_folder_new(gf);
	FmFolderModel* model = fm_folder_model_new(folder, FALSE);
	g_object_unref(folder);
	g_object_unref(gf);

	g_free(fv->cwd);
	fv->cwd = g_strdup(path);

	switch(fv->mode)
	{
	case FM_FV_LIST_VIEW:
		gtk_tree_view_set_model(fv->view, model);
		break;
	case FM_FV_ICON_VIEW:
	case FM_FV_COMPACT_VIEW:
		gtk_icon_view_set_model(fv->view, model);
		break;
	}
	fv->model = model;
	return TRUE;
}

const char* fm_folder_view_get_cwd(FmFolderView* fv)
{
	
}

GList* fm_folder_get_selected(FmFolderView* fv)
{
	
}

guint fm_folder_get_n_selected(FmFolderView* fv)
{
	return 0;
}

