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
    char* parent_dirname;
    /* length of parent dir */
    gint parent_len;

    gboolean highlight_completion_match;
    GtkEntryCompletion* completion;

    /* cancellable for dir listing */
    GCancellable* cancellable;

    /* length of basename typed by the user */
    gint typed_basename_len;

    /* automatic common suffix completion */
    gint common_suffix_append_idle_id;
    gchar *common_suffix;
};

typedef struct
{
    FmPathEntry* entry;
    GFile* dir;
    GList* subdirs;
}ListSubDirNames;

/*
 * We only show basenames rather than full paths in the drop down list.
 * So inline autocompletion provided by GtkEntryCompletion does not work
 * since nothing in the list can match the text in entry.
 * We have to handle many things ourselves.
 */

static void      fm_path_entry_activate(GtkEntry *entry, gpointer user_data);
static gboolean  fm_path_entry_key_press(GtkWidget   *widget, GdkEventKey *event, gpointer user_data);
static void      fm_path_entry_class_init(FmPathEntryClass *klass);
static void  fm_path_entry_editable_init(GtkEditableClass *iface);
static gboolean  fm_path_entry_focus_in_event(GtkWidget *widget, GdkEvent  *event);
static gboolean  fm_path_entry_focus_out_event(GtkWidget *widget, GdkEvent  *event);
static void      fm_path_entry_changed(GtkEditable *editable, gpointer user_data);
static void      fm_path_entry_do_insert_text(GtkEditable *editable,
                                              const gchar *new_text,
                                              gint new_text_length,
                                              gint        *position,
                                              gpointer user_data);
static gboolean  fm_path_entry_suffix_append_idle(gpointer user_data);
static void      fm_path_entry_suffix_append_idle_destroy(gpointer user_data);
static char*     fm_path_entry_find_common_suffix(FmPathEntry *entry);
static void      fm_path_entry_init(FmPathEntry *entry);
static void      fm_path_entry_finalize(GObject *object);
static gboolean  fm_path_entry_match_func(GtkEntryCompletion   *completion,
                                          const gchar          *key,
                                          GtkTreeIter          *iter,
                                          gpointer user_data);
static gboolean  fm_path_entry_match_selected(GtkEntryCompletion *widget,
                                              GtkTreeModel       *model,
                                              GtkTreeIter        *iter,
                                              gpointer user_data);
static gboolean  fm_path_entry_insert_prefix(GtkEntryCompletion *widget,
                                             gchar* prefix,
                                             gpointer user_data);
