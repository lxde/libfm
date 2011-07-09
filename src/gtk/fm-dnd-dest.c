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

    gboolean waiting_data;
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

/* GdkAtom value for drag target: XdndDirectSave0 */
static GdkAtom xds_target_atom = 0;


static void fm_dnd_dest_finalize              (GObject *object);
static gboolean fm_dnd_dest_files_dropped(FmDndDest* dd, int x, int y, GdkDragAction action, int info_type, FmList* files);

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
    dnd_dest_class->files_dropped = fm_dnd_dest_files_dropped;

    /* emitted when files are dropped on dest widget. */
    signals[ FILES_DROPPED ] =
        g_signal_new("files-dropped",
                     G_TYPE_FROM_CLASS( klass ),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET ( FmDndDestClass, files_dropped ),
                     g_signal_accumulator_true_handled, NULL,
                     fm_marshal_BOOL__INT_INT_UINT_UINT_POINTER,
                     G_TYPE_BOOLEAN, 5, G_TYPE_INT, G_TYPE_INT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER);

    xds_target_atom = gdk_atom_intern_static_string(fm_default_dnd_dest_targets[FM_DND_DEST_TARGET_XDS].target);
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
    dd->widget = w;
    if( w )
        g_object_add_weak_pointer(G_OBJECT(w), &dd->widget);
}

gboolean fm_dnd_dest_files_dropped(FmDndDest* dd, int x, int y, GdkDragAction action,
                                   int info_type, FmList* files)
{
    FmPath* dest;
    GtkWidget* parent;
    dest = fm_dnd_dest_get_dest_path(dd);
    if(!dest)
        return FALSE;
    g_debug("%d files-dropped!, info_type: %d", fm_list_get_length(files), info_type);

    if(fm_list_is_file_info_list(files))
        files = fm_path_list_new_from_file_info_list(files);
    else
        fm_list_ref(files);

    parent = gtk_widget_get_toplevel(dd->widget);
    switch(action)
    {
    case GDK_ACTION_MOVE:
        if(fm_path_is_trash_root(fm_dnd_dest_get_dest_path(dd)))
            fm_trash_files(GTK_WINDOW(parent), files);
        else
            fm_move_files(GTK_WINDOW(parent), files, fm_dnd_dest_get_dest_path(dd));
        break;
    case GDK_ACTION_COPY:
        fm_copy_files(GTK_WINDOW(parent), files, fm_dnd_dest_get_dest_path(dd));
        break;
    case GDK_ACTION_LINK:
        // fm_link_files(parent, files, fm_dnd_dest_get_dest_path(dd));
        break;
    case GDK_ACTION_ASK:
        g_debug("TODO: GDK_ACTION_ASK");
        break;
    }
    fm_list_unref(files);
    return TRUE;
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
    dd->src_dev = 0;
    dd->src_fs_id = NULL;

    dd->info_type = 0;
    dd->idle = 0;
    dd->waiting_data = FALSE;
    return FALSE;
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
    if(dd->dest_file == dest_file)
        return;
    if(dd->dest_file)
        fm_file_info_unref(dd->dest_file);
    dd->dest_file = dest_file ? fm_file_info_ref(dest_file) : NULL;
}

