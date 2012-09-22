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
 * The #FmDndSrc can be used by some widget to provide support for drag
 * operations on #FmFileInfo objects that are represented in that widget.
 *
 * To use #FmDndSrc the widget should create it with fm_dnd_src_new() and
 * connect to the #FmDndSrc::data-get signal of the created #FmDndSrc
 * object.
 * <example id="example-fmdndsrc-usage">
 * <title>Sample Usage</title>
 * <programlisting>
 * {
 *    widget->ds = fm_dnd_src_new(widget);
 *    g_signal_connect(widget->ds, "data-get", G_CALLBACK(on_data_get), widget);
 *
 *    ...
 * }
 *
 * static void on_object_finalize(MyWidget *widget)
 * {
 *    ...
 *
 *    g_signal_handlers_disconnect_by_data(widget->ds, widget);
 *    g_object_unref(G_OBJECT(widget->ds));
 * }
 *
 * static void on_data_get(FmDndSrc *ds, MyWidget *widget)
 * {
 *    FmFileInfo *file = widget->selected_file;
 *
 *    fm_dnd_src_set_file(ds, file);
 * }
 * </programlisting>
 * </example>
 * The #FmDndSrc will set drag activation for the widget by left mouse
 * button so if widget wants to use mouse movement with left button
 * pressed for something else (rubberbanding for example) then it should
 * disable Gtk drag handlers when needs (by blocking handlers that match
 * object data "gtk-site-data" usually).
 *
 * If widget wants to handle some types of data other than #FmFileInfo
 * objects it should do it the usual way by connecting handlers for the
 * #GtkWidget::drag-data-get, #GtkWidget::drag-begin, and #GtkWidget::drag-end
 * signals and adding own targets to widget's drag source target list. To
 * exclude conflicts the widget's specific handlers should use info
 * indices starting from N_FM_DND_SRC_DEFAULT_TARGETS.
 */

#include "fm-dnd-src.h"

GtkTargetEntry fm_default_dnd_src_targets[] =
{
    {"application/x-fmlist-ptr", GTK_TARGET_SAME_APP, FM_DND_SRC_TARGET_FM_LIST},
    {"text/uri-list", 0, FM_DND_SRC_TARGET_URI_LIST}
};

#if 0
static GdkAtom src_target_atom[N_FM_DND_SRC_DEFAULT_TARGETS];
#endif

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

#if 0
    for(i = 0; i < N_FM_DND_SRC_DEFAULT_TARGETS; i++)
        src_target_atom[i] = GDK_NONE;
    for(i = 0; i < G_N_ELEMENTS(fm_default_dnd_src_targets); i++)
        src_target_atom[fm_default_dnd_src_targets[i].info] =
            gdk_atom_intern_static_string(fm_default_dnd_src_targets[i].target);
#endif
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
 * Before 1.0.1 this API didn't update drag source on widget so caller
 * should set it itself. Since access to fm_default_dnd_src_targets
 * outside of this API considered unsecure, that behavior was changed.
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
 * Before 1.0.1 this API didn't update drag source on widget so caller
 * should set and unset it itself. Access to fm_default_dnd_src_targets
 * outside of this API considered unsecure so that behavior was changed.
 *
 * Since: 0.1.0
 */
void fm_dnd_src_set_widget(FmDndSrc* ds, GtkWidget* w)
{
    if(w == ds->widget)
        return;
    if(ds->widget) /* there is an old widget connected */
    {
        gtk_drag_source_unset(ds->widget);
        g_object_remove_weak_pointer(G_OBJECT(ds->widget), (gpointer*)&ds->widget);
        g_signal_handlers_disconnect_by_func(ds->widget, on_drag_data_get, ds);
        g_signal_handlers_disconnect_by_func(ds->widget, on_drag_begin, ds);
        g_signal_handlers_disconnect_by_func(ds->widget, on_drag_end, ds);
    }
    ds->widget = w;
    if( w )
    {
        GtkTargetList *targets;
        gtk_drag_source_set(w, GDK_BUTTON1_MASK, fm_default_dnd_src_targets,
                            G_N_ELEMENTS(fm_default_dnd_src_targets),
                            GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
        targets = gtk_drag_source_get_target_list(w);
        gtk_target_list_add_text_targets(targets, FM_DND_SRC_TARGET_TEXT);
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

    if(info == 0 || info >= N_FM_DND_SRC_DEFAULT_TARGETS)
        return;

    /*  Don't call the default handler  */
    /* FIXME: is this ever needed? */
    /* g_signal_stop_emission_by_name( src_widget, "drag-data-get" ); */

    type = gtk_selection_data_get_target(sel_data);
    switch( info )
    {
    case FM_DND_SRC_TARGET_FM_LIST:
        /* just store the pointer in GtkSelection since this is used
         * within the same app. */
        gtk_selection_data_set(sel_data, type, 8,
                               (guchar*)&ds->files, sizeof(gpointer));
        break;
    case FM_DND_SRC_TARGET_URI_LIST:
    case FM_DND_SRC_TARGET_TEXT:
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
                g_string_append( uri_list, "\n" );
            }
            if(info == FM_DND_SRC_TARGET_URI_LIST)
                gtk_selection_data_set(sel_data, type, 8,
                                       (guchar*)uri_list->str, uri_list->len);
            else /* FM_DND_SRC_TARGET_TEXT */
                gtk_selection_data_set_text(sel_data, uri_list->str, uri_list->len);
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
