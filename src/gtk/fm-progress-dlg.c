/*
 *      fm-progress-dlg.c
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

#include <config.h>
#include "fm-progress-dlg.h"
#include "fm-gtk-utils.h"
#include <glib/gi18n.h>

#define SHOW_DLG_DELAY  1000

enum
{
    RESPONSE_OVERWRITE = 1,
    RESPONSE_RENAME,
    RESPONSE_SKIP
};

struct _FmProgressDisplay
{
    GtkWidget* dlg;
    FmFileOpsJob* job;

    /* private */
    GtkWidget* act;
    GtkWidget* src;
    GtkWidget* dest;
    GtkWidget* current;
    GtkWidget* progress;

    char* cur_file;
    const char* old_cur_file;

    guint delay_timeout;
    guint update_timeout;
};

static void ensure_dlg(FmProgressDisplay* data);
static void fm_progress_display_destroy(FmProgressDisplay* data);

static void on_percent(FmFileOpsJob* job, guint percent, FmProgressDisplay* data)
{
    if(data->dlg)
    {
        char percent_text[64];
        g_snprintf(percent_text, 64, "%d %%", percent);
        gtk_progress_bar_set_fraction(data->progress, (gdouble)percent/100);
        gtk_progress_bar_set_text(data->progress, percent_text);
    }
}

static void on_cur_file(FmFileOpsJob* job, const char* cur_file, FmProgressDisplay* data)
{
    /* FIXME: Displaying currently processed file will slow down the 
     * operation and waste CPU source due to showing the text with pango.
     * Consider showing current file every 0.5 second. */
    g_free(data->cur_file);
    data->cur_file = g_strdup(cur_file);
}

static gboolean on_error(FmFileOpsJob* job, GError* err, gboolean recoverable, FmProgressDisplay* data)
{
    ensure_dlg(data);
    fm_show_error(data->dlg, err->message);
    return FALSE;
}

static gint on_ask(FmFileOpsJob* job, const char* question, const char** options, FmProgressDisplay* data)
{
    ensure_dlg(data);
    return fm_askv(data->dlg, question, options);
}

static void on_filename_changed(GtkEditable* entry, GtkWidget* rename)
{
    const char* old_name = g_object_get_data(entry, "old_name");
    const char* new_name = gtk_entry_get_text(entry);
    gtk_widget_set_sensitive(rename, new_name && *new_name && g_strcmp0(old_name, new_name));
}

static gint on_ask_rename(FmFileOpsJob* job, FmFileInfo* src, FmFileInfo* dest, char** new_name, FmProgressDisplay* data)
{
    int res;
    GtkBuilder* builder = gtk_builder_new();
    GtkWidget *dlg, *src_icon, *dest_icon, *src_fi, *dest_fi, *filename, *apply_all;
    char* tmp;
    ensure_dlg(data);

    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/ask-rename.ui", NULL);
    dlg = (GtkWidget*)gtk_builder_get_object(builder, "dlg");
    src_icon = (GtkWidget*)gtk_builder_get_object(builder, "src_icon");
    src_fi = (GtkWidget*)gtk_builder_get_object(builder, "src_fi");
    dest_icon = (GtkWidget*)gtk_builder_get_object(builder, "dest_icon");
    dest_fi = (GtkWidget*)gtk_builder_get_object(builder, "dest_fi");
    filename = (GtkWidget*)gtk_builder_get_object(builder, "filename");
    apply_all = (GtkWidget*)gtk_builder_get_object(builder, "apply_all");
    gtk_window_set_transient_for(dlg, data->dlg);

    gtk_image_set_from_gicon(src_icon, src->icon->gicon, GTK_ICON_SIZE_DIALOG);
    tmp = g_strdup_printf("Type: %s\nSize: %s\nModified: %s",
                          fm_file_info_get_desc(src),
                          fm_file_info_get_disp_size(src),
                          fm_file_info_get_disp_mtime(src));
    gtk_label_set_text(src_fi, tmp);
    g_free(tmp);

    gtk_image_set_from_gicon(dest_icon, src->icon->gicon, GTK_ICON_SIZE_DIALOG);
    tmp = g_strdup_printf("Type: %s\nSize: %s\nModified: %s",
                          fm_file_info_get_desc(dest),
                          fm_file_info_get_disp_size(dest),
                          fm_file_info_get_disp_mtime(dest));
    gtk_label_set_text(dest_fi, tmp);
    g_free(tmp);

    gtk_entry_set_text(filename, dest->disp_name);
    g_object_set_data(filename, "old_name", dest->disp_name);
    g_signal_connect(filename, "changed", on_filename_changed, gtk_builder_get_object(builder, "rename"));

    g_object_unref(builder);

    res = gtk_dialog_run(GTK_DIALOG(dlg));
    switch(res)
    {
    case RESPONSE_RENAME:
        *new_name = g_strdup(gtk_entry_get_text(filename));
        res = FM_FILE_OP_RENAME;
        break;
    case RESPONSE_OVERWRITE:
        res = FM_FILE_OP_OVERWRITE;
        break;
    case RESPONSE_SKIP:
        res = FM_FILE_OP_SKIP;
        break;
    default:
        res = FM_FILE_OP_CANCEL;
    }

    if(gtk_toggle_button_get_active(apply_all))
    {
        /* FIXME: set default action */
    }

    gtk_widget_destroy(dlg);

    return res;
}

