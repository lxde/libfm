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

#define FM_PATH_ENTRY_GET_PRIVATE(obj) ( G_TYPE_INSTANCE_GET_PRIVATE( (obj), FM_TYPE_PATH_ENTRY, FmPathEntryPrivate ) )

typedef struct _FmPathEntryPrivate FmPathEntryPrivate;

struct _FmPathEntryPrivate
{
    FmPath* path;
    /* model used for completion */
    FmFolderModel* model;

    /* name of parent dir */
    char* parent_dir;
    /* length of parent dir */
    gint parent_len;

    gboolean highlight_completion_match;
    GtkEntryCompletion* completion;

    /* cancellable for dir listing */
    GCancellable* cancellable;

    /* length of basename typed by the user */
    gint typed_basename_len;
};

typedef struct
{
    FmPathEntry* entry;
    GFile* dir;
    GList* subdirs;
    GCancellable* cancellable;
}ListSubDirNames;

static void      fm_path_entry_activate(GtkEntry *entry, gpointer user_data);
static gboolean  fm_path_entry_key_press(GtkWidget   *widget, GdkEventKey *event, gpointer user_data);
static void      fm_path_entry_class_init(FmPathEntryClass *klass);
static void  fm_path_entry_editable_init(GtkEditableClass *iface);
static gboolean  fm_path_entry_focus_in_event(GtkWidget *widget, GdkEvent  *event);
static gboolean  fm_path_entry_focus_out_event(GtkWidget *widget, GdkEvent  *event);
static void      fm_path_entry_changed(GtkEditable *editable, gpointer user_data);
static void      fm_path_entry_init(FmPathEntry *entry);
static void      fm_path_entry_finalize(GObject *object);
static gboolean  fm_path_entry_match_func(GtkEntryCompletion   *completion,
                                          const gchar          *key,
                                          GtkTreeIter          *iter,
                                          gpointer user_data);
static void fm_path_entry_completion_render_func(GtkCellLayout *cell_layout,
                                                 GtkCellRenderer *cell,
                                                 GtkTreeModel *model,
                                                 GtkTreeIter *iter,
                                                 gpointer data);
static void fm_path_entry_set_property(GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec);
static void fm_path_entry_get_property(GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec);

G_DEFINE_TYPE_EXTENDED( FmPathEntry, fm_path_entry, GTK_TYPE_ENTRY,
                       0, G_IMPLEMENT_INTERFACE(GTK_TYPE_EDITABLE, fm_path_entry_editable_init) );


/* customized model used for entry completion to save memory.
 * GtkEntryCompletion requires that we store full paths in the model
 * to work, but we only want to store basenames to save memory.
 * So we created a custom model to do this. */

enum {
    COL_BASENAME,
    COL_FULL_PATH,
    N_COLS
};

#define FM_TYPE_PATH_ENTRY_MODEL (fm_path_entry_model_get_type())

typedef struct _FmPathEntryModel FmPathEntryModel;
typedef struct _FmPathEntryModelClass FmPathEntryModelClass;

struct _FmPathEntryModel
{
    GtkListStore parent_instance;
    FmPathEntry* entry;
};

struct _FmPathEntryModelClass
{
    GtkListStoreClass parent_class;
};

static GType fm_path_entry_model_get_type(void);
static void fm_path_entry_model_iface_init(GtkTreeModelIface *iface);
static GtkTreeModel* fm_path_entry_model_new(FmPathEntry* entry);

G_DEFINE_TYPE_EXTENDED( FmPathEntryModel, fm_path_entry_model, GTK_TYPE_LIST_STORE,
                       0, G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, fm_path_entry_model_iface_init) );

/* end declaration of the customized model. */

static GtkEditableClass *parent_editable_interface = NULL;
static GtkTreeModelIface *parent_tree_model_interface = NULL;

static gboolean fm_path_entry_key_press(GtkWidget   *widget, GdkEventKey *event, gpointer user_data)
{
    FmPathEntry *entry = FM_PATH_ENTRY(widget);
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);
    char* text;
    int pos;
    switch( event->keyval )
    {
    case GDK_Tab:
        {
            gtk_entry_completion_insert_prefix(priv->completion);
            gtk_editable_set_position(GTK_EDITABLE(entry), -1);
            return TRUE;
        }
    }
    return FALSE;
}

