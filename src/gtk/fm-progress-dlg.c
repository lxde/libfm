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

enum
{
    RESPONSE_OVERWRITE = 1,
    RESPONSE_RENAME,
    RESPONSE_SKIP
};

typedef struct _FmProgressDlgData FmProgressData;
struct _FmProgressDlgData
{
    GtkWidget* dlg;
    GtkWidget* act;
    GtkWidget* src;
    GtkWidget* dest;
    GtkWidget* current;
    GtkWidget* progress;
    FmFileOpsJob* job;
};

static void data_free(GtkWidget* dlg, FmProgressData* data)
{
    if(data->job)
    {
        fm_job_cancel(data->job);
        g_object_unref(data->job);
    }
    g_slice_free(FmProgressData, data);
}

static void on_percent(FmFileOpsJob* job, guint percent, FmProgressData* data)
{
    char percent_text[64];
    g_snprintf(percent_text, 64, "%d %%", percent);
    gtk_progress_bar_set_fraction(data->progress, (gdouble)percent/100);
    gtk_progress_bar_set_text(data->progress, percent_text);
}

static void on_cur_file(FmFileOpsJob* job, const char* cur_file, FmProgressData* data)
{
    /* FIXME: Displaying currently processed file will slow down the 
     * operation and waste CPU source due to showing the text with pango.
     * Consider showing current file every 0.5 second. */
    /*
    gtk_label_set_text(data->current, cur_file);
    */
}

static gboolean on_error(FmFileOpsJob* job, const char* msg, gboolean recoverable, FmProgressData* data)
{
    fm_show_error(NULL, msg);
    return FALSE;
}

static gint on_ask(FmFileOpsJob* job, const char* question, const char** options, FmProgressData* data)
{
    return fm_askv(NULL, question, options);
}

static void on_filename_changed(GtkEditable* entry, GtkWidget* rename)
{
    const char* old_name = g_object_get_data(entry, "old_name");
    const char* new_name = gtk_entry_get_text(entry);
    gtk_widget_set_sensitive(rename, new_name && *new_name && g_strcmp0(old_name, new_name));
}

static gint on_ask_rename(FmFileOpsJob* job, FmFileInfo* src, FmFileInfo* dest, char** new_name, FmProgressData* data)
{
    int res;
    GtkBuilder* builder = gtk_builder_new();
    GtkWidget *dlg, *src_icon, *dest_icon, *src_fi, *dest_fi, *filename, *apply_all;
    char* tmp;
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

static void on_finished(FmFileOpsJob* job, FmProgressData* data)
{
    g_object_unref(data->job);
    data->job = NULL;
    gtk_widget_destroy(data->dlg);
    g_debug("finished!");
}

static void on_response(GtkDialog* dlg, gint id, FmProgressData* data)
{
    /* cancel the job */
    if(id == GTK_RESPONSE_CANCEL || id == GTK_RESPONSE_DELETE_EVENT)
    {
        if(data->job)
            fm_job_cancel(data->job);
        gtk_widget_destroy(dlg);
    }
}

GtkWidget* fm_progress_dlg_new(FmFileOpsJob* job)
{
    FmProgressData* data = g_slice_new(FmProgressData);
    GtkBuilder* builder = gtk_builder_new();

    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/progress.ui", NULL);

    data->dlg = (GtkWidget*)gtk_builder_get_object(builder, "dlg");

    data->job = (FmFileOpsJob*)g_object_ref(job);
    g_signal_connect(data->dlg, "response", on_response, data);
    g_signal_connect(data->dlg, "destroy", data_free, data);

    data->act = (GtkWidget*)gtk_builder_get_object(builder, "action");
    data->src = (GtkWidget*)gtk_builder_get_object(builder, "src");
    data->dest = (GtkWidget*)gtk_builder_get_object(builder, "dest");
    data->current = (GtkWidget*)gtk_builder_get_object(builder, "current");
    data->progress = (GtkWidget*)gtk_builder_get_object(builder, "progress");

    g_object_unref(builder);

    g_signal_connect(job, "ask", G_CALLBACK(on_ask), data);
    g_signal_connect(job, "ask-rename", G_CALLBACK(on_ask_rename), data);
    g_signal_connect(job, "error", G_CALLBACK(on_error), data);
    g_signal_connect(job, "cur-file", G_CALLBACK(on_cur_file), data);
    g_signal_connect(job, "percent", G_CALLBACK(on_percent), data);
    g_signal_connect(job, "finished", G_CALLBACK(on_finished), data);

    return data->dlg;
}

