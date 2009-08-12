/*
 *      fm-dnd-dest.c
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

#include "fm-dnd-dest.h"
#include "fm-gtk-marshal.h"

#include <string.h>

struct _FmDndDest
{
	GObject parent;
	GtkWidget* widget;
	guint scroll_timeout;
	int info_type; /* type of src_files */
	FmList* src_files;
    gboolean src_ready; /* whether src_files are retrived */
	guint32 src_dev; /* UNIX dev of source fs */
	const char* src_fs_id; /* filesystem id of source fs */
	FmFileInfo* dest_file;
};

enum
{
    QUERY_INFO,
    FILES_DROPPED,
    N_SIGNALS
};

GtkTargetEntry fm_default_dnd_dest_targets[] = 
{
    {"application/x-fmlist-ptr", GTK_TARGET_SAME_APP, FM_DND_DEST_TARGET_FM_LIST},
    {"text/uri-list", 0, FM_DND_DEST_TARGET_URI_LIST}, /* text/uri-list */
    { "XdndDirectSave0", 0, FM_DND_DEST_TARGET_XDS, } /* X direct save */
};


static void fm_dnd_dest_finalize              (GObject *object);
static gboolean fm_dnd_dest_query_info(FmDndDest* dd, int x, int y, int* suggested_action);

static gboolean
on_drag_motion ( GtkWidget *dest_widget,
                 GdkDragContext *drag_context,
                 gint x,
                 gint y,
                 guint time,
                 FmDndDest* dd );

static gboolean
on_drag_leave ( GtkWidget *dest_widget,
                GdkDragContext *drag_context,
                guint time,
                FmDndDest* dd );

static gboolean
on_drag_drop ( GtkWidget *dest_widget,
               GdkDragContext *drag_context,
               gint x,
               gint y,
               guint time,
               FmDndDest* dd );

static void
on_drag_data_received ( GtkWidget *dest_widget,
                        GdkDragContext *drag_context,
                        gint x,
                        gint y,
                        GtkSelectionData *sel_data,
                        guint info,
                        guint time,
                        FmDndDest* dd );

static guint signals[N_SIGNALS];


G_DEFINE_TYPE(FmDndDest, fm_dnd_dest, G_TYPE_OBJECT);


