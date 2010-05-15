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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-dnd-dest.h"
#include "fm-gtk-utils.h"
#include "fm-gtk-marshal.h"
#include "fm-file-info-job.h"

#include <glib/gi18n-lib.h>
#include <string.h>

struct _FmDndDest
{
	GObject parent;
	GtkWidget* widget;

	int info_type; /* type of src_files */
	FmList* src_files;
	guint32 src_dev; /* UNIX dev of source fs */
	const char* src_fs_id; /* filesystem id of source fs */
	FmFileInfo* dest_file;
    guint idle; /* idle handler */
    GMainLoop* mainloop; /* used to block when retriving data */

    guint scroll_timeout; /* auto scroll on dnd */
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

#define SCROLL_EDGE_SIZE 15

static void fm_dnd_dest_finalize              (GObject *object);
static gboolean fm_dnd_dest_query_info(FmDndDest* dd, int x, int y, int* action);
static gboolean fm_dnd_dest_files_dropped(FmDndDest* dd, GdkDragAction action, int info_type, FmList* files);

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

static gboolean clear_src_cache(FmDndDest* dest);

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
    dnd_dest_class->files_dropped = fm_dnd_dest_files_dropped;

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
                       g_signal_accumulator_true_handled, NULL,
                       fm_marshal_BOOL__UINT_UINT_POINTER,
                       G_TYPE_BOOLEAN, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER );
}


static void fm_dnd_dest_finalize(GObject *object)
{
    FmDndDest *dd;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_DND_DEST(object));

    dd = FM_DND_DEST(object);

    fm_dnd_dest_set_widget(dd, NULL);

    if(dd->idle)
        g_source_remove(dd->idle);

	if(dd->dest_file)
		fm_file_info_unref(dd->dest_file);

	if(dd->src_files)
		fm_list_unref(dd->src_files);

    if(dd->mainloop)
        g_main_loop_unref(dd->mainloop);

    if(dd->scroll_timeout)
        g_source_remove(dd->scroll_timeout);

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
		g_object_add_weak_pointer(G_OBJECT(w), &dd->widget);
        g_signal_connect_after(w, "drag-motion", G_CALLBACK(on_drag_motion), dd);
        g_signal_connect(w, "drag-leave", G_CALLBACK(on_drag_leave), dd);
        g_signal_connect(w, "drag-drop", G_CALLBACK(on_drag_drop), dd );
        g_signal_connect(w, "drag-data-received", G_CALLBACK(on_drag_data_received), dd);
    }
}

gboolean fm_dnd_dest_query_info(FmDndDest* dd, int x, int y, int* action)
{
	if( dd->dest_file ) /* if info of destination path is available */
	{
        /* FIXME: src_files might not be a FmFileInfoList, but FmPathList. */
		FmFileInfo* fi = (FmFileInfo*)fm_list_peek_head(dd->src_files);
		FmPath* path = dd->dest_file->path;
		gboolean same_fs;
        if(fm_path_is_trash(path))
        {
            if(fm_path_is_trash_root(path)) /* only move is allowed for trash */
                *action = GDK_ACTION_MOVE;
            else
                *action = 0;
        }
        /* FIXME: this seems to have some problems sometimes. */
        else if(fm_path_is_virtual(path))
        {
    		/* FIXME: computer:// and network:// shouldn't received dnd */
            /* FIXME: some special handling can be done with menu:// */
            action = 0;
        }
        else
		{
            if(dd->src_dev || dd->src_fs_id)
            {
                if(fm_path_is_native(path))
                    same_fs = dd->src_dev && (dd->src_dev == dd->dest_file->dev);
                else /* FIXME: can we use direct comparison here? */
                    same_fs = dd->src_fs_id && (0 == g_strcmp0(dd->src_fs_id, dd->dest_file->fs_id));
            }
            else
                same_fs = FALSE;
			if( same_fs )
				*action = GDK_ACTION_MOVE;
			else
				*action = GDK_ACTION_COPY;
		}
	}
	return TRUE;
}

