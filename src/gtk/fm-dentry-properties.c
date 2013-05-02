/*
 *      fm-dentry-properties.c
 *
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

#include "fm-file-properties.h"

#define GRP_NAME "Desktop Entry"

typedef struct _FmFilePropertiesDEntryData FmFilePropertiesDEntryData;
struct _FmFilePropertiesDEntryData
{
    GFile *file;
    GKeyFile *kf;
    GtkEntry *name;
    GtkEntry *comment;
    GtkEntry *exec;
    GtkToggleButton *terminal;
    gchar *lang;
    gboolean changed;
};

static gboolean _dentry_icon_click_event(GtkWidget *widget, GdkEventButton *event,
                                         FmFilePropertiesDEntryData *data)
{
    /* g_debug("icon click received (button=%d)", event->button); */
    return FALSE;
}

static gboolean _dentry_icon_press_event(GtkWidget *widget, GdkEventKey *event,
                                         FmFilePropertiesDEntryData *data)
{
    /* g_debug("icon key received (key=%u)", event->keyval); */
    return FALSE;
}

static void _dentry_browse_exec_event(GtkButton *button, FmFilePropertiesDEntryData *data)
{
    /* g_debug("browse button pressed"); */
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

static void _dentry_terminal_toggled(GtkToggleButton *togglebutton,
                                     FmFilePropertiesDEntryData *data)
{
    /* g_debug("run in terminal toggled"); */
    g_key_file_set_boolean(data->kf, GRP_NAME, "Terminal",
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
    /* replace row 5 with "Comment" entry */
    HIDE_WIDGET("open_with");
    widget = gtk_builder_get_object(ui, "open_with_label");
    label = GTK_LABEL(widget);
    gtk_label_set_markup_with_mnemonic(label, _("<b>_Tooltip:</b>"));
    new_widget = gtk_entry_new();
    data->comment = GTK_ENTRY(new_widget);
    txt = g_key_file_get_locale_string(data->kf, GRP_NAME, "Comment", NULL, NULL);
    if (txt)
        gtk_entry_set_text(data->comment, txt);
    gtk_widget_set_tooltip_text(GTK_WIDGET(new_widget),
                                _("Tooltip to show on application"));
    g_signal_connect(new_widget, "changed", G_CALLBACK(_dentry_tooltip_changed), data);
    gtk_table_attach_defaults(table, new_widget, 2, 3, 5, 6);
    gtk_label_set_mnemonic_widget(label, new_widget);
    gtk_widget_show(new_widget);
    /* replace row 6 with "Exec" GtkHBox: GtkEntry+GtkButton */
    HIDE_WIDGET("total_size");
    widget = gtk_builder_get_object(ui, "total_size_label");
    label = GTK_LABEL(widget);
    gtk_label_set_markup_with_mnemonic(label, _("<b>Co_mmand:</b>"));
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
    gtk_widget_set_tooltip_text(GTK_WIDGET(new_widget),
                                _("Command to execute when the application icon is activated"));
    gtk_box_pack_start(GTK_BOX(widget), new_widget, TRUE, TRUE, 0);
    g_signal_connect(new_widget, "changed", G_CALLBACK(_dentry_exec_changed), data);
    gtk_table_attach_defaults(table, GTK_WIDGET(widget), 2, 3, 6, 7);
    gtk_label_set_mnemonic_widget(label, new_widget);
    gtk_widget_show_all(GTK_WIDGET(widget));
    /* replace row 7 with "Terminal" GtkCheckButton */
    HIDE_WIDGET("size_on_disk_label");
    HIDE_WIDGET("size_on_disk");
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
    gtk_table_attach_defaults(table, new_widget, 2, 3, 7, 8);
    gtk_widget_show(new_widget);
    /* Remove mtime and atime lines */
    /* TODO: add checkbox for 'Leave terminal after exit' */
    HIDE_WIDGET("mtime_label");
    HIDE_WIDGET("mtime");
    HIDE_WIDGET("atime_label");
    HIDE_WIDGET("atime");
#undef HIDE_WIDGET
    //.......
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
    if (data->changed)
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
