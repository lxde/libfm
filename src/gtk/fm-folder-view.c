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

#include <glib/gi18n.h>

#include "fm-folder-view.h"
#include "fm-folder.h"
#include "fm-folder-model.h"
#include "fm-gtk-marshal.h"
#include "fm-cell-renderer-text.h"

#include "exo/exo-icon-view.h"
#include "exo/exo-tree-view.h"

enum{
    CLICKED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static void fm_folder_view_finalize              (GObject *object);
G_DEFINE_TYPE(FmFolderView, fm_folder_view, GTK_TYPE_SCROLLED_WINDOW);

static gboolean fm_folder_view_focus_in(GtkWidget* widget, GdkEventFocus* evt);

static GList* fm_folder_view_get_selected_tree_paths(FmFolderView* fv);

static gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmFolderView* fv);

static void on_dnd_src_data_get(FmDndSrc* ds, FmFolderView* fv);

static void on_dnd_dest_files_dropped(FmDndDest* dd, 
                                      GdkDragAction action,
                                      int info,
                                      FmPathList* files, FmFolderView* fv);

static gboolean on_dnd_dest_query_info(FmDndDest* dd, int x, int y,
									   GdkDragAction* action, FmFolderView* fv);

static void fm_folder_view_class_init(FmFolderViewClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass *widget_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_folder_view_finalize;
    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->focus_in_event = fm_folder_view_focus_in;
    fm_folder_view_parent_class = (GtkScrolledWindowClass*)g_type_class_peek(GTK_TYPE_SCROLLED_WINDOW);

    signals[ CLICKED ] =
        g_signal_new ( "clicked",
                       G_TYPE_FROM_CLASS( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmFolderViewClass, clicked ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__UINT_POINTER,
                       G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER );

}

gboolean fm_folder_view_focus_in(GtkWidget* widget, GdkEventFocus* evt)
{
    FmFolderView* fv = (FmFolderView*)widget;
    if( fv->view )
    {
        gtk_widget_grab_focus(fv->view);
        return TRUE;
    }
    return FALSE;
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
            fm_file_info_unref(fi);
        }
    }
    else
        g_signal_emit(fv, signals[CLICKED], 0, type, NULL);
}

static void on_icon_view_item_activated(ExoIconView* iv, GtkTreePath* path, FmFolderView* fv)
{
    item_clicked(fv, path, FM_FV_ACTIVATED);
}

static void on_tree_view_row_activated(GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, FmFolderView* fv)
{
    item_clicked(fv, path, FM_FV_ACTIVATED);
}