gboolean fm_dnd_dest_files_dropped(FmDndDest* dd, GdkDragAction action,
                                        int info_type, FmList* files)
{
	FmPath* dest;
	dest = fm_dnd_dest_get_dest_path(dd);
	if(!dest)
		return FALSE;
    g_debug("%d files-dropped!, info_type: %d", fm_list_get_length(files), info_type);

    if(fm_list_is_file_info_list(files))
        files = fm_path_list_new_from_file_info_list(files);
    else
        fm_list_ref(files);

    switch(action)
    {
    case GDK_ACTION_MOVE:
        if(fm_path_is_trash_root(fm_dnd_dest_get_dest_path(dd)))
            fm_trash_files(files);
        else
            fm_move_files(files, fm_dnd_dest_get_dest_path(dd));
        break;
    case GDK_ACTION_COPY:
        fm_copy_files(files, fm_dnd_dest_get_dest_path(dd));
        break;
    case GDK_ACTION_LINK:
        // fm_link_files(files, fm_dnd_dest_get_dest_path(dd));
        break;
    case GDK_ACTION_ASK:
        g_debug("TODO: GDK_ACTION_ASK");
        break;
    }
    fm_list_unref(files);
    return TRUE;
}

inline static
gboolean cache_src_file_infos(FmDndDest* dd, GtkWidget *dest_widget,
                        gint x, gint y, GdkDragContext *drag_context)
{
    GdkAtom target;
    target = gtk_drag_dest_find_target( dest_widget, drag_context, NULL );
    if( target != GDK_NONE )
    {
        GdkDragAction action;
        gboolean ret;
        /* treat X direct save as a special case. */
        if( target == gdk_atom_intern_static_string("XdndDirectSave0") )
        {
            /* FIXME: need a better way to handle this. */
            action = drag_context->suggested_action;
            g_signal_emit(dd, signals[QUERY_INFO], 0, x, y, &action, &ret);

            gdk_drag_status(drag_context, action, time);
            return TRUE;
        }

        /* g_debug("try to cache src_files"); */
        dd->mainloop = g_main_loop_new(NULL, TRUE);
        gtk_drag_get_data(dest_widget, drag_context, target, time);
        /* run the main loop to block here waiting for
         * 'drag-data-received' signal being handled first. */
        /* it's possible that g_main_loop_quit is called before we really run the loop. */
        if(g_main_loop_is_running(dd->mainloop))
            g_main_loop_run(dd->mainloop);
        g_main_loop_unref(dd->mainloop);
        dd->mainloop = NULL;
        /* g_debug("src_files cached: %p", dd->src_files); */

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
    return FALSE;
}

static gboolean on_auto_scroll(FmDndDest* dd)
{
    GtkWidget* parent = gtk_widget_get_parent(dd->widget);
    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(parent);
    GtkAdjustment* va = gtk_scrolled_window_get_vadjustment(scroll);
    GtkAdjustment* ha = gtk_scrolled_window_get_hadjustment(scroll);
    int x, y;
    gdk_window_get_pointer(dd->widget->window, &x, &y, NULL);

    if(va)
    {
        if(y < SCROLL_EDGE_SIZE) /* scroll up */
        {
            if(va->value > va->lower)
            {
                va->value -= va->step_increment;
                if(va->value < va->lower)
                    va->value = va->lower;
            }
        }
        else if(y > (dd->widget->allocation.height - SCROLL_EDGE_SIZE))
        {
            /* scroll down */
            if(va->value < va->upper - va->page_size)
            {
                va->value += va->step_increment;
                if(va->value > va->upper - va->page_size)
                    va->value = va->upper - va->page_size;
            }
        }
        gtk_adjustment_value_changed(va);
    }

    if(ha)
    {
        if(x < SCROLL_EDGE_SIZE) /* scroll to left */
        {
            if(ha->value > ha->lower)
            {
                ha->value -= ha->step_increment;
                if(ha->value < ha->lower)
                    ha->value = ha->lower;
            }
        }
        else if(x > (dd->widget->allocation.width - SCROLL_EDGE_SIZE))
        {
            /* scroll to right */
            if(ha->value < ha->upper - ha->page_size)
            {
                ha->value += ha->step_increment;
                if(ha->value > ha->upper - ha->page_size)
                    ha->value = ha->upper - ha->page_size;
            }
        }
        gtk_adjustment_value_changed(ha);
    }
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

    /* if there is a idle handler scheduled to remove cached drag source */
    if(dd->idle)
    {
        g_source_remove(dd->idle); /* remove the idle handler */
        dd->idle = 0;
        /* do it now */
        clear_src_cache(dd);
    }

	/* cache drag source files */
	if( G_UNLIKELY(!dd->src_files) )
    {
        if(cache_src_file_infos(dd, dest_widget, x, y, drag_context))
            return TRUE;
    }
    if( !dd->src_files )
        return FALSE;

	action = drag_context->suggested_action;
    g_signal_emit(dd, signals[QUERY_INFO], 0, x, y, &action, &ret);
    /* FIXME: is this correct? */
    /* if currently action is not allowed */
    if(ret && action && (drag_context->actions & action) == 0)
        action = drag_context->suggested_action;

	gdk_drag_status(drag_context, action, time);

    if(0 == dd->scroll_timeout) /* install a scroll timeout if needed */
    {
        GtkWidget* parent = gtk_widget_get_parent(dd->widget);
        if(parent && GTK_IS_SCROLLED_WINDOW(parent))
        {
            dd->scroll_timeout = gdk_threads_add_timeout(150, (GSourceFunc)on_auto_scroll, dd);
        }
    }
    return (action != 0);
}

gboolean clear_src_cache(FmDndDest* dd)
{
	/* free cached source files */
	if(dd->src_files)
	{
		fm_list_unref(dd->src_files);
		dd->src_files = NULL;
	}
	if(dd->dest_file)
	{
		fm_file_info_unref(dd->dest_file);
		dd->dest_file = NULL;
	}
    dd->info_type = 0;
    dd->idle = 0;
    return FALSE;
}

static gboolean
on_drag_leave ( GtkWidget *dest_widget,
                GdkDragContext *drag_context,
                guint time,
                FmDndDest* dd )
{
    /* g_debug("drag leave"); */
	gtk_drag_unhighlight(dest_widget);
    /* FIXME: is there any better place to do this? */
    dd->idle = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)clear_src_cache, dd, NULL);

    if(dd->scroll_timeout)
    {
        g_source_remove(dd->scroll_timeout);
        dd->scroll_timeout = 0;
    }
    return TRUE;
}