gboolean fm_dnd_dest_drag_data_received(FmDndDest* dd, GdkDragContext *drag_context,
             gint x, gint y, GtkSelectionData *sel_data, guint info, guint time)
{
    FmList* files = NULL;
    GtkWidget *dest_widget = dd->widget;

    if(info ==  FM_DND_DEST_TARGET_FM_LIST)
    {
        if((sel_data->length >= 0) && (sel_data->format==8))
        {
            /* get the pointer */
            memcpy(&files, sel_data->data, sel_data->length);
            if(files)
                fm_list_ref(files);
            if(files)
            {
                FmFileInfo* fi = FM_FILE_INFO(fm_list_peek_head(files));
                /* get the device of the first dragged source file */
                if(fm_path_is_native(fi->path))
                    dd->src_dev = fi->dev;
                else
                    dd->src_fs_id = fi->fs_id;
            }
        }
    }
    else if(info == FM_DND_DEST_TARGET_URI_LIST)
    {
        if((sel_data->length >= 0) && (sel_data->format==8))
        {
            gchar **uris;
            uris = gtk_selection_data_get_uris( sel_data );
            files = fm_path_list_new_from_uris((const char **)uris);
            g_free(uris);
            if(files)
            {
                GFileInfo* inf;
                FmPath* path = FM_PATH(fm_list_peek_head(files));
                GFile* gf = fm_path_to_gfile(path);
                const char* attr = fm_path_is_native(path) ? G_FILE_ATTRIBUTE_UNIX_DEVICE : G_FILE_ATTRIBUTE_ID_FILESYSTEM;
                inf = g_file_query_info(gf, attr, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
                g_object_unref(gf);

                if(fm_path_is_native(path))
                    dd->src_dev = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_DEVICE);
                else
                    dd->src_fs_id = g_intern_string(g_file_info_get_attribute_string(inf, G_FILE_ATTRIBUTE_ID_FILESYSTEM));
                g_object_unref(inf);
            }
        }
    }
    else if(info == FM_DND_DEST_TARGET_XDS) /* X direct save */
    {
        if( sel_data->format == 8 && sel_data->length == 1 && sel_data->data[0] == 'F')
        {
            gdk_property_change(GDK_DRAWABLE(drag_context->source_window),
                               xds_target_atom,
                               gdk_atom_intern_static_string("text/plain"), 8,
                               GDK_PROP_MODE_REPLACE, (const guchar *)"", 0);
        }
        else if(sel_data->format == 8 && sel_data->length == 1 && sel_data->data[0] == 'S')
        {
            /* XDS succeeds */
        }
        gtk_drag_finish(drag_context, TRUE, FALSE, time);
        return TRUE;
    }
    else
        return FALSE;

    /* remove previously cached source files. */
    if(G_UNLIKELY(dd->src_files))
    {
        fm_list_unref(dd->src_files);
        dd->src_files = NULL;
    }
    dd->src_files = files;
    dd->waiting_data = FALSE;
    dd->info_type = info;
    return TRUE;
}

GdkAtom fm_dnd_dest_find_target(FmDndDest* dd, GdkDragContext *drag_context)
{
    int i;
    for(i = 0; i < G_N_ELEMENTS(fm_default_dnd_dest_targets); ++i)
    {
        GdkAtom target = gdk_atom_intern_static_string(fm_default_dnd_dest_targets[i].target);
        if(fm_drag_context_has_target(drag_context, target))
            return target;
    }
    return GDK_NONE;
}

gboolean fm_dnd_dest_is_target_supported(FmDndDest* dd, GdkAtom target)
{
    gboolean ret = FALSE;
    GtkWidget *dest_widget = dd->widget;
    int i;

    for(i = 0; i < G_N_ELEMENTS(fm_default_dnd_dest_targets); ++i)
    {
        if(gdk_atom_intern_static_string(fm_default_dnd_dest_targets[i].target) == target)
        {
            ret = TRUE;
            break;
        }
    }
    return ret;
}