static void fm_folder_view_init(FmFolderView *self)
{
    gtk_scrolled_window_set_hadjustment(self, NULL);
    gtk_scrolled_window_set_vadjustment(self, NULL);
    gtk_scrolled_window_set_policy(self, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* dnd support */
    self->dnd_src = fm_dnd_src_new(NULL);
    g_signal_connect(self->dnd_src, "data-get", G_CALLBACK(on_dnd_src_data_get), self);

    self->dnd_dest = fm_dnd_dest_new(NULL);
    g_signal_connect(self->dnd_dest, "query-info", G_CALLBACK(on_dnd_dest_query_info), self);
    g_signal_connect(self->dnd_dest, "files-dropped", G_CALLBACK(on_dnd_dest_files_dropped), self);
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


	g_object_unref(self->dnd_src);
	g_object_unref(self->dnd_dest);

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
        GList *sels, *l, *cells;
        GtkCellRenderer* render;

        if( G_LIKELY(fv->view) )
        {
            /* preserve old selections */
            sels = fm_folder_view_get_selected_tree_paths(fv);
            gtk_widget_destroy(fv->view );
            /* FIXME: compact view and icon view actually use the same
             * type of widget, ExoIconView. So it may be better to 
             * reuse the widget when available. */
        }
        else
            sels = NULL;

        fv->mode = mode;
        switch(fv->mode)
        {
        case FM_FV_COMPACT_VIEW:
        case FM_FV_ICON_VIEW:
            fv->view = exo_icon_view_new();

            render = gtk_cell_renderer_pixbuf_new();
            g_object_set((GObject*)render, "follow-state", TRUE, NULL );
            gtk_cell_layout_pack_start((GtkCellLayout*)fv->view, render, TRUE);
            gtk_cell_layout_add_attribute((GtkCellLayout*)fv->view, render,
                                        "pixbuf", fv->mode == FM_FV_ICON_VIEW ? COL_FILE_BIG_ICON : COL_FILE_SMALL_ICON );

            render = fm_cell_renderer_text_new();
            if(fv->mode == FM_FV_COMPACT_VIEW) /* compact view */
            {
                g_object_set((GObject*)render,
                             "xalign", 0.0,
                             "yalign", 0.5,
                             NULL );
                exo_icon_view_set_layout_mode( (ExoIconView*)fv->view, EXO_ICON_VIEW_LAYOUT_COLS );
                exo_icon_view_set_orientation( (ExoIconView*)fv->view, GTK_ORIENTATION_HORIZONTAL );
            }
            else /* big icon view */
            {
                g_object_set((GObject*)render,
                             "wrap-mode", PANGO_WRAP_WORD_CHAR,
                             "wrap-width", 90,
                             "alignment", PANGO_ALIGN_CENTER,
                             "yalign", 0.0,
                             NULL );
                exo_icon_view_set_column_spacing( (ExoIconView*)fv->view, 4 );
                exo_icon_view_set_item_width ( (ExoIconView*)fv->view, 110 );
            }
            gtk_cell_layout_pack_start((GtkCellLayout*)fv->view, render, TRUE);
            gtk_cell_layout_add_attribute((GtkCellLayout*)fv->view, render,
                                        "text", COL_FILE_NAME );
            exo_icon_view_set_item_width((ExoIconView*)fv->view, 96);
            exo_icon_view_set_search_column((ExoIconView*)fv->view, COL_FILE_NAME);
            g_signal_connect(fv->view, "item-activated", G_CALLBACK(on_icon_view_item_activated), fv);
            exo_icon_view_set_model((ExoIconView*)fv->view, fv->model);
            exo_icon_view_set_selection_mode((ExoIconView*)fv->view, fv->sel_mode);
            for(l = sels;l;l=l->next)
                exo_icon_view_select_path((ExoIconView*)fv->view, (GtkTreePath*)l->data);
            break;
        case FM_FV_LIST_VIEW: /* detailed list view */
            fv->view = exo_tree_view_new();

			render = gtk_cell_renderer_pixbuf_new();
            col = gtk_tree_view_column_new();
			gtk_tree_view_column_set_title(col, _("Name"));
			gtk_tree_view_column_pack_start(col, render, FALSE);
			gtk_tree_view_column_set_attributes(col, render, "pixbuf", COL_FILE_SMALL_ICON, NULL);
			render = gtk_cell_renderer_text_new();
			g_object_set(render, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
			gtk_tree_view_column_pack_start(col, render, TRUE);
			gtk_tree_view_column_set_attributes(col, render, "text", COL_FILE_NAME, NULL);
			gtk_tree_view_column_set_sort_column_id(col, COL_FILE_NAME);
			gtk_tree_view_column_set_expand(col, TRUE);
			gtk_tree_view_column_set_resizable(col, TRUE);
			gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
            gtk_tree_view_append_column((GtkTreeView*)fv->view, col);

			render = gtk_cell_renderer_text_new();
            col = gtk_tree_view_column_new_with_attributes(_("Description"), render, "text", COL_FILE_DESC, NULL);
			gtk_tree_view_column_set_resizable(col, TRUE);
			gtk_tree_view_column_set_sort_column_id(col, COL_FILE_DESC);
            gtk_tree_view_append_column((GtkTreeView*)fv->view, col);

			render = gtk_cell_renderer_text_new();
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

		gtk_drag_source_set(fv->view, GDK_BUTTON1_MASK,
			fm_default_dnd_src_targets, N_FM_DND_SRC_DEFAULT_TARGETS,
			GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK|GDK_ACTION_ASK);
        fm_dnd_src_set_widget(fv->dnd_src, fv->view);

		gtk_drag_dest_set(fv->view, 0,
			fm_default_dnd_dest_targets, N_FM_DND_DEST_DEFAULT_TARGETS,
			GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK|GDK_ACTION_ASK);
		fm_dnd_dest_set_widget(fv->dnd_dest, fv->view);

        g_signal_connect_after(fv->view, "button-press-event", G_CALLBACK(on_btn_pressed), fv);

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

gboolean fm_folder_view_chdir(FmFolderView* fv, const char* path_str)
{
    GFile* gf;
    FmFolderModel* model;
    FmFolder* folder;

    if( G_UNLIKELY( !path_str ) )
        return FALSE;

    if(fv->model)
        g_object_unref(fv->model);

    folder = fm_folder_get_for_path(path_str);
    model = fm_folder_model_new(folder, fv->show_hidden);
    gtk_tree_sortable_set_sort_column_id(model, fv->sort_by, fv->sort_type);
    g_object_unref(folder);

    g_free(fv->cwd);
    fv->cwd = g_strdup(path_str);

    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
        gtk_tree_view_set_model(fv->view, model);
        break;
    case FM_FV_ICON_VIEW:
    case FM_FV_COMPACT_VIEW:
        exo_icon_view_set_model(fv->view, model);
        break;
    }
    fv->model = model;
    return TRUE;
}

const char* fm_folder_view_get_cwd(FmFolderView* fv)
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
        sels = exo_icon_view_get_selected_items((ExoIconView*)fv->view);
        break;
    }
    return sels;
}

FmFileInfoList* fm_folder_view_get_selected_files(FmFolderView* fv)
{
    FmFileInfoList* fis = fm_file_info_list_new();
    GList *sels = fm_folder_view_get_selected_tree_paths(fv);
    GList *l, *next;
    for(l = sels;l;l=next)
    {
        FmFileInfo* fi;
        GtkTreeIter it;
        GtkTreePath* tp = (GtkTreePath*)l->data;
        gtk_tree_model_get_iter(fv->model, &it, l->data);
        gtk_tree_model_get(fv->model, &it, COL_FILE_INFO, &fi, -1);
        gtk_tree_path_free(tp);
        next = l->next;
        l->data = fi;
        l->prev = l->next = NULL;
        fm_list_push_tail_link(fis, l);
    }
    return fis;
}

FmPathList* fm_folder_view_get_selected_file_paths(FmFolderView* fv)
{
    FmFileInfoList* files = fm_folder_view_get_selected_files(fv);
    FmPathList* list = fm_path_list_new_from_file_info_list(files);
    fm_list_unref(files);
    return list;
}

/* FIXME: is this really useful? */
guint fm_folder_view_get_n_selected_files(FmFolderView* fv)
{
    return 0;
}

gboolean on_btn_pressed(GtkWidget* view, GdkEventButton* evt, FmFolderView* fv)
{
    GList* sels;
    FmFolderViewClickType type = 0;
	/* FIXME: handle single click activation */
    if( evt->type == GDK_BUTTON_PRESS )
    {
        /* FIXME: select rows if needed */
		if(evt->button == 2) /* middle click */
			type = FM_FV_MIDDLE_CLICK;
		else if(evt->button == 3) /* right click */
			type = FM_FV_CONTEXT_MENU;
    }
    else if( evt->type == GDK_2BUTTON_PRESS )
    {
		if(evt->button == 1) /* left double click */
			type = FM_FV_ACTIVATED;
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
        exo_icon_view_select_all((ExoIconView*)fv->view);
        break;
    }
}

void on_dnd_src_data_get(FmDndSrc* ds, FmFolderView* fv)
{
    FmFileInfoList* files = fm_folder_view_get_selected_files(fv);
    fm_dnd_src_set_files(ds, files);
    fm_list_unref(files);
}

void on_dnd_dest_files_dropped(FmDndDest* dd, GdkDragAction action,
                              int info, FmPathList* files, FmFolderView* fv)
{
    g_debug("%d files-dropped!", fm_list_get_length(files));
}

gboolean on_dnd_dest_query_info(FmDndDest* dd, int x, int y,
			GdkDragAction* action, FmFolderView* fv)
{
	GtkTreePath* tp = NULL;
	gboolean droppable = TRUE;
    switch(fv->mode)
    {
    case FM_FV_LIST_VIEW:
		{
			GtkTreeViewDropPosition pos;
			GtkTreeViewColumn* col;
			if(gtk_tree_view_get_dest_row_at_pos((GtkTreeView*)fv->view, x, y,
							&tp, NULL))
			{
/*
				if(gtk_tree_view_column_get_sort_column_id(col)!=COL_FILE_NAME)
				{
					gtk_tree_path_free(tp);
					tp = NULL;
				}
*/
			}
			gtk_tree_view_set_drag_dest_row(fv->view, tp, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
			break;
		}
	case FM_FV_ICON_VIEW:
	case FM_FV_COMPACT_VIEW:
		{
			tp = exo_icon_view_get_path_at_pos(fv->view, x, y);
			exo_icon_view_set_drag_dest_item(fv->view, tp, EXO_ICON_VIEW_DROP_INTO);
			break;
		}
	}

	if(tp)
	{
		GtkTreeIter it;
		if(gtk_tree_model_get_iter(fv->model, &it, tp))
		{
			FmFileInfo* fi;
			gtk_tree_model_get(fv->model, &it, COL_FILE_INFO, &fi, -1);
			if(fi)
			{
				fm_dnd_dest_set_dest_file(dd, fi);
				fm_file_info_unref(fi);
			}
			else
				fm_dnd_dest_set_dest_file(dd, NULL);
		}
		gtk_tree_path_free(tp);
	}
	else
	{
		FmFolderModel* model = (FmFolderModel*)fv->model;
		FmPath* dir_path =  model->dir->dir_path;
		if( fm_path_is_native(dir_path) )
		{
			/*
			FmFileInfo* fi;
			fm_dnd_dest_set_dest_file(dd, fi);
			*/
			fm_dnd_dest_set_dest_file(dd, NULL);
		}
	}
	return TRUE;
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

/* select files by custom func, not yet implemented */
void fm_folder_view_custom_select(FmFolderView* fv, GFunc filter, gpointer user_data)
{
    
}

