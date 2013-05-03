/*
 *      fm-dentry-properties.c
 *
 *      Copyright 2008  <pcman.tw@gmail.com>
 *      Copyright 2013 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

/* File properties dialog extension for desktop entry type */

/* FIXME: this should be module, not part of libfm-gtk */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "fm-file-properties.h"

#include "gtk-compat.h"

#define GRP_NAME "Desktop Entry"

typedef struct _FmFilePropertiesDEntryData FmFilePropertiesDEntryData;
struct _FmFilePropertiesDEntryData
{
    GFile *file;
    GKeyFile *kf;
    GtkImage *icon;
    GtkEntry *name;
    GtkEntry *comment;
    GtkEntry *exec;
    GtkEntry *generic_name;
    GtkEntry *path;
    GtkToggleButton *terminal;
    GtkToggleButton *notification;
    gchar *lang;
    gboolean changed;
};

/* this handler is taken from lxshortcut and modified a bit */
static void on_update_preview(GtkFileChooser* chooser, GtkImage* img)
{
    char *file = gtk_file_chooser_get_preview_filename(chooser);
    if (file)
    {
        GdkPixbuf *pix = gdk_pixbuf_new_from_file_at_scale(file, 48, 48, TRUE, NULL);
        if (pix)
        {
            gtk_image_set_from_pixbuf(img, pix);
            g_object_unref(pix);
            return;
        }
    }
    gtk_image_clear(img);
}

static void on_toggle_theme(GtkToggleButton *btn, GtkNotebook *notebook)
{
    gtk_notebook_set_current_page(notebook, 0);
}

static void on_toggle_files(GtkToggleButton *btn, GtkNotebook *notebook)
{
    gtk_notebook_set_current_page(notebook, 1);
}

static GdkPixbuf *vfs_load_icon(GtkIconTheme *theme, const char *icon_name, int size)
{
    GdkPixbuf *icon = NULL;
    const char *file;
    GtkIconInfo *inf = gtk_icon_theme_lookup_icon(theme, icon_name, size,
                                                  GTK_ICON_LOOKUP_USE_BUILTIN);
    if (G_UNLIKELY(!inf))
        return NULL;

    file = gtk_icon_info_get_filename(inf);
    if (G_LIKELY(file))
        icon = gdk_pixbuf_new_from_file_at_scale(file, size, size, TRUE, NULL);
    else
    {
        icon = gtk_icon_info_get_builtin_pixbuf(inf);
        g_object_ref(icon);
    }
    gtk_icon_info_free(inf);

    if (G_LIKELY(icon))  /* scale down the icon if it's too big */
    {
        int width, height;
        height = gdk_pixbuf_get_height(icon);
        width = gdk_pixbuf_get_width(icon);

        if (G_UNLIKELY(height > size || width > size))
        {
            GdkPixbuf *scaled;
            if (height > width)
            {
                width = size * height / width;
                height = size;
            }
            else if (height < width)
            {
                height = size * width / height;
                width = size;
            }
            else
                height = width = size;
            scaled = gdk_pixbuf_scale_simple(icon, width, height, GDK_INTERP_BILINEAR);
            g_object_unref(icon);
            icon = scaled;
        }
    }
    return icon;
}

typedef struct {
    GtkIconView *view;
    GtkListStore *model;
    GAsyncQueue *queue;
} IconThreadData;

static gpointer load_themed_icon(GtkIconTheme *theme, IconThreadData *data)
{
    GdkPixbuf *pix;
    char *icon_name = g_async_queue_pop(data->queue);

    gdk_threads_enter();
    pix = vfs_load_icon(theme, icon_name, 48);
    gdk_threads_leave();
    g_thread_yield();
    if (pix)
    {
        GtkTreeIter it;
        gdk_threads_enter();
        gtk_list_store_append(data->model, &it);
        gtk_list_store_set(data->model, &it, 0, pix, 1, icon_name, -1);
        g_object_unref(pix);
        gdk_threads_leave();
    }
    g_thread_yield();
    if (g_async_queue_length(data->queue) == 0)
    {
        gdk_threads_enter();
        if (gtk_icon_view_get_model(data->view) == NULL)
        {
            gtk_icon_view_set_model(data->view, GTK_TREE_MODEL(data->model));
#if GTK_CHECK_VERSION(2, 20, 0)
            if (gtk_widget_get_realized(GTK_WIDGET(data->view)))
                gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(data->view)), NULL);
#else
            if (GTK_WIDGET_REALIZED(GTK_WIDGET(data->view)))
                gdk_window_set_cursor(GTK_WIDGET(data->view)->window, NULL);
#endif
        }
        gdk_threads_leave();
    }
    /* g_debug("load: %s", icon_name); */
    g_free(icon_name);
    return NULL;
}