static gboolean  fm_path_entry_cursor_on_match(GtkEntryCompletion *widget,
                                               GtkTreeModel       *model,
                                               GtkTreeIter        *iter,
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

static GtkEditableClass *parent_editable_interface = NULL;

static gboolean fm_path_entry_key_press(GtkWidget   *widget, GdkEventKey *event, gpointer user_data)
{
    FmPathEntry *entry = FM_PATH_ENTRY(widget);
    switch( event->keyval )
    {
    case GDK_Tab:
        /* place the cursor at the end */
        gtk_editable_set_position(GTK_EDITABLE(entry), -1);
        return TRUE;
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

static gboolean  fm_path_entry_focus_in_event(GtkWidget *widget, GdkEvent  *event)
{
    FmPathEntry *entry = FM_PATH_ENTRY(widget);
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);
    /* activate auto-completion */
    gtk_entry_set_completion(entry, priv->completion);
    return GTK_WIDGET_CLASS(fm_path_entry_parent_class)->focus_in_event(widget, event);
}

static gboolean  fm_path_entry_focus_out_event(GtkWidget *widget, GdkEvent  *event)
{
    FmPathEntry *entry = FM_PATH_ENTRY(widget);
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);
    /* de-activate auto-completion */
    gtk_entry_set_completion(entry, NULL);
    if(priv->cancellable) /* cancel dir listing in progress */
        g_cancellable_cancel(priv->cancellable);
    return GTK_WIDGET_CLASS(fm_path_entry_parent_class)->focus_out_event(widget, event);
}

static void on_dir_list_finished(gpointer user_data)
{
    ListSubDirNames* data = (ListSubDirNames*)user_data;
    FmPathEntry* entry = data->entry;
    FmPathEntryPrivate *priv  = FM_PATH_ENTRY_GET_PRIVATE(entry);
    GList* l;
    GtkListStore* new_model = gtk_list_store_new(1, G_TYPE_STRING);
    /* g_debug("dir list is finished!"); */

    /* update the model */
    for(l = data->subdirs; l; l=l->next)
    {
        char* name = l->data;
        gtk_list_store_insert_with_values(new_model, NULL, -1, 0, name, -1);
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

    /* don't touch auto-completion if we don't have input focus */
    if(!GTK_WIDGET_HAS_FOCUS(widget))
        return;

    /* find parent dir of current path */
    path_str = gtk_entry_get_text( GTK_ENTRY(entry) );
    sep = g_utf8_strrchr(path_str, -1, G_DIR_SEPARATOR);
    if(sep) /* we found a parent dir */
    {
        int parent_len = (sep - path_str);
        if(parent_len == 0) /* special case for / */
            parent_len = 1;

        if(!priv->parent_dirname
           || priv->parent_len != parent_len
           || strncmp(priv->parent_dirname, path_str, parent_len ))
        {
            /* parent dir has been changed, reload dir list */
            ListSubDirNames* data = g_slice_new0(ListSubDirNames);
            g_free(priv->parent_dirname);
            priv->parent_dirname = g_strndup(path_str, parent_len);
            priv->parent_len = parent_len;
            /* g_debug("parent dir is changed to %s", priv->parent_dirname); */

            data->entry = entry;
            if(priv->parent_dirname[0] == '~') /* special case for home dir */
            {
                char* expand = g_strconcat(g_get_home_dir(), priv->parent_dirname + 1, NULL);
                data->dir = g_file_new_for_commandline_arg(expand);
                g_free(expand);
            }
            else
                data->dir = g_file_new_for_commandline_arg(priv->parent_dirname);

            /* clear current model */
            gtk_list_store_clear(GTK_LIST_STORE(priv->model));

            /* cancel running dir-listing jobs */
            g_cancellable_cancel(priv->cancellable);

            /* launch a new job to do dir listing */
            g_cancellable_reset(priv->cancellable);
            g_io_scheduler_push_job(list_sub_dirs,
                                    data, (GDestroyNotify)list_sub_dir_names_free,
                                    G_PRIORITY_LOW, priv->cancellable);
        }

        /* calculate the length of remaining part after / */
        priv->typed_basename_len = strlen(sep + 1);
    }
    else
    {
        priv->parent_len = 0;
        g_free(priv->parent_dirname);
        priv->parent_dirname = NULL;
        /* cancel running dir-listing jobs */
        g_cancellable_cancel(priv->cancellable);
        /* clear current model */
        gtk_list_store_clear(GTK_LIST_STORE(priv->model));

        priv->typed_basename_len = 0;
        g_free(priv->common_suffix);
        priv->common_suffix = NULL;
        if(priv->common_suffix_append_idle_id >=0)
        {
            g_source_remove(priv->common_suffix_append_idle_id);
            priv->common_suffix_append_idle_id = -1;
        }
    }
}

static void fm_path_entry_do_insert_text(GtkEditable *editable, const gchar *new_text,
                                         gint new_text_length, gint *position,
                                         gpointer user_data)
{
    FmPathEntry *entry = FM_PATH_ENTRY(editable);
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(entry);

    /* inline autocompletion */
    if( GTK_WIDGET_HAS_FOCUS(editable) && priv->model )
    {
        /* we have a common suffix -> add idle function */
        g_free(priv->common_suffix);
        priv->common_suffix = fm_path_entry_find_common_suffix(entry);
        if( (priv->common_suffix_append_idle_id < 0) && priv->common_suffix )
            priv->common_suffix_append_idle_id  = g_idle_add_full(G_PRIORITY_HIGH,
                                                                  fm_path_entry_suffix_append_idle,
                                                                  entry, NULL);
    }
}

static gboolean fm_path_entry_suffix_append_idle(gpointer user_data)
{
    FmPathEntry *entry = FM_PATH_ENTRY(user_data);
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(entry);
    const gchar *original_key = gtk_entry_get_text( GTK_ENTRY(entry) );

    /* we have a common suffix -> insert/select it */
    if( priv->common_suffix )
    {
        gint suffix_offset = g_utf8_strlen(original_key, -1);
        gint suffix_offset_save = suffix_offset;

        /* don't recur */
        g_signal_handlers_block_by_func(entry, fm_path_entry_changed, NULL);
        g_signal_handlers_block_by_func(entry, fm_path_entry_do_insert_text, NULL);

        gtk_editable_insert_text(GTK_EDITABLE(entry), priv->common_suffix + priv->typed_basename_len, -1,  &suffix_offset);
        gtk_editable_select_region(GTK_EDITABLE(entry), suffix_offset_save, -1);

        g_signal_handlers_unblock_by_func(entry, fm_path_entry_changed, NULL);
        g_signal_handlers_unblock_by_func(entry, fm_path_entry_do_insert_text, NULL);
    }
    priv->common_suffix_append_idle_id = -1;
    /* don't call again */
    return FALSE;
}

static char* fm_path_entry_find_common_suffix(FmPathEntry *entry)
{
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(entry);
    GtkTreeIter iter;
    gchar *prefix = NULL;
    gboolean valid;
    const gchar *key;

    key = gtk_entry_get_text(GTK_ENTRY(entry));
    key += priv->parent_len;
    while(*key == G_DIR_SEPARATOR)
        ++key;

    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->model),
                     &iter);
    while (valid)
    {
        gchar *text;
        gtk_tree_model_get(GTK_TREE_MODEL(priv->model), &iter, 0, &text, -1);
        if (text && g_str_has_prefix (text, key))
        {
            if (!prefix)
                prefix = g_strdup (text);
            else
            {
                gchar *p = prefix;
                gchar *q = text;
                while (*p && *p == *q)
                {
                    p++;
                    q++;
                }
                *p = '\0';
                if (p > prefix)
                {
                    /* strip a partial multibyte character */
                    q = g_utf8_find_prev_char (prefix, p);
                    switch(g_utf8_get_char_validated(q, p - q))
                    {
                    case (gunichar)-2:
                    case (gunichar)-1:
                      *q = 0;
                    default: ;
                    }
                }
            }
        }
        g_free (text);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->model), &iter);
    }
    return prefix;
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

    priv->model = gtk_list_store_new(1, G_TYPE_STRING);
    priv->completion = completion;
    priv->cancellable = g_cancellable_new();
    priv->highlight_completion_match = TRUE;
    priv->common_suffix_append_idle_id = -1;
    priv->common_suffix = NULL;
    gtk_entry_completion_set_minimum_key_length(completion, 1);

    gtk_entry_completion_set_match_func(completion, fm_path_entry_match_func, NULL, NULL);
    g_signal_connect(completion, "match-selected", G_CALLBACK(fm_path_entry_match_selected), (gpointer)NULL);
    g_signal_connect(completion, "insert-prefix", G_CALLBACK(fm_path_entry_insert_prefix), (gpointer)NULL);
    g_signal_connect(completion, "cursor-on-match", G_CALLBACK(fm_path_entry_cursor_on_match), (gpointer)NULL);
    g_object_set(completion, "text_column", 0, NULL);

    gtk_entry_completion_set_model(completion, priv->model);

    render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( (GtkCellLayout*)completion, render, TRUE );
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(completion), render, fm_path_entry_completion_render_func, entry, NULL);
    gtk_entry_completion_set_inline_completion(completion, TRUE);
    gtk_entry_completion_set_popup_set_width(completion, TRUE);
    gtk_entry_completion_set_popup_single_match(completion, FALSE);

    /* connect to these signals rather than overriding default handlers since
     * we want to invoke our handlers before the default ones provided by Gtk. */
    g_signal_connect(entry, "key-press-event", G_CALLBACK(fm_path_entry_key_press), NULL);
    g_signal_connect(entry, "changed", G_CALLBACK(fm_path_entry_changed), NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(fm_path_entry_activate), NULL);
    g_signal_connect_after(entry, "insert-text", G_CALLBACK(fm_path_entry_do_insert_text), NULL);
}