gboolean fm_dnd_dest_drag_drop(FmDndDest* dd, GdkDragContext *drag_context,
                               GdkAtom target, int x, int y, guint time)
{
    gboolean ret = FALSE;
    GtkWidget* dest_widget = dd->widget;
    int i;
    for(i = 0; i < G_N_ELEMENTS(fm_default_dnd_dest_targets); ++i)
    {
        if(gdk_atom_intern_static_string(fm_default_dnd_dest_targets[i].target) == target)
        {
            ret = TRUE;
            break;
        }
    }
    if(ret) /* we support this kind of target */
    {
        if(i == FM_DND_DEST_TARGET_XDS) /* if this is XDS */
        {
            guchar *data = NULL;
            gint len = 0;
            GdkAtom text_atom = gdk_atom_intern_static_string("text/plain");
            /* get filename from the source window */
            if(gdk_property_get(drag_context->source_window, xds_target_atom, text_atom,
                                0, 1024, FALSE, NULL, NULL,
                                &len, &data) && data)
            {
                FmFileInfo* dest = fm_dnd_dest_get_dest_file(dd);
                if( dest && fm_file_info_is_dir(dest) )
                {
                    FmPath* path = fm_path_new_child(dest->path, data);
                    char* uri = fm_path_to_uri(path);
                    /* setup the property */
                    gdk_property_change(GDK_DRAWABLE(drag_context->source_window), xds_target_atom,
                                       text_atom, 8, GDK_PROP_MODE_REPLACE, (const guchar *)uri,
                                       strlen(uri) + 1);
                    fm_path_unref(path);
                    g_free(uri);
                }
            }
            else
            {
                fm_show_error(gtk_widget_get_toplevel(dest_widget), NULL,
                              _("XDirectSave failed."));
                gdk_property_change(GDK_DRAWABLE(drag_context->source_window), xds_target_atom,
                                   text_atom, 8, GDK_PROP_MODE_REPLACE, (const guchar *)"", 0);
            }
            g_free(data);
            gtk_drag_get_data(dest_widget, drag_context, target, time);
            /* we should call gtk_drag_finish later in data-received callback. */
            return TRUE;
        }

        /* see if the drag files are cached */
        if(dd->src_files)
        {
            /* emit files-dropped signal */
            g_signal_emit(dd, signals[FILES_DROPPED], 0, x, y, drag_context->action, dd->info_type, dd->src_files, &ret);
        }
        else /* we don't have the data */
        {
            if(dd->waiting_data) /* if we're still waiting for the data */
            {
                /* FIXME: how to handle this? */
                ret = FALSE;
            }
            else
                ret = FALSE;
        }
        gtk_drag_finish(drag_context, ret, FALSE, time);
    }
    return ret;
}

/**
 * fm_dnd_dest_get_default_action
 * @dd FmDndDest object
 * @target GdkTarget of the target data type
 * @dest FmFileInfo of the destination file at drop site.
 *
 * Returns the default action to take for the dragged files.
 */
GdkDragAction fm_dnd_dest_get_default_action(FmDndDest* dd,
                                             GdkDragContext* drag_context,
                                             GdkTarget target)
{
    GdkDragAction action;
    FmFileInfo* dest = dd->dest_file;

    if(!dd->src_files)  /* we didn't have any data, cache it */
    {
        action = 0;
        if(!dd->waiting_data) /* we're still waiting for "drag-data-received" signal */
        {
            /* retrieve the source files */
            gtk_drag_get_data(dd->widget, drag_context, target, time);
            dd->waiting_data = TRUE;
        }
    }

    if(!dest || !dest->path)
        return FALSE;

    /* this is XDirectSave */
    if(target == xds_target_atom)
        return GDK_ACTION_COPY;

    if(dd->src_files) /* we have got drag source files */
    {
        FmPath* dest_path = dest->path;
        if(fm_path_is_trash(dest_path))
        {
            if(fm_path_is_trash_root(dest_path)) /* we can only move files to trash can */
                action = GDK_ACTION_MOVE;
            else /* files inside trash are read only */
                action = 0;
        }
        else if(fm_path_is_virtual(dest_path))
        {
            /* computer:/// and network:/// shouldn't received dropped files. */
            /* FIXME: some special handling can be done with menu:// */
            action = 0;
        }
        else /* dest is a ordinary path */
        {
            /* determine if the dragged files are on the same device as destination file */
            /* Here we only check the first dragged file since checking all of them can
             * make the operation very slow. */
            gboolean same_fs;
            if(dd->src_dev || dd->src_fs_id) /* we know the device of dragged source files */
            {
                /* compare the device/filesystem id against that of destination file */
                if(fm_path_is_native(dest_path))
                    same_fs = dd->src_dev && (dd->src_dev == dest->dev);
                else /* FIXME: can we use direct comparison here? */
                    same_fs = dd->src_fs_id && (0 == g_strcmp0(dd->src_fs_id, dest->fs_id));
                action = same_fs ? GDK_ACTION_MOVE : GDK_ACTION_COPY;
            }
            else /* we don't know on which device the dragged source files are. */
                action = 0;
        }
    }

    if( action && 0 == (drag_context->actions & action) )
        action = drag_context->suggested_action;

    return action;
}

void fm_dnd_dest_drag_leave(FmDndDest* dd, GdkDragContext* drag_context, guint time)
{
    dd->idle = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)clear_src_cache, dd, NULL);
}