static void _change_icon(GtkWidget *dlg, FmFilePropertiesDEntryData *data)
{
    GtkBuilder *builder;
    GtkFileChooser *chooser;
    GtkWidget *chooser_dlg, *preview, *notebook;
    GtkFileFilter *filter;
    GtkIconTheme *theme;
    GList *contexts, *l;
    GThreadPool *thread_pool;
    IconThreadData thread_data;

    builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/choose-icon.ui", NULL);
    chooser_dlg = GTK_WIDGET(gtk_builder_get_object(builder, "dlg"));
    chooser = GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "chooser"));
    thread_data.view = GTK_ICON_VIEW(gtk_builder_get_object(builder, "icons"));
    notebook = GTK_WIDGET(gtk_builder_get_object(builder, "notebook"));
    g_signal_connect(gtk_builder_get_object(builder,"theme"), "toggled", G_CALLBACK(on_toggle_theme), notebook);
    g_signal_connect(gtk_builder_get_object(builder,"files"), "toggled", G_CALLBACK(on_toggle_files), notebook);

    gtk_window_set_default_size(GTK_WINDOW(chooser_dlg), 600, 440);
    gtk_window_set_transient_for(GTK_WINDOW(chooser_dlg), GTK_WINDOW(dlg));

    preview = gtk_image_new();
    gtk_widget_show(preview);
    gtk_file_chooser_set_preview_widget(chooser, preview);
    g_signal_connect(chooser, "update-preview", G_CALLBACK(on_update_preview),
                     GTK_IMAGE(preview));

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(GTK_FILE_FILTER(filter), _("Image files"));
    gtk_file_filter_add_pixbuf_formats(GTK_FILE_FILTER(filter));
    gtk_file_chooser_add_filter(chooser, filter);
    gtk_file_chooser_set_local_only(chooser, TRUE);
    gtk_file_chooser_set_select_multiple(chooser, FALSE);
    gtk_file_chooser_set_use_preview_label(chooser, FALSE);

    gtk_widget_show(chooser_dlg);
    while (gtk_events_pending())
        gtk_main_iteration();

    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(thread_data.view)),
                          gdk_cursor_new(GDK_WATCH));

    /* load themed icons */
    thread_pool = g_thread_pool_new((GFunc)load_themed_icon, &thread_data, 1, TRUE, NULL);
    g_thread_pool_set_max_threads(thread_pool, 1, NULL);
    thread_data.queue = g_async_queue_new();

    thread_data.model = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
    theme = gtk_icon_theme_get_default();

    gtk_icon_view_set_pixbuf_column(thread_data.view, 0);
    gtk_icon_view_set_item_width(thread_data.view, 80);
    gtk_icon_view_set_text_column(thread_data.view, 1);

    /* GList* contexts = gtk_icon_theme_list_contexts(theme); */
    contexts = g_list_alloc();
    /* FIXME: we should enable more contexts */
    /* http://standards.freedesktop.org/icon-naming-spec/icon-naming-spec-latest.html#context */
    contexts->data = g_strdup("Applications");
    for (l = contexts; l; l = l->next)
    {
        /* g_debug(l->data); */
        GList *icon_names = gtk_icon_theme_list_icons(theme, (char*)l->data);
        GList *icon_name;
        for (icon_name = icon_names; icon_name; icon_name = icon_name->next)
        {
            g_async_queue_push(thread_data.queue, icon_name->data);
            g_thread_pool_push(thread_pool, theme, NULL);
        }
        g_list_free(icon_names);
        g_free(l->data);
    }
    g_list_free(contexts);

    if (gtk_dialog_run(GTK_DIALOG(chooser_dlg)) == GTK_RESPONSE_OK)
    {
        char* icon_name = NULL;
        if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) == 0)
        {
            GList *sels = gtk_icon_view_get_selected_items(thread_data.view);
            GtkTreePath *tp = (GtkTreePath*)sels->data;
            GtkTreeIter it;
            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(thread_data.model), &it, tp))
            {
                gtk_tree_model_get(GTK_TREE_MODEL(thread_data.model), &it, 1, &icon_name, -1);
            }
            g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
            g_list_free(sels);
            if (icon_name)
                gtk_image_set_from_icon_name(data->icon, icon_name, GTK_ICON_SIZE_DIALOG);
        }
        else
        {
            icon_name = gtk_file_chooser_get_filename(chooser);
            if (icon_name)
            {
                GdkPixbuf *pix = gdk_pixbuf_new_from_file_at_scale(icon_name,
                                                            48, 48, TRUE, NULL);
                if (pix)
                {
                    gtk_image_set_from_pixbuf(data->icon, pix);
                    g_object_unref(pix);
                }
            }
        }
        if (icon_name)
        {
            g_key_file_set_string(data->kf, GRP_NAME, "Icon", icon_name);
            data->changed = TRUE;
            g_free(icon_name);
        }
    }
    g_thread_pool_free(thread_pool, TRUE, FALSE);
    gtk_widget_destroy(chooser_dlg);
}


