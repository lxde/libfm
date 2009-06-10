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
#include "fm-gtk-marshal.h"
#include "fm-gtk-marshal.h"

enum{
	CLICKED,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

static void fm_folder_view_finalize  			(GObject *object);
G_DEFINE_TYPE(FmFolderView, fm_folder_view, GTK_TYPE_SCROLLED_WINDOW);

static GList* fm_folder_get_selected_tree_paths(FmFolderView* fv);

static gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmFolderView* fv);

static void fm_folder_view_class_init(FmFolderViewClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_folder_view_finalize;
	fm_folder_view_parent_class = (GtkScrolledWindowClass*)g_type_class_peek(GTK_TYPE_SCROLLED_WINDOW);

    signals[ CLICKED ] =
        g_signal_new ( "clicked",
                       G_TYPE_FROM_CLASS( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderViewClass, file_clicked ),
                       NULL, NULL,
                       fm_marshal_VOID__POINTER_UINT_UINT,
                       G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT );

}

static void item_clicked( FmFolderView* fv, GtkTreePath* path, guint type, guint btn )
{
	GtkTreeIter it;
	if(path)
	{
		if(gtk_tree_model_get_iter(fv->model, &it, path))
		{
			FmFileInfo* fi;
			gtk_tree_model_get(fv->model, &it, COL_FILE_INFO, &fi, -1);
			g_signal_emit(fv, signals[CLICKED], 0, fi, type, btn);
			fm_file_info_unref(fi);
		}
	}
	else
		g_signal_emit(fv, signals[CLICKED], 0, NULL, type, btn);
}

static void on_icon_view_item_activated(GtkIconView* iv, GtkTreePath* path, FmFolderView* fv)
{
	item_clicked(fv, path, GDK_2BUTTON_PRESS, 1);
}

static void on_tree_view_row_activated(GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, FmFolderView* fv)
{
	item_clicked(fv, path, GDK_2BUTTON_PRESS, 1);
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
	if( mode != fv->mode )
	{
		GtkTreeViewColumn* col;
		GtkTreeSelection* ts;
		GList *sels, *l;

		if( G_LIKELY(fv->view) )
		{
			/* preserve old selections */
			sels = fm_folder_get_selected_tree_paths(fv);
			gtk_widget_destroy(fv->view );
		}
		else
			sels = NULL;

		fv->mode = mode;
		switch(fv->mode)
		{
		case FM_FV_COMPACT_VIEW:
		case FM_FV_ICON_VIEW:
			fv->view = gtk_icon_view_new();
			gtk_icon_view_set_pixbuf_column((GtkIconView*)fv->view, COL_FILE_BIG_ICON);
			gtk_icon_view_set_text_column((GtkIconView*)fv->view, COL_FILE_NAME);
			gtk_icon_view_set_item_width((GtkIconView*)fv->view, 96);
			g_signal_connect(fv->view, "item-activated", G_CALLBACK(on_icon_view_item_activated), fv);
			gtk_icon_view_set_model((GtkIconView*)fv->view, fv->model);
			gtk_icon_view_set_selection_mode((GtkIconView*)fv->view, fv->sel_mode);
			for(l = sels;l;l=l->next)
				gtk_icon_view_select_path((GtkIconView*)fv->view, (GtkTreePath*)l->data);
			break;
		case FM_FV_LIST_VIEW:
			fv->view = gtk_tree_view_new();
			col = gtk_tree_view_column_new_with_attributes(NULL, gtk_cell_renderer_pixbuf_new(), "gicon", COL_FILE_GICON, NULL);
			gtk_tree_view_append_column((GtkTreeView*)fv->view, col);
			col = gtk_tree_view_column_new_with_attributes("Name", gtk_cell_renderer_text_new(), "text", COL_FILE_NAME, NULL);
			gtk_tree_view_append_column((GtkTreeView*)fv->view, col);
			g_signal_connect(fv->view, "row-activated", G_CALLBACK(on_tree_view_row_activated), fv);
			gtk_tree_view_set_model((GtkTreeView*)fv->view, fv->model);
			ts = gtk_tree_view_get_selection((GtkTreeView*)fv->view);
			gtk_tree_selection_set_mode(ts, fv->sel_mode);
			for(l = sels;l;l=l->next)
				gtk_tree_selection_select_path(ts, (GtkTreePath*)l->data);
			break;
		}
		g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
		g_list_free(sels);

		g_signal_connect(fv->view, "button-press-event", G_CALLBACK(on_btn_pressed), fv);

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
	if(type >=0)
		fv->sort_type = type;
	if(by >=0)
		fv->sort_by = by;
	if(fv->model)
		gtk_tree_sortable_set_sort_column_id(fv->model, fv->sort_by, fv->sort_type);
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
		fm_folder_model_set_show_hidden(fv->model, show);
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
	FmFolderModel* model = fm_folder_model_new(folder, fv->show_hidden);
	gtk_tree_sortable_set_sort_column_id(model, fv->sort_by, fv->sort_type);
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
	return fv->cwd;
}

GList* fm_folder_get_selected_tree_paths(FmFolderView* fv)
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
		sels = gtk_icon_view_get_selected_items((GtkIconView*)fv->view);
		break;
	}
	return sels;
}

GList* fm_folder_get_selected_files(FmFolderView* fv)
{
	GList *l, *sels = fm_folder_get_selected_tree_paths(fv);
	for(l = sels;l;l=l->next)
	{
		FmFileInfo* fi;
		GtkTreeIter it;
		GtkTreePath* tp = (GtkTreePath*)l->data;
		gtk_tree_model_get_iter(fv->model, &it, l->data);
		gtk_tree_model_get(fv->model, &it, COL_FILE_INFO, &fi, -1);
		gtk_tree_path_free(tp);
		l->data = fi;
	}
	return sels;
}

/* FIXME: is this really useful? */
guint fm_folder_get_n_selected_files(FmFolderView* fv)
{
	return 0;
}

gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmFolderView* fv)
{
	GList* sels = fm_folder_get_selected_tree_paths(fv);
	if( evt->type == GDK_2BUTTON_PRESS )
	{
		if(sels)
			goto _out;
	}
	item_clicked(fv, sels ? sels->data : NULL, evt->type, evt->button);
_out:
	g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(sels);
	return FALSE;
}