static void fm_path_entry_completion_render_func(GtkCellLayout *cell_layout,
                                                 GtkCellRenderer *cell,
                                                 GtkTreeModel *model,
                                                 GtkTreeIter *iter,
                                                 gpointer data)
{
    gchar *model_file_name;
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE( FM_PATH_ENTRY(data) );
    gtk_tree_model_get(GTK_TREE_MODEL(model), iter,
                       0, &model_file_name, -1);
    if( priv->highlight_completion_match )
    {
        int buf_len = strlen(model_file_name) + 14 + 1;
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

    g_free(priv->parent_dirname);
    g_free(priv->common_suffix);

    if(priv->model)
        g_object_unref(priv->model);

    if(priv->cancellable)
    {
        g_cancellable_cancel(priv->cancellable);
        g_object_unref(priv->cancellable);
    }

    /* drop idle func for suffix completion */
    if( priv->common_suffix_append_idle_id > 0 )
        g_source_remove(priv->common_suffix_append_idle_id);

    (*G_OBJECT_CLASS(fm_path_entry_parent_class)->finalize)(object);
}

GtkWidget* fm_path_entry_new()
{
    return g_object_new(FM_TYPE_PATH_ENTRY, NULL);
}

/* deprecated, kept for backward compatibility only. */
void fm_path_entry_set_model(FmPathEntry *entry, FmPath* path, FmFolderModel* model)
{
    fm_path_entry_set_path(entry, path);
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
    char* name;
    /* we don't use the case-insensitive key provided by entry completion here */
    key = gtk_entry_get_text(entry) + priv->parent_len;
    while(*key == G_DIR_SEPARATOR)
        ++key;
    gtk_tree_model_get(model, iter, 0, &name, -1);
    if(name[0] == '.' && key[0] != '.')
        ret = FALSE; /* ignore hidden files when needed. */
    else if(*name == '\0')
        ret = TRUE;
    else
        ret = g_str_has_prefix(name, key); /* FIXME: should we be case insensitive here? */
    g_free(name);
    return ret;
}

static gboolean  fm_path_entry_match_selected(GtkEntryCompletion *widget,
                                              GtkTreeModel       *model,
                                              GtkTreeIter        *iter,
                                              gpointer user_data)
{
    GtkWidget *entry = gtk_entry_completion_get_entry(widget);
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(entry);
    char* name;
    char* full_path;
    FmPath* parent_path;

    gtk_tree_model_get(model, iter, 0, &name, -1);
    full_path = g_build_filename(priv->parent_dirname, name, NULL);
    g_free(name);

    name = g_filename_display_name(full_path);
    g_free(full_path);
    gtk_entry_set_text(entry, name);
    g_free(name);

    /* move the cursor to the end of entry */
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
    return TRUE;
}

static gboolean  fm_path_entry_cursor_on_match(GtkEntryCompletion *widget,
                                               GtkTreeModel       *model,
                                               GtkTreeIter        *iter,
                                               gpointer user_data)
{
    /* FIXME: why this doesn't work?
     * Maybe we only store basenames rather than full paths in model.
     * So gtk+ cannot match them automatically and this never works. */
    g_debug("cursor on match");
    return TRUE;
}

static gboolean  fm_path_entry_insert_prefix(GtkEntryCompletion *widget,
                                             gchar* prefix,
                                             gpointer user_data)
{
    /* FIXME: why this doesn't work?
     * Maybe we only store basenames rather than full paths in model.
     * So gtk+ cannot match them automatically and this never works. */
    g_debug("insert prefix: %s", prefix);
    return TRUE;
}

FmPath* fm_path_entry_get_path(FmPathEntry *entry)
{
    FmPathEntryPrivate *priv = FM_PATH_ENTRY_GET_PRIVATE(entry);
    return priv->path;
}