static void fm_dnd_dest_class_init(FmDndDestClass *klass)
{
    GObjectClass *g_object_class;
    FmDndDestClass *dnd_dest_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_dnd_dest_finalize;

    dnd_dest_class = FM_DND_DEST_CLASS(klass);
	dnd_dest_class->query_info = fm_dnd_dest_query_info;

    /* emitted when information of drop site is needed.
     * call fm_dnd_set_droppable() in its callback to
     * tell FmDnd whether current location is droppable
     * for source files, and update UI if needed. */
    signals[ QUERY_INFO ] =
        g_signal_new ( "query-info",
                       G_TYPE_FROM_CLASS( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( FmDndDestClass, query_info ),
                       g_signal_accumulator_true_handled, NULL,
                       fm_marshal_BOOL__UINT_UINT_POINTER,
                       G_TYPE_BOOLEAN, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER );

    /* emitted when files are dropped on dest widget. */
    signals[ FILES_DROPPED ] =
        g_signal_new ( "files-dropped",
                       G_TYPE_FROM_CLASS( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( FmDndDestClass, files_dropped ),
                       NULL, NULL,
                       fm_marshal_VOID__UINT_UINT_POINTER,
                       G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER );
}


static void fm_dnd_dest_finalize(GObject *object)
{
    FmDndDest *dd;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_DND_DEST(object));

    dd = FM_DND_DEST(object);

    fm_dnd_dest_set_widget(dd, NULL);

	if(dd->dest_file)
		fm_file_info_unref(dd->dest_file);

	if(dd->src_files)
		fm_list_unref(dd->src_files);

    G_OBJECT_CLASS(fm_dnd_dest_parent_class)->finalize(object);
}


static void fm_dnd_dest_init(FmDndDest *self)
{
    
}


FmDndDest *fm_dnd_dest_new(GtkWidget* w)
{
    FmDndDest* dd = (FmDndDest*)g_object_new(FM_TYPE_DND_DEST, NULL);
    fm_dnd_dest_set_widget(dd, w);
    return dd;
}

void fm_dnd_dest_set_widget(FmDndDest* dd, GtkWidget* w)
{
    if(w == dd->widget)
        return;
    if(dd->widget) /* there is an old widget connected */
    {
        g_signal_handlers_disconnect_by_func(dd->widget, on_drag_motion, dd);
        g_signal_handlers_disconnect_by_func(dd->widget, on_drag_leave, dd);
        g_signal_handlers_disconnect_by_func(dd->widget, on_drag_drop, dd);
        g_signal_handlers_disconnect_by_func(dd->widget, on_drag_data_received, dd);
    }
    dd->widget = w;
    if( w )
    {
		g_object_add_weak_pointer(w, &dd->widget);
        g_signal_connect_after(w, "drag-motion", G_CALLBACK(on_drag_motion), dd);
        g_signal_connect(w, "drag-leave", G_CALLBACK(on_drag_leave), dd);
        g_signal_connect(w, "drag-drop", G_CALLBACK(on_drag_drop), dd );
        g_signal_connect(w, "drag-data-received", G_CALLBACK(on_drag_data_received), dd);
    }
}

gboolean fm_dnd_dest_query_info(FmDndDest* dd, int x, int y, int* suggested_action)
{
	return TRUE;
}

static gboolean
on_drag_motion( GtkWidget *dest_widget,
                GdkDragContext *drag_context,
                gint x,
                gint y,
                guint time,
                FmDndDest* dd )
{
	GdkDragAction action;
	gboolean ret;

	/* cache drag source files */
	if( G_UNLIKELY(!dd->src_files) )
	{
		GdkAtom target;
		target = gtk_drag_dest_find_target( dest_widget, drag_context, NULL );
		if( target != GDK_NONE )
		{
			gtk_drag_get_data(dest_widget, drag_context, target, time);
            /* run the main loop to block here waiting for
             * 'drag-data-received' single being handled first. */
            while(!dd->src_ready)
                gtk_main_iteration_do(TRUE);

			/* dd->src_files should be set now */
            if( dd->src_files && fm_list_is_file_info_list(dd->src_files) )
            {
                /* cache file system id of source files */
                if( fm_file_info_list_is_same_fs(dd->src_files) )
                {
                    FmFileInfo* fi = (FmFileInfo*)fm_list_peek_head(dd->src_files);
                    if(fm_path_is_native(fi->path))
                        dd->src_dev = fi->dev;
                    else
                        dd->src_fs_id = fi->fs_id;
                }
            }
		}
	}
    if( !dd->src_files )
        return FALSE;

	action = drag_context->suggested_action;
    g_signal_emit(dd, signals[QUERY_INFO], 0, x, y, &action, &ret);

	if( dd->dest_file ) /* if info of destination path is available */
	{
		FmFileInfo* fi = (FmFileInfo*)fm_list_peek_head(dd->src_files);
		FmPath* path = dd->dest_file->path;
		gboolean same_fs;

#if 0
		/* FIXME: computer:// and network:// shouldn't received dnd */
		if(fm_path_is_trash(path)) /* only move is allowed for trash */
			action = GDK_ACTION_MOVE;
		else if(fm_path_is_computer(path))
			action = 0;
		else if(fm_path_is_network(path))
			action = 0;
		else
#endif
		{
			if(fm_path_is_native(path))
				same_fs = (fi->dev == dd->dest_file->dev);
			else
				same_fs = (fi->fs_id == dd->dest_file->fs_id);

			if( same_fs )
				action = GDK_ACTION_MOVE;
			else
				action = GDK_ACTION_COPY;
		}
	}

	gdk_drag_status(drag_context, ret ? action : 0, time);
    return ret;
}

static gboolean
on_drag_leave ( GtkWidget *dest_widget,
                GdkDragContext *drag_context,
                guint time,
                FmDndDest* dd )
{
    /*  Don't call the default handler  */
	gtk_drag_unhighlight(dest_widget);
}

static gboolean
on_drag_drop ( GtkWidget *dest_widget,
               GdkDragContext *drag_context,
               gint x,
               gint y,
               guint time,
               FmDndDest* dd )
{
    GdkAtom target;
    target = gtk_drag_dest_find_target( dest_widget, drag_context, NULL );

	if( target == GDK_NONE )
		return FALSE;

	/* if it's XDS */
	if( target == gdk_atom_intern_static_string(fm_default_dnd_dest_targets[FM_DND_DEST_TARGET_XDS].target) )
	{
        g_debug("XDS is not yet implemented");
		gtk_drag_finish( drag_context, FALSE, FALSE, time );
		return TRUE;
	}

	gtk_drag_finish( drag_context, TRUE, FALSE, time );

	/* free cached source files */
	if(dd->src_files)
	{
        g_signal_emit(dd, signals[FILES_DROPPED], 0, drag_context->action, dd->info_type, dd->src_files);
		fm_list_unref(dd->src_files);
		dd->src_files = NULL;
	}
    dd->info_type = 0;
    return TRUE;
}

static void
on_drag_data_received ( GtkWidget *dest_widget,
                        GdkDragContext *drag_context,
                        gint x,
                        gint y,
                        GtkSelectionData *sel_data,
                        guint info,
                        guint time,
                        FmDndDest* dd )
{
    FmList* files = NULL;
    switch(info)
    {
    case FM_DND_DEST_TARGET_FM_LIST:
		if((sel_data->length >= 0) && (sel_data->format==8))
		{
			/* get the pointer */
			memcpy(&files, sel_data->data, sel_data->length);
			if(!files)
				break;
			fm_list_ref(files);
		}
        break;
    case FM_DND_DEST_TARGET_URI_LIST:
        if((sel_data->length >= 0) && (sel_data->format==8))
        {
            gchar **uris;
            uris = gtk_selection_data_get_uris( sel_data );
			files = fm_path_list_new_from_uris(uris);
            g_free(uris);
        }
        break;
    case FM_DND_DEST_TARGET_XDS:
        /* FIXME: support XDS in the future. */

		return;
    }
	if(G_UNLIKELY(dd->src_files))
		fm_list_unref(dd->src_files);
    dd->info_type = info;
	dd->src_files = files;
    dd->src_ready = TRUE;
}

/* the returned list can be either FmPathList or FmFileInfoList */
/* check with fm_list_is_path_list() and fm_list_is_file_info_list(). */
FmList* fm_dnd_dest_get_src_files(FmDndDest* dd)
{
	return dd->src_files;
}

FmFileInfo* fm_dnd_dest_get_dest_file(FmDndDest* dd)
{
    return dd->dest_file;
}

FmPath* fm_dnd_dest_get_dest_path(FmDndDest* dd)
{
    return dd->dest_file ? dd->dest_file->path : NULL;
}


#if 0
gboolean query_info(FmDndDest* dd, GdkDragContext* drag_context,
					int x, int y, FmFileInfo** dest, GdkDragAction* action)
{
	GdkDragAction action;
	FmFileInfo* dest = NULL;
	gboolean ret;

	/* cache drag source */
	if( G_UNLIKELY(!dd->src_files) )
	{
		GdkAtom target;
		target = gtk_drag_dest_find_target( dest_widget, drag_context, NULL );
		if( target != GDK_NONE )
		{
			gtk_drag_get_data(dest_widget, drag_context, target, time);
			/* dd->src_files should be set now */
		}
	}

	action = drag_context->suggested_action;
    g_signal_emit(dd, signals[QUERY_INFO], 0, x, y, &dest, &action, &ret);
#if 0
	if( dest ) /* if info of destination path is available */
	{
		if(fm_list_is_file_info_list(dd->src_files))
		{
			FmPath* path = dest->path;

			/* only move is allowed when dest is trash. */
			if(fm_path_is_trash(path))
			{
				*action = GDK_ACTION_MOVE;
				drag_context->actions = GDK_ACTION_MOVE;
			}
			
			fm_file_info_unref(dest);
		}
		else
			*action = GDK_ACTION_ASK;
	}
#endif
	return ;
}
#endif

void fm_dnd_dest_set_dest_file(FmDndDest* dd, FmFileInfo* dest_file)
{
	if(dd->dest_file)
		fm_file_info_unref(dd->dest_file);
	dd->dest_file = dest_file ? fm_file_info_ref(dest_file) : NULL;
}