static gboolean _dentry_icon_click_event(GtkWidget *widget, GdkEventButton *event,
                                         FmFilePropertiesDEntryData *data)
{
    /* g_debug("icon click received (button=%d)", event->button); */
    if (event->button == 1) /* accept only left click */
        _change_icon(gtk_widget_get_toplevel(widget), data);
    return FALSE;
}

static gboolean _dentry_icon_press_event(GtkWidget *widget, GdkEventKey *event,
                                         FmFilePropertiesDEntryData *data)
{
    /* g_debug("icon key received (key=%u)", event->keyval); */
    if (event->keyval == GDK_KEY_space)
        _change_icon(gtk_widget_get_toplevel(widget), data);
    return FALSE;
}

static gboolean exe_filter(const GtkFileFilterInfo *inf, gpointer user_data)
{
    return g_file_test(inf->filename, G_FILE_TEST_IS_EXECUTABLE);
}

static void _dentry_browse_exec_event(GtkButton *button, FmFilePropertiesDEntryData *data)
{
    /* g_debug("browse button pressed"); */
    /* this handler is also taken from lxshortcut */
    GtkWidget *chooser;
    GtkFileFilter *filter;

    chooser = gtk_file_chooser_dialog_new(_("Choose an executable file"),
                                          NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), "/usr/bin");
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(GTK_FILE_FILTER(filter), _("Executable files") );
    gtk_file_filter_add_custom(GTK_FILE_FILTER(filter), GTK_FILE_FILTER_FILENAME,
                               exe_filter, NULL, NULL);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_OK)
    {
        char *file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        gtk_entry_set_text(data->exec, file);
        g_free(file);
    }
    gtk_widget_destroy(chooser);
}

static void _dentry_name_changed(GtkEditable *editable, FmFilePropertiesDEntryData *data)
{
    /* g_debug("entry content changed"); */
    if (data->lang)
        g_key_file_set_locale_string(data->kf, GRP_NAME, "Name", data->lang,
                                     gtk_entry_get_text(GTK_ENTRY(editable)));
    else
        g_key_file_set_string(data->kf, GRP_NAME, "Name",
                              gtk_entry_get_text(GTK_ENTRY(editable)));
    data->changed = TRUE;
}

static void _dentry_tooltip_changed(GtkEditable *editable, FmFilePropertiesDEntryData *data)
{
    /* g_debug("entry content changed"); */
    if (data->lang)
        g_key_file_set_locale_string(data->kf, GRP_NAME, "Comment", data->lang,
                                     gtk_entry_get_text(GTK_ENTRY(editable)));
    else
        g_key_file_set_string(data->kf, GRP_NAME, "Comment",
                              gtk_entry_get_text(GTK_ENTRY(editable)));
    data->changed = TRUE;
}

static void _dentry_exec_changed(GtkEditable *editable, FmFilePropertiesDEntryData *data)
{
    /* g_debug("entry content changed"); */
    g_key_file_set_string(data->kf, GRP_NAME, "Exec",
                          gtk_entry_get_text(GTK_ENTRY(editable)));
    data->changed = TRUE;
}