static gboolean
on_drag_drop ( GtkWidget *dest_widget,
               GdkDragContext *drag_context,
               gint x,
               gint y,
               guint time,
               FmDndDest* dd )
{
    gboolean ret = TRUE;
    GdkAtom target, xds;
    target = gtk_drag_dest_find_target( dest_widget, drag_context, NULL );
    /* g_debug("drag drop"); */

	if( target == GDK_NONE)
		ret = FALSE;

	/* if it's XDS */
    xds = gdk_atom_intern_static_string(fm_default_dnd_dest_targets[FM_DND_DEST_TARGET_XDS].target);
	if( target == xds )
	{
        guchar *data = NULL;
        gint len = 0;
        GdkAtom text_atom = gdk_atom_intern_static_string ("text/plain");
        /* get filename from the source window */
        if(gdk_property_get(drag_context->source_window, xds, text_atom,
                            0, 1024, FALSE, NULL, NULL,
                            &len, &data) && data)
        {
            FmFileInfo* dest = fm_dnd_dest_get_dest_file(dd);
            if( dest && fm_file_info_is_dir(dest) )
            {
                FmPath* path = fm_path_new_child(dest->path, data);
                char* uri = fm_path_to_uri(path);
                /* setup the property */
                gdk_property_change(GDK_DRAWABLE(drag_context->source_window), xds,
                                   text_atom, 8, GDK_PROP_MODE_REPLACE, (const guchar *)uri,
                                   strlen(uri) + 1);
                fm_path_unref(path);
                g_free(uri);
            }
        }
        else
        {
            fm_show_error(gtk_widget_get_toplevel(dest_widget),
                          _("XDirectSave failed."));
            gdk_property_change(GDK_DRAWABLE(drag_context->source_window), xds,
                               text_atom, 8, GDK_PROP_MODE_REPLACE, (const guchar *)"", 0);
        }
        g_free(data);
        gtk_drag_get_data(dest_widget, drag_context, target, time);
        /* we should call gtk_drag_finish later in data-received callback. */
		return TRUE;
	}

	gtk_drag_finish( drag_context, ret, FALSE, time );

	/* free cached source files */
	if(dd->src_files)
	{
        if(ret)
            g_signal_emit(dd, signals[FILES_DROPPED], 0, drag_context->action, dd->info_type, dd->src_files, &ret);
        /* it's possible that src_files is already freed in idle handler */
        if(dd->src_files)
        {
            fm_list_unref(dd->src_files);
            dd->src_files = NULL;
        }
	}
	if(dd->dest_file)
	{
		fm_file_info_unref(dd->dest_file);
		dd->dest_file = NULL;
	}
    dd->info_type = 0;
    return ret;
}