static void  fm_path_entry_activate(GtkEntry *entry, gpointer user_data)
{
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);
    const char* full_path;
    char* disp_name;
    /* convert current path string to FmPath here */

    full_path = gtk_entry_get_text(entry);
    if(priv->path)
        fm_path_unref(priv->path);

    /* special handling for home dir */
    if(full_path[0] == '~' && full_path[1] == G_DIR_SEPARATOR)
        priv->path = fm_path_new_relative(fm_path_get_home(), full_path + 2);
    else if(full_path[0] == '~' && full_path[1] == 0)
        priv->path = fm_path_ref(fm_path_get_home());
    else
        priv->path = fm_path_new_for_str(full_path);

    disp_name = fm_path_display_name(priv->path, FALSE);
    gtk_entry_set_text(entry, disp_name);
    g_free(disp_name);

    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
}

static void fm_path_entry_class_init(FmPathEntryClass *klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    GtkEntryClass* entry_class = GTK_ENTRY_CLASS(klass);

    object_class->get_property = fm_path_entry_get_property;
    object_class->set_property = fm_path_entry_set_property;
    g_object_class_install_property( object_class,
                                    PROP_HIGHLIGHT_COMPLETION_MATCH,
                                    g_param_spec_boolean("highlight-completion-match",
                                                         "Highlight completion match",
                                                         "Wheather to highlight the completion match",
                                                         TRUE, G_PARAM_READWRITE) );
    object_class->finalize = fm_path_entry_finalize;
    /* entry_class->activate = fm_path_entry_activate; */

    widget_class->focus_in_event = fm_path_entry_focus_in_event;
    widget_class->focus_out_event = fm_path_entry_focus_out_event;

    g_type_class_add_private( klass, sizeof (FmPathEntryPrivate) );
}

static void fm_path_entry_editable_init(GtkEditableClass *iface)
{
    parent_editable_interface = g_type_interface_peek_parent(iface);
    /* iface->changed = fm_path_entry_changed; */
    /* iface->do_insert_text = fm_path_entry_do_insert_text; */
}

static void clear_completion(FmPathEntryPrivate* priv)
{
    if(priv->parent_dir)
    {
        priv->parent_len = 0;
        g_free(priv->parent_dir);
        priv->parent_dir = NULL;
        /* cancel running dir-listing jobs */
        if(priv->cancellable)
        {
            g_cancellable_cancel(priv->cancellable);
            g_object_unref(priv->cancellable);
            priv->cancellable = NULL;
        }
        /* clear current model */
        gtk_list_store_clear(GTK_LIST_STORE(priv->model));
    }
    priv->typed_basename_len = 0;
}

static gboolean  fm_path_entry_focus_in_event(GtkWidget *widget, GdkEvent  *event)
{
    FmPathEntry *entry = FM_PATH_ENTRY(widget);
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);
    /* activate auto-completion */
    gtk_entry_set_completion(entry, priv->completion);

    /* listen to 'changed' signal for auto-completion */
    g_signal_connect(entry, "changed", G_CALLBACK(fm_path_entry_changed), NULL);
    return GTK_WIDGET_CLASS(fm_path_entry_parent_class)->focus_in_event(widget, event);
}

static gboolean  fm_path_entry_focus_out_event(GtkWidget *widget, GdkEvent  *event)
{
    FmPathEntry *entry = FM_PATH_ENTRY(widget);
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);
    /* de-activate auto-completion */
    gtk_entry_set_completion(entry, NULL);

    /* release all resources allocated for completion. */
    clear_completion(priv);

    /* disconnect from 'changed' signal since we don't do auto-completion
     * when we have no keyboard focus. */
    g_signal_handlers_disconnect_by_func(entry, fm_path_entry_changed, NULL);

    return GTK_WIDGET_CLASS(fm_path_entry_parent_class)->focus_out_event(widget, event);
}