static void _dentry_genname_changed(GtkEditable *editable, FmFilePropertiesDEntryData *data)
{
    /* g_debug("entry content changed"); */
    g_key_file_set_string(data->kf, GRP_NAME, "GenericName",
                          gtk_entry_get_text(GTK_ENTRY(editable)));
    data->changed = TRUE;
}

static void _dentry_path_changed(GtkEditable *editable, FmFilePropertiesDEntryData *data)
{
    /* g_debug("entry content changed"); */
    g_key_file_set_string(data->kf, GRP_NAME, "Path",
                          gtk_entry_get_text(GTK_ENTRY(editable)));
    data->changed = TRUE;
}

static void _dentry_terminal_toggled(GtkToggleButton *togglebutton,
                                     FmFilePropertiesDEntryData *data)
{
    /* g_debug("run in terminal toggled"); */
    g_key_file_set_boolean(data->kf, GRP_NAME, "Terminal",
                           gtk_toggle_button_get_active(togglebutton));
    data->changed = TRUE;
}

static void _dentry_notification_toggled(GtkToggleButton *togglebutton,
                                         FmFilePropertiesDEntryData *data)
{
    /* g_debug("startup notification toggled"); */
    g_key_file_set_boolean(data->kf, GRP_NAME, "StartupNotify",
                           gtk_toggle_button_get_active(togglebutton));
    data->changed = TRUE;
}

static gpointer _dentry_ui_init(GtkBuilder *ui, gpointer uidata, FmFileInfoList *files)
{
    GObject *widget;
    GtkWidget *new_widget;
    FmFilePropertiesDEntryData *data;
    GtkTable *table;
    GtkLabel *label;
    GError *err = NULL;
    GFile *gf;
    gchar *file_path, *txt;
    const gchar * const *langs;
    gboolean tmp_bool;

    /* disable permissions tab in any case */
#define HIDE_WIDGET(x) widget = gtk_builder_get_object(ui, x); \
        gtk_widget_hide(GTK_WIDGET(widget))
    /* HIDE_WIDGET("permissions_tab");
       TODO: made visibility of permissions_tab configurable */
    /* we will do the thing only for single file! */
    if (fm_file_info_list_get_length(files) != 1)
        return NULL;
    gf = fm_path_to_gfile(fm_file_info_get_path(fm_file_info_list_peek_head(files)));
    file_path = g_file_get_path(gf);
    /* FIXME: load not file but stream */
    if (file_path == NULL)
    {
        g_warning("file properties dialog: cannot access desktop entry file");
        g_object_unref(gf);
        return NULL;
    }
    data = g_slice_new(FmFilePropertiesDEntryData);
    data->changed = FALSE;
    data->file = gf;
    data->kf = g_key_file_new();
    g_key_file_load_from_file(data->kf, file_path,
                              G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                              NULL);
    g_free(file_path);
    /* FIXME: handle errors, also do g_key_file_has_group() */
    /* get locale name */
    data->lang = NULL;
    langs = g_get_language_names();
    if (strcmp(langs[0], "C") != 0)
    {
        /* remove encoding from locale name */
        char *sep = strchr(langs[0], '.');
        if (sep)
            data->lang = g_strndup(langs[0], sep - langs[0]);
        else
            data->lang = g_strdup(langs[0]);
    }
    /* set events handlers for icon */
    table = GTK_TABLE(gtk_builder_get_object(ui, "general_table"));
    widget = gtk_builder_get_object(ui, "icon_eventbox");
    data->icon = GTK_IMAGE(gtk_builder_get_object(ui, "icon"));
    gtk_widget_set_can_focus(GTK_WIDGET(widget), TRUE);
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(_dentry_icon_click_event), data);
    g_signal_connect(widget, "key-press-event",
                     G_CALLBACK(_dentry_icon_press_event), data);
    /* disable Name event handler in the widget */
    widget = gtk_builder_get_object(ui, "name");
    g_signal_handlers_block_matched(widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, uidata);
    g_signal_connect(widget, "changed", G_CALLBACK(_dentry_name_changed), data);
    /* FIXME: two lines below is temporary workaround on FIXME in widget */
    gtk_widget_set_can_focus(GTK_WIDGET(widget), TRUE);
    gtk_editable_set_editable(GTK_EDITABLE(widget), TRUE);
    /* Name is set from "Name" by libfm already so don't touch it */
    /* show desktop entry file name */
    widget = gtk_builder_get_object(ui, "file_label");
    gtk_label_set_markup(GTK_LABEL(widget), _("<b>File:</b>"));
    gtk_widget_show(GTK_WIDGET(widget));
    widget = gtk_builder_get_object(ui, "file");
    gtk_label_set(GTK_LABEL(widget),
                  fm_file_info_get_name(fm_file_info_list_peek_head(files)));
    gtk_widget_show(GTK_WIDGET(widget));
    /* hide 'Open with' choose box */
    HIDE_WIDGET("open_with");
    HIDE_WIDGET("open_with_label");
    gtk_table_set_row_spacing(table, 5, 0);