static void on_finished(FmFileOpsJob* job, FmProgressDisplay* data)
{
    fm_progress_display_destroy(data);
    g_debug("file operation is finished!");
}

static void on_response(GtkDialog* dlg, gint id, FmProgressDisplay* data)
{
    /* cancel the job */
    if(id == GTK_RESPONSE_CANCEL || id == GTK_RESPONSE_DELETE_EVENT)
    {
        if(data->job)
            fm_job_cancel(data->job);
        gtk_widget_destroy(dlg);
    }
}

static gboolean on_update_dlg(FmProgressDisplay* data)
{
    if(data->old_cur_file != data->cur_file)
    {
        gtk_label_set_text(data->current, data->cur_file);
        data->old_cur_file = data->cur_file;
    }
    return TRUE;
}

static gboolean on_show_dlg(FmProgressDisplay* data)
{
    GtkBuilder* builder = gtk_builder_new();
    GtkWidget* to, *to_label;
    FmPath* dest;
    const char* title = NULL;
    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/progress.ui", NULL);

    data->dlg = (GtkWidget*)gtk_builder_get_object(builder, "dlg");

    g_signal_connect(data->dlg, "response", on_response, data);

    to_label = (GtkWidget*)gtk_builder_get_object(builder, "to_label");
    to = (GtkWidget*)gtk_builder_get_object(builder, "dest");
    data->act = (GtkWidget*)gtk_builder_get_object(builder, "action");
    data->src = (GtkWidget*)gtk_builder_get_object(builder, "src");
    data->dest = (GtkWidget*)gtk_builder_get_object(builder, "dest");
    data->current = (GtkWidget*)gtk_builder_get_object(builder, "current");
    data->progress = (GtkWidget*)gtk_builder_get_object(builder, "progress");

    g_object_unref(builder);

    /* FIXME: use accessor functions instead */
    switch(data->job->type)
    {
	case FM_FILE_OP_MOVE:
        title = _("Moving files");
        break;
	case FM_FILE_OP_COPY:
        title = _("Copying files");
        break;
	case FM_FILE_OP_TRASH:
        title = _("Trashing files");
        break;
	case FM_FILE_OP_DELETE:
        title = _("Deleting files");
        break;
    case FM_FILE_OP_LINK:
        title = _("Creating symlinks");
        break;
	case FM_FILE_OP_CHMOD:
	case FM_FILE_OP_CHOWN:
        title = _("Changing file properties");
        break;
    }
    if(title)
    {
        gtk_window_set_title(data->dlg, title);
        gtk_label_set_text(data->act, title);
    }

    if(dest = fm_file_ops_job_get_dest(data->job))
    {
        char* dest_str = fm_path_to_str(dest);
        gtk_label_set_text(to, dest_str);
        g_free(dest_str);
    }
    else
    {
        gtk_widget_destroy(dest);
        gtk_widget_destroy(to_label);
    }

    gtk_window_present(data->dlg);
    data->update_timeout = g_timeout_add(500, (GSourceFunc)on_update_dlg, data);

    data->delay_timeout = 0;
    return FALSE;
}

void ensure_dlg(FmProgressDisplay* data)
{
    if(!data->dlg)
        on_show_dlg(data);
    if(data->delay_timeout)
    {
        g_source_remove(data->delay_timeout);
        data->delay_timeout = 0;
    }
}

/* Show progress dialog for file operations */
FmProgressDisplay* fm_display_progress(FmFileOpsJob* job)
{
    FmProgressDisplay* data = g_slice_new0(FmProgressDisplay);
    data->job = (FmFileOpsJob*)g_object_ref(job);
    data->delay_timeout = g_timeout_add(SHOW_DLG_DELAY, (GSourceFunc)on_show_dlg, data);

    g_signal_connect(job, "ask", G_CALLBACK(on_ask), data);
    g_signal_connect(job, "ask-rename", G_CALLBACK(on_ask_rename), data);
    g_signal_connect(job, "error", G_CALLBACK(on_error), data);
    g_signal_connect(job, "cur-file", G_CALLBACK(on_cur_file), data);
    g_signal_connect(job, "percent", G_CALLBACK(on_percent), data);
    g_signal_connect(job, "finished", G_CALLBACK(on_finished), data);

    return data;
}

void fm_progress_display_destroy(FmProgressDisplay* data)
{
    if(data->job)
    {
        fm_job_cancel(data->job);

        g_signal_handlers_disconnect_by_func(data->job, on_ask, data);
        g_signal_handlers_disconnect_by_func(data->job, on_ask_rename, data);
        g_signal_handlers_disconnect_by_func(data->job, on_error, data);
        g_signal_handlers_disconnect_by_func(data->job, on_cur_file, data);
        g_signal_handlers_disconnect_by_func(data->job, on_percent, data);
        g_signal_handlers_disconnect_by_func(data->job, on_finished, data);

        g_object_unref(data->job);
    }

    g_free(data->cur_file);

    if(data->delay_timeout)
        g_source_remove(data->delay_timeout);

    if(data->update_timeout)
        g_source_remove(data->update_timeout);

    if(data->dlg)
        gtk_widget_destroy(data->dlg);

    g_slice_free(FmProgressDisplay, data);
}