/* FIXME: this is a little bit dirty... */
static void on_src_file_info_finished(FmFileInfoJob* job, FmDndDest* dd)
{
    dd->src_files = fm_list_ref(job->file_infos);
    dd->info_type = FM_DND_DEST_TARGET_FM_LIST;
    g_main_loop_quit(dd->mainloop);
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
    /* g_debug("data received: %d", info); */
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
			files = fm_path_list_new_from_uris((const char **)uris);
            g_free(uris);
        }
        break;
    case FM_DND_DEST_TARGET_XDS:
        if( sel_data->format == 8 && sel_data->length == 1 && sel_data->data[0] == 'F')
        {
            gdk_property_change(GDK_DRAWABLE(drag_context->source_window),
                               gdk_atom_intern_static_string ("XdndDirectSave0"),
                               gdk_atom_intern_static_string ("text/plain"), 8,
                               GDK_PROP_MODE_REPLACE, (const guchar *)"", 0);
        }
        else if(sel_data->format == 8 && sel_data->length == 1 && sel_data->data[0] == 'S')
        {
            /* XDS succeeds */
        }
        gtk_drag_finish(drag_context, TRUE, FALSE, time);
		return;
    }
	if(G_UNLIKELY(dd->src_files))
    {
		fm_list_unref(dd->src_files);
        dd->src_files = NULL;
    }

    if(info == FM_DND_DEST_TARGET_FM_LIST)
    {
        dd->info_type = info;
        dd->src_files = files;
        g_main_loop_quit(dd->mainloop);
    }
    else if(info == FM_DND_DEST_TARGET_URI_LIST)
    {
        /* convert FmPathList to FmFileInfoList */
        FmFileInfoJob* job = fm_file_info_job_new(files, 0);
        dd->src_files = NULL;
        fm_list_unref(files);
        /* g_main_loop_quit(dd->mainloop); will be called in on_src_file_info_finished() */
        g_signal_connect(job, "finished", G_CALLBACK(on_src_file_info_finished), dd);
        fm_job_run_async(FM_JOB(job));
    }
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

void fm_dnd_dest_set_dest_file(FmDndDest* dd, FmFileInfo* dest_file)
{
	if(dd->dest_file)
		fm_file_info_unref(dd->dest_file);
	dd->dest_file = dest_file ? fm_file_info_ref(dest_file) : NULL;
}
