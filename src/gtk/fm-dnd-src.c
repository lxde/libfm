/*
 *      fm-dnd-src.c
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

/**
 * SECTION:fm-dnd-src
 * @short_description: Libfm support for drag&drop source.
 * @title: FmDndSrc
 *
 * @include: libfm/fm-dnd-src.h
 *
 * The #FmDndSrc is used to assign selection for Drag & Drop operations
 * and deliver info about it to target widget when requested.
 */

#include "fm-dnd-src.h"

GtkTargetEntry fm_default_dnd_src_targets[] =
{
    {"application/x-fmlist-ptr", GTK_TARGET_SAME_APP, FM_DND_SRC_TARGET_FM_LIST},
    {"text/uri-list", 0, FM_DND_SRC_TARGET_URI_LIST}
};

enum
{
    DATA_GET,
    N_SIGNALS
};

static void fm_dnd_src_dispose             (GObject *object);

static void
on_drag_data_get ( GtkWidget *src_widget,
                   GdkDragContext *drag_context,
                   GtkSelectionData *sel_data,
                   guint info,
                   guint time,
                   FmDndSrc* ds );

static void
on_drag_begin ( GtkWidget *src_widget,
                GdkDragContext *drag_context,
                FmDndSrc* ds );

static void
on_drag_end ( GtkWidget *src_widget,
              GdkDragContext *drag_context,
              FmDndSrc* ds );

static guint signals[N_SIGNALS];


G_DEFINE_TYPE(FmDndSrc, fm_dnd_src, G_TYPE_OBJECT);


static void fm_dnd_src_class_init(FmDndSrcClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_dnd_src_dispose;

    /**
     * FmDndSrc::data-get:
     * @object: the object which emitted the signal
     *
     * The #FmDndSrc::data-get signal is emitted when information of
     * source files is needed. Handler of the signal should then call
     * fm_dnd_src_set_files() to provide info of dragged source files.
     *
     * Since: 0.1.0
     */
    signals[ DATA_GET ] =
        g_signal_new ( "data-get",
                       G_TYPE_FROM_CLASS( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( FmDndSrcClass, data_get ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0 );
}


static void fm_dnd_src_dispose(GObject *object)
{
    FmDndSrc *ds;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_DND_SRC(object));

    ds = (FmDndSrc*)object;

    if(ds->files)
    {
        fm_file_info_list_unref(ds->files);
        ds->files = NULL;
    }

    fm_dnd_src_set_widget(ds, NULL);

    G_OBJECT_CLASS(fm_dnd_src_parent_class)->dispose(object);
}


static void fm_dnd_src_init(FmDndSrc *self)
{

}

/**
 * fm_dnd_src_new
 * @w: (allow-none): the widget where source files are selected
 *
 * Creates new drag source descriptor.
 *
 * Returns: (transfer full): a new #FmDndSrc object.
 *
 * Since: 0.1.0
 */
FmDndSrc *fm_dnd_src_new(GtkWidget* w)
{
    FmDndSrc* ds = (FmDndSrc*)g_object_new(FM_TYPE_DND_SRC, NULL);
    fm_dnd_src_set_widget(ds, w);
    return ds;
}

/**
 * fm_dnd_src_set_widget
 * @ds: the drag source descriptor
 * @w: (allow-none): the widget where source files are selected
 *
 * Resets drag source widget in @ds.
 *
 * Since: 0.1.0
 */
void fm_dnd_src_set_widget(FmDndSrc* ds, GtkWidget* w)
{
    if(w == ds->widget)
        return;
    if(ds->widget) /* there is an old widget connected */
    {
        g_object_remove_weak_pointer(G_OBJECT(ds->widget), (gpointer*)&ds->widget);
        g_signal_handlers_disconnect_by_func(ds->widget, on_drag_data_get, ds);
        g_signal_handlers_disconnect_by_func(ds->widget, on_drag_begin, ds);
        g_signal_handlers_disconnect_by_func(ds->widget, on_drag_end, ds);
    }
    ds->widget = w;
    if( w )
    {
        g_object_add_weak_pointer(G_OBJECT(w), (gpointer*)&ds->widget);
        g_signal_connect(w, "drag-data-get", G_CALLBACK(on_drag_data_get), ds);
        g_signal_connect_after(w, "drag-begin", G_CALLBACK(on_drag_begin), ds);
        g_signal_connect_after(w, "drag-end", G_CALLBACK(on_drag_end), ds);
    }
}

/**
 * fm_dnd_src_set_files
 * @ds: the drag source descriptor
 * @files: list of files to set
 *
 * Sets @files as selection list in the source descriptor.
 *
 * Since: 0.1.0
 */
void fm_dnd_src_set_files(FmDndSrc* ds, FmFileInfoList* files)
{
    if(ds->files)
        fm_file_info_list_unref(ds->files);
    ds->files = fm_file_info_list_ref(files);
}

/**
 * fm_dnd_src_set_file
 * @ds: the drag source descriptor
 * @file: files to set
 *
 * Sets @file as selection in the source descriptor.
 *
 * Since: 0.1.0
 */
void fm_dnd_src_set_file(FmDndSrc* ds, FmFileInfo* file)
{
    FmFileInfoList* files = fm_file_info_list_new();
    fm_file_info_list_push_tail(files, file);
    if(ds->files)
        fm_file_info_list_unref(ds->files);
    ds->files = files;
}

static void
on_drag_data_get ( GtkWidget *src_widget,
                   GdkDragContext *drag_context,
                   GtkSelectionData *sel_data,
                   guint info,
                   guint time,
                   FmDndSrc* ds )
{
    GdkAtom type;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( src_widget, "drag-data-get" );
    drag_context->actions = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;

    type = gdk_atom_intern_static_string(fm_default_dnd_src_targets[info].target);
    switch( info )
    {
    case FM_DND_SRC_TARGET_FM_LIST:
        /* just store the pointer in GtkSelection since this is used
         * within the same app. */
        gtk_selection_data_set(sel_data, type, 8,
                               (guchar*)&ds->files, sizeof(gpointer));
        break;
    case FM_DND_SRC_TARGET_URI_LIST:
        {
            gchar* uri;
            GString* uri_list = g_string_sized_new( 8192 );
            GList* l;
            FmFileInfo* file;

            for( l = fm_file_info_list_peek_head_link(ds->files); l; l=l->next )
            {
                file = FM_FILE_INFO(l->data);
                uri = fm_path_to_uri(fm_file_info_get_path(file));
                g_string_append( uri_list, uri );
                g_free( uri );
                g_string_append( uri_list, "\r\n" );
            }
            gtk_selection_data_set ( sel_data, type, 8,
                                     ( guchar* ) uri_list->str, uri_list->len + 1 );
            g_string_free( uri_list, TRUE );
        }
        break;
    }
}

static void
on_drag_begin ( GtkWidget *src_widget,
                GdkDragContext *drag_context,
                FmDndSrc* ds )
{
    /* block default handler */

    gtk_drag_set_icon_default( drag_context );

    /* FIXME: set the icon to file icon later */
    // gtk_drag_set_icon_pixbuf();

    /* ask drag source to provide list of source files. */
    g_signal_emit(ds, signals[DATA_GET], 0);
}

static void
on_drag_end ( GtkWidget *src_widget,
              GdkDragContext *drag_context,
              FmDndSrc* ds )
{

}