static void on_dir_list_finished(gpointer user_data)
{
    ListSubDirNames* data = (ListSubDirNames*)user_data;
    FmPathEntry* entry = data->entry;
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);
    GList* l;
    GtkListStore* new_model;

    /* final chance to check cancellable */
    if(g_cancellable_is_cancelled(data->cancellable))
        return;

    new_model = fm_path_entry_model_new(entry);
    /* g_debug("dir list is finished!"); */

    /* update the model */
    for(l = data->subdirs; l; l=l->next)
    {
        char* name = l->data;
        gtk_list_store_insert_with_values(new_model, NULL, -1, COL_BASENAME, name, -1);
    }

    if(priv->model)
        g_object_unref(priv->model);
    priv->model = new_model;
    gtk_entry_completion_set_model(priv->completion, GTK_TREE_MODEL(new_model));

    /* NOTE: after the content of entry gets changed, by default gtk+ installs
     * an timeout handler with timeout 300 ms to popup completion list.
     * If the dir listing takes more than 300 ms and finished after the
     * timeout callback is called, then the completion list is empty at
     * that time. So completion doesn't work. So, we trigger a 'changed'
     * signal here to let GtkEntry do the completion with the new model again. */

    /* trigger completion popup. FIXME: this is a little bit dirty.
     * A even more dirty thing to do is to check if we finished after
     * 300 ms timeout happens. */
    g_signal_emit_by_name(entry, "changed", 0);
}

static gboolean list_sub_dirs(GIOSchedulerJob *job, GCancellable *cancellable, gpointer user_data)
{
    ListSubDirNames* data = (ListSubDirNames*)user_data;
    GError *err = NULL;
    /* g_debug("new dir listing job!"); */
    GFileEnumerator* enu = g_file_enumerate_children(data->dir,
                                    G_FILE_ATTRIBUTE_STANDARD_NAME","
                                    G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                    G_FILE_QUERY_INFO_NONE, cancellable,
                                    &err);
    if(enu)
    {
        while(!g_cancellable_is_cancelled(cancellable))
        {
            GFileInfo* inf = g_file_enumerator_next_file(enu, cancellable, &err);
            if(inf)
            {
                GFileType type = g_file_info_get_file_type(inf);
                if(type == G_FILE_TYPE_DIRECTORY)
                {
                    const char* name = g_file_info_get_name(inf);
                    data->subdirs = g_list_prepend(data->subdirs, g_strdup(name));
                }
                g_object_unref(inf);
            }
            else
            {
                if(err) /* error happens */
                {
                    g_error_free(err);
                    err = NULL;
                }
                else /* EOF */
                    break;
            }
        }
        g_object_unref(enu);
    }

    if(!g_cancellable_is_cancelled(cancellable))
    {
        /* finished! */
        g_io_scheduler_job_send_to_mainloop(job, on_dir_list_finished, data, NULL);
    }
    return FALSE;
}

static void list_sub_dir_names_free(ListSubDirNames* data)
{
    g_object_unref(data->dir);
    g_object_unref(data->cancellable);
    g_list_foreach(data->subdirs, (GFunc)g_free, NULL);
    g_list_free(data->subdirs);
    g_slice_free(ListSubDirNames, data);
}

static void fm_path_entry_changed(GtkEditable *editable, gpointer user_data)
{
    FmPathEntry *entry = FM_PATH_ENTRY(editable);
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);
    GtkWidget* widget = GTK_WIDGET(entry);
    const gchar *path_str, *sep;

    /* find parent dir of current path */
    path_str = gtk_entry_get_text( GTK_ENTRY(entry) );
    sep = g_utf8_strrchr(path_str, -1, G_DIR_SEPARATOR);
    if(sep) /* we found a parent dir */
    {
        int parent_len = (sep - path_str) + 1; /* includes the dir separator / */
        if(!priv->parent_dir
           || priv->parent_len != parent_len
           || strncmp(priv->parent_dir, path_str, parent_len ))
        {
            /* parent dir has been changed, reload dir list */
            ListSubDirNames* data = g_slice_new0(ListSubDirNames);
            g_free(priv->parent_dir);
            priv->parent_dir = g_strndup(path_str, parent_len);
            priv->parent_len = parent_len;
            /* g_debug("parent dir is changed to %s", priv->parent_dir); */

            /* FIXME: convert utf-8 encoded path to on-disk encoding. */
            data->entry = entry;
            if(priv->parent_dir[0] == '~') /* special case for home dir */
            {
                char* expand = g_strconcat(g_get_home_dir(), priv->parent_dir + 1, NULL);
                data->dir = g_file_new_for_commandline_arg(expand);
                g_free(expand);
            }
            else
                data->dir = g_file_new_for_commandline_arg(priv->parent_dir);

            /* clear current model */
            gtk_list_store_clear(GTK_LIST_STORE(priv->model));

            /* cancel running dir-listing jobs */
            if(priv->cancellable)
            {
                g_cancellable_cancel(priv->cancellable);
                g_object_unref(priv->cancellable);
            }

            /* launch a new job to do dir listing */
            data->cancellable = g_cancellable_new();
            priv->cancellable = (GCancellable*)g_object_ref(data->cancellable);
            g_io_scheduler_push_job(list_sub_dirs,
                                    data, (GDestroyNotify)list_sub_dir_names_free,
                                    G_PRIORITY_LOW, data->cancellable);
        }
        /* calculate the length of remaining part after / */
        priv->typed_basename_len = strlen(sep + 1);
    }
    else /* clear all autocompletion thing. */
        clear_completion(priv);
}