#undef HIDE_WIDGET
    /* FIXME: migrate to GtkGrid */
    table = GTK_TABLE(gtk_table_new(8, 2, FALSE));
    gtk_table_set_row_spacings(table, 4);
    gtk_table_set_col_spacings(table, 12);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    /* row 0: "Exec" GtkHBox: GtkEntry+GtkButton */
    new_widget = gtk_label_new(NULL);
    label = GTK_LABEL(new_widget);
    gtk_misc_set_alignment(GTK_MISC(new_widget), 0.0, 0.0);
    gtk_label_set_markup_with_mnemonic(label, _("<b>Co_mmand:</b>"));
    gtk_table_attach(table, new_widget, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
#if GTK_CHECK_VERSION(3, 2, 0)
    /* FIXME: migrate to GtkGrid */
    widget = G_OBJECT(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
#else
    widget = G_OBJECT(gtk_hbox_new(FALSE, 6));
#endif
    new_widget = gtk_button_new_with_mnemonic(_("_Browse..."));
    gtk_box_pack_end(GTK_BOX(widget), new_widget, FALSE, FALSE, 0);
    g_signal_connect(new_widget, "clicked",
                     G_CALLBACK(_dentry_browse_exec_event), data);
    new_widget = gtk_entry_new();
    data->exec = GTK_ENTRY(new_widget);
    txt = g_key_file_get_locale_string(data->kf, GRP_NAME, "Exec", NULL, NULL);
    if (txt)
        gtk_entry_set_text(data->exec, txt);
    gtk_widget_set_tooltip_text(new_widget,
                                _("Command to execute when the application icon is activated"));
    gtk_box_pack_start(GTK_BOX(widget), new_widget, TRUE, TRUE, 0);
    g_signal_connect(new_widget, "changed", G_CALLBACK(_dentry_exec_changed), data);
    gtk_table_attach(table, GTK_WIDGET(widget), 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(label, new_widget);
    /* row 1: "Terminal" GtkCheckButton */
    new_widget = gtk_check_button_new_with_mnemonic(_("_Run in terminal emulator"));
    data->terminal = GTK_TOGGLE_BUTTON(new_widget);
    tmp_bool = g_key_file_get_boolean(data->kf, GRP_NAME, "Terminal", &err);
    if (err) /* no such key present */
    {
        tmp_bool = FALSE;
        g_clear_error(&err);
    }
    gtk_toggle_button_set_active(data->terminal, tmp_bool);
    g_signal_connect(new_widget, "toggled", G_CALLBACK(_dentry_terminal_toggled), data);
    gtk_table_attach(table, new_widget, 0, 2, 1, 2, GTK_FILL, 0, 18, 0);
    /* FIXME: add checkbox for 'Leave terminal after exit' */
    /* row 4: "GenericName" GtkEntry */
    new_widget = gtk_label_new(NULL);
    label = GTK_LABEL(new_widget);
    gtk_misc_set_alignment(GTK_MISC(new_widget), 0.0, 0.0);
    gtk_label_set_markup_with_mnemonic(label, _("<b>D_escription:</b>"));
    gtk_table_attach(table, new_widget, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);
    new_widget = gtk_entry_new();
    data->generic_name = GTK_ENTRY(new_widget);
    txt = g_key_file_get_locale_string(data->kf, GRP_NAME, "GenericName", NULL, NULL);
    if (txt)
        gtk_entry_set_text(data->generic_name, txt);
    gtk_widget_set_tooltip_text(new_widget, _("Generic name of the application"));
    g_signal_connect(new_widget, "changed", G_CALLBACK(_dentry_genname_changed), data);
    gtk_table_attach(table, new_widget, 1, 2, 4, 5, GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(label, new_widget);
    /* row 3: "Path" GtkEntry */
    new_widget = gtk_label_new(NULL);
    label = GTK_LABEL(new_widget);
    gtk_misc_set_alignment(GTK_MISC(new_widget), 0.0, 0.0);
    gtk_label_set_markup_with_mnemonic(label, _("<b>_Working directory:</b>"));
    gtk_table_attach(table, new_widget, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
    new_widget = gtk_entry_new();
    data->path = GTK_ENTRY(new_widget);
    txt = g_key_file_get_locale_string(data->kf, GRP_NAME, "Path", NULL, NULL);
    if (txt)
        gtk_entry_set_text(data->path, txt);
    gtk_widget_set_tooltip_text(new_widget,
                                _("The working directory to run the program in"));
    g_signal_connect(new_widget, "changed", G_CALLBACK(_dentry_path_changed), data);
    gtk_table_attach(table, new_widget, 1, 2, 3, 4, GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(label, new_widget);
    /* row 5: "Comment" GtkEntry */
    new_widget = gtk_label_new(NULL);
    label = GTK_LABEL(new_widget);
    gtk_misc_set_alignment(GTK_MISC(new_widget), 0.0, 0.0);
    gtk_label_set_markup_with_mnemonic(label, _("<b>_Tooltip:</b>"));
    gtk_table_attach(table, new_widget, 0, 1, 5, 6, GTK_FILL, 0, 0, 0);
    new_widget = gtk_entry_new();
    data->comment = GTK_ENTRY(new_widget);
    txt = g_key_file_get_locale_string(data->kf, GRP_NAME, "Comment", NULL, NULL);
    if (txt)
        gtk_entry_set_text(data->comment, txt);
    gtk_widget_set_tooltip_text(new_widget, _("Tooltip to show on application"));
    g_signal_connect(new_widget, "changed", G_CALLBACK(_dentry_tooltip_changed), data);
    gtk_table_attach(table, new_widget, 1, 2, 5, 6, GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(label, new_widget);
    /* TODO: handle "TryExec" field ? */
    /* row 7: "StartupNotify" GtkCheckButton */
    new_widget = gtk_check_button_new_with_mnemonic(_("_Use startup notification"));
    data->notification = GTK_TOGGLE_BUTTON(new_widget);
    tmp_bool = g_key_file_get_boolean(data->kf, GRP_NAME, "StartupNotify", &err);
    if (err) /* no such key present */
    {
        tmp_bool = FALSE;
        g_clear_error(&err);
    }
    gtk_toggle_button_set_active(data->notification, tmp_bool);
    g_signal_connect(new_widget, "toggled", G_CALLBACK(_dentry_notification_toggled), data);
    gtk_table_attach(table, new_widget, 0, 2, 7, 8, GTK_FILL, 0, 0, 0);
    /* put the table into third tab and enable it */
    widget = gtk_builder_get_object(ui, "extra_tab_label");
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(widget), "_Desktop entry");
    widget = gtk_builder_get_object(ui, "extra_tab");
    gtk_container_add(GTK_CONTAINER(widget), GTK_WIDGET(table));
    gtk_widget_show_all(GTK_WIDGET(widget));
    return data;
}

static void _dentry_ui_finish(gpointer pdata, gboolean cancelled)
{
    FmFilePropertiesDEntryData *data = pdata;
    gsize len, olen;
    char *text;
    GOutputStream *out;

    if (data == NULL)
        return;
    if (!cancelled && data->changed)
    {
        text = g_key_file_to_data(data->kf, &len, NULL);
        out = G_OUTPUT_STREAM(g_file_replace(data->file, NULL, FALSE, 0, NULL, NULL));
        /* FIXME: handle errors */
        g_output_stream_write_all(out, text, len, &olen, NULL, NULL);
        /* FIXME: handle errors */
        g_output_stream_close(out, NULL, NULL);
        g_object_unref(out);
        g_free(text);
    }
    g_object_unref(data->file);
    g_key_file_free(data->kf);
    g_free(data->lang);
    g_slice_free(FmFilePropertiesDEntryData, data);
}

static FmFilePropertiesExtensionInit _callbacks = {
    &_dentry_ui_init,
    &_dentry_ui_finish
};

void _fm_dentry_properties_init(void)
{
    fm_file_properties_add_for_mime_type("application/x-desktop", &_callbacks);
}