static void fm_path_entry_set_property(GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
    FmPathEntry *entry = FM_PATH_ENTRY(object);
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);

    switch( prop_id )
    {
    case PROP_HIGHLIGHT_COMPLETION_MATCH:
        priv->highlight_completion_match = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void fm_path_entry_get_property(GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
    FmPathEntry *entry = FM_PATH_ENTRY(object);
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);

    switch( prop_id ) {
    case PROP_HIGHLIGHT_COMPLETION_MATCH:
        g_value_set_boolean(value, priv->highlight_completion_match);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
fm_path_entry_init(FmPathEntry *entry)
{
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(entry);
    GtkEntryCompletion* completion = gtk_entry_completion_new();
    GtkCellRenderer* render;

    priv->model = fm_path_entry_model_new(entry);
    priv->completion = completion;
    priv->cancellable = g_cancellable_new();
    priv->highlight_completion_match = TRUE;
    gtk_entry_completion_set_minimum_key_length(completion, 1);

    gtk_entry_completion_set_match_func(completion, fm_path_entry_match_func, NULL, NULL);
    g_object_set(completion, "text_column", COL_FULL_PATH, NULL);
    gtk_entry_completion_set_model(completion, priv->model);

    render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( (GtkCellLayout*)completion, render, TRUE );
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(completion), render, fm_path_entry_completion_render_func, entry, NULL);

    /* NOTE: this is to avoid a bug of gtk+.
     * The inline selection provided by GtkEntry is buggy.
     * If we change the content of the entry, it still stores
     * the old prefix sometimes so things don't work as expected.
     * So, unfortunately, we're not able to use this nice feature.
     *
     * Please see gtk_entry_completion_key_press() of gtk/gtkentry.c
     * and look for completion->priv->completion_prefix.
     */
    gtk_entry_completion_set_inline_selection(completion, FALSE);

    gtk_entry_completion_set_inline_completion(completion, TRUE);
    gtk_entry_completion_set_popup_set_width(completion, TRUE);
    /* gtk_entry_completion_set_popup_single_match(completion, FALSE); */

    /* connect to these signals rather than overriding default handlers since
     * we want to invoke our handlers before the default ones provided by Gtk. */
    g_signal_connect(entry, "key-press-event", G_CALLBACK(fm_path_entry_key_press), NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(fm_path_entry_activate), NULL);
}

static void fm_path_entry_completion_render_func(GtkCellLayout *cell_layout,
                                                 GtkCellRenderer *cell,
                                                 GtkTreeModel *model,
                                                 GtkTreeIter *iter,
                                                 gpointer data)
{
    gchar *model_file_name;
    int model_file_name_len;
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE( FM_PATH_ENTRY(data) );
    gtk_tree_model_get(GTK_TREE_MODEL(model), iter,
                       COL_BASENAME, &model_file_name, -1);
    model_file_name_len = strlen(model_file_name);

    if( priv->highlight_completion_match )
    {
        int buf_len = model_file_name_len + 14 + 1;
        gchar* markup = g_malloc(buf_len);
        gchar *trail = g_stpcpy(markup, "<b><u>");
        trail = strncpy(trail, model_file_name, priv->typed_basename_len) + priv->typed_basename_len;
        trail = g_stpcpy(trail, "</u></b>");
        trail = g_stpcpy(trail, model_file_name + priv->typed_basename_len);
        g_object_set(cell, "markup", markup, NULL);
        g_free(markup);
    }
    /* FIXME: We don't need a custom render func if we don't hightlight */
    else
        g_object_set(cell, "text", model_file_name, NULL);
    g_free(model_file_name);
}

static void
fm_path_entry_finalize(GObject *object)
{
    FmPathEntryPrivate* priv = FM_PATH_ENTRY_GET_PRIVATE(object);

    if(priv->completion)
        g_object_unref(priv->completion);

    if(priv->path)
        fm_path_unref(priv->path);

    g_free(priv->parent_dir);

    if(priv->model)
        g_object_unref(priv->model);

    if(priv->cancellable)
    {
        g_cancellable_cancel(priv->cancellable);
        g_object_unref(priv->cancellable);
    }

    (*G_OBJECT_CLASS(fm_path_entry_parent_class)->finalize)(object);
}

GtkWidget* fm_path_entry_new()
{
    return g_object_new(FM_TYPE_PATH_ENTRY, NULL);
}

void fm_path_entry_set_path(FmPathEntry *entry, FmPath* path)
{
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(entry);
    char* disp_path;

    if(priv->path)
        fm_path_unref(priv->path);
    priv->path = fm_path_ref(path);

    disp_path = fm_path_display_name(path, FALSE);
    /* FIXME: blocks changed signal */
    gtk_entry_set_text(entry, disp_path);
    g_free(disp_path);
}


static gboolean fm_path_entry_match_func(GtkEntryCompletion   *completion,
                                         const gchar          *key,
                                         GtkTreeIter          *iter,
                                         gpointer user_data)
{
    gboolean ret;
    GtkTreeModel *model = gtk_entry_completion_get_model(completion);
    FmPathEntry *entry = FM_PATH_ENTRY( gtk_entry_completion_get_entry(completion) );
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(entry);
    char *model_basename;
    const char* typed_basename;
    /* we don't use the case-insensitive key provided by entry completion here */
    typed_basename = gtk_entry_get_text(entry) + priv->parent_len;
    gtk_tree_model_get(model, iter, COL_BASENAME, &model_basename, -1);

    if(model_basename[0] == '.' && typed_basename[0] != '.')
        ret = FALSE; /* ignore hidden files when needed. */
    else
        ret = g_str_has_prefix(model_basename, typed_basename); /* FIXME: should we be case insensitive here? */
    g_free(model_basename);
    return ret;
}


FmPath* fm_path_entry_get_path(FmPathEntry *entry)
{
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(entry);
    return priv->path;
}

/* custom tree model implementation. */
static void fm_path_entry_model_init(FmPathEntryModel *model)
{
    GType cols[] = {G_TYPE_STRING, G_TYPE_STRING};
    gtk_list_store_set_column_types(GTK_LIST_STORE(model), G_N_ELEMENTS(cols), cols);
}

static void fm_path_entry_model_class_init(FmPathEntryModelClass *klass)
{
}

static void fm_path_entry_model_get_value(GtkTreeModel *tree_model,
                                          GtkTreeIter  *iter,
                                          gint          column,
                                          GValue       *value)
{
    FmPathEntryModel *model = (FmPathEntryModel*)tree_model;
    GtkListStore* store = GTK_LIST_STORE(model);
    if(column == COL_FULL_PATH)
    {
        FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(model->entry);
        char* full_path;
        parent_tree_model_interface->get_value(tree_model, iter, COL_BASENAME, value);
        full_path = g_strconcat(priv->parent_dir, g_value_get_string(value), NULL);
        g_value_take_string(value, full_path);
    }
    else
        parent_tree_model_interface->get_value(tree_model, iter, column, value);
}

static void fm_path_entry_model_iface_init(GtkTreeModelIface *iface)
{
    parent_tree_model_interface = g_type_interface_peek_parent(iface);
    iface->get_value = fm_path_entry_model_get_value;
}

static GtkTreeModel* fm_path_entry_model_new(FmPathEntry* entry)
{
    FmPathEntryModel* model = g_object_new(FM_TYPE_PATH_ENTRY_MODEL, NULL);
    model->entry = entry;
    return model;
}
