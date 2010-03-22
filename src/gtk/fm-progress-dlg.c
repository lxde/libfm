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
#include <glib/gi18n-lib.h>

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
    GtkWidget* icon;
    GtkWidget* msg;
    GtkWidget* act;
    GtkWidget* src;
    GtkWidget* dest;
    GtkWidget* current;
    GtkWidget* progress;
    GtkWidget* remaining_time;
    GtkWidget* error_pane;
    GtkWidget* error_msg;
    GtkTextBuffer* error_buf;
    GtkTextTag* bold_tag;

    FmFileOpOption default_opt;

    char* cur_file;
    const char* old_cur_file;

    guint delay_timeout;
    guint update_timeout;

    GTimer* timer;

    gboolean has_error : 1;
};

static void ensure_dlg(FmProgressDisplay* data);
static void fm_progress_display_destroy(FmProgressDisplay* data);

static void on_percent(FmFileOpsJob* job, guint percent, FmProgressDisplay* data)
{
    if(data->dlg)
    {
        GTimeVal cur_time;
        char percent_text[64];
        g_snprintf(percent_text, 64, "%d %%", percent);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->progress), (gdouble)percent/100);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(data->progress), percent_text);

        gdouble elapsed = g_timer_elapsed(data->timer, NULL);
        if(elapsed >= 0.5)
        {
            gdouble remaining = elapsed * (100 - percent) / percent;
            if(data->remaining_time)
            {
                char time_str[32];
                guint secs = (guint)remaining;
                guint mins = 0;
                guint hrs = 0;
                if(secs > 60)
                {
                    mins = secs / 60;
                    secs %= 60;
                    if(mins > 60)
                    {
                        hrs = mins / 60;
                        mins %= 60;
                    }
                }
                g_snprintf(time_str, 32, "%02d:%02d:%02d", hrs, mins, secs);
                gtk_label_set_text(data->remaining_time, time_str);
            }
        }
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

static FmJobErrorAction on_error(FmFileOpsJob* job, GError* err, FmJobErrorSeverity severity, FmProgressDisplay* data)
{
    GtkTextIter it;
    if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_CANCELLED)
        return;

    if(data->timer)
        g_timer_stop(data->timer);

    data->has_error = TRUE;

    ensure_dlg(data);

/*
    FIXME: Need to mount volumes on demand here, too.
    if( err->domain == G_IO_ERROR )
    {
        if( err->code == G_IO_ERROR_NOT_MOUNTED && severity < FM_JOB_ERROR_CRITICAL )
            if(fm_mount_path(parent, dest_path))
                return FM_JOB_RETRY;
    }
*/

    gtk_text_buffer_get_end_iter(data->error_buf, &it);
    gtk_text_buffer_insert_with_tags(data->error_buf, &it, data->cur_file, -1, data->bold_tag, NULL);
    gtk_text_buffer_insert(data->error_buf, &it, _(": "), -1);
    gtk_text_buffer_insert(data->error_buf, &it, err->message, -1);
    gtk_text_buffer_insert(data->error_buf, &it, "\n", 1);

    if(!GTK_WIDGET_VISIBLE(data->error_pane))
        gtk_widget_show(data->error_pane);

    if(data->timer)
        g_timer_continue(data->timer);
    return FM_JOB_CONTINUE;
}

static gint on_ask(FmFileOpsJob* job, const char* question, const char** options, FmProgressDisplay* data)
{
    ensure_dlg(data);
    return fm_askv(GTK_WINDOW(data->dlg), question, options);
}

static void on_filename_changed(GtkEditable* entry, GtkWidget* rename)
{
    const char* old_name = g_object_get_data(G_OBJECT(entry), "old_name");
    const char* new_name = gtk_entry_get_text(GTK_ENTRY(entry));
    gboolean can_rename = new_name && *new_name && g_strcmp0(old_name, new_name);
    gtk_widget_set_sensitive(rename, can_rename);
    if(can_rename)
    {
        GtkDialog* dlg = GTK_DIALOG(gtk_widget_get_toplevel(entry));
        gtk_dialog_set_default_response(dlg, gtk_dialog_get_response_for_widget(dlg, rename));
    }
}

static gint on_ask_rename(FmFileOpsJob* job, FmFileInfo* src, FmFileInfo* dest, char** new_name, FmProgressDisplay* data)
{
    int res;
    GtkBuilder* builder = gtk_builder_new();
    GtkWidget *dlg, *src_icon, *dest_icon, *src_fi, *dest_fi, *filename, *apply_all;
    char* tmp;
    const char* disp_size;

    /* return default operation if the user has set it */
    if(data->default_opt)
        return data->default_opt;

    if(data->timer)
        g_timer_stop(data->timer);

    gtk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);
    ensure_dlg(data);

    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/ask-rename.ui", NULL);
    dlg = (GtkWidget*)gtk_builder_get_object(builder, "dlg");
    src_icon = (GtkWidget*)gtk_builder_get_object(builder, "src_icon");
    src_fi = (GtkWidget*)gtk_builder_get_object(builder, "src_fi");
    dest_icon = (GtkWidget*)gtk_builder_get_object(builder, "dest_icon");
    dest_fi = (GtkWidget*)gtk_builder_get_object(builder, "dest_fi");
    filename = (GtkWidget*)gtk_builder_get_object(builder, "filename");
    apply_all = (GtkWidget*)gtk_builder_get_object(builder, "apply_all");
    gtk_window_set_transient_for(GTK_WINDOW(dlg), data->dlg);

    gtk_image_set_from_gicon(GTK_IMAGE(src_icon), src->icon->gicon, GTK_ICON_SIZE_DIALOG);
    disp_size = fm_file_info_get_disp_size(src);
    if(disp_size)
    {
        tmp = g_strdup_printf("Type: %s\nSize: %s\nModified: %s",
                              fm_file_info_get_desc(src),
                              disp_size,
                              fm_file_info_get_disp_mtime(src));
    }
    else
    {
        tmp = g_strdup_printf("Type: %s\nModified: %s",
                              fm_file_info_get_desc(src),
                              fm_file_info_get_disp_mtime(src));
    }

    gtk_label_set_text(GTK_LABEL(src_fi), tmp);
    g_free(tmp);

    gtk_image_set_from_gicon(GTK_IMAGE(dest_icon), src->icon->gicon, GTK_ICON_SIZE_DIALOG);
    disp_size = fm_file_info_get_disp_size(dest);
    if(disp_size)
    {
        tmp = g_strdup_printf("Type: %s\nSize: %s\nModified: %s",
                              fm_file_info_get_desc(dest),
                              fm_file_info_get_disp_size(dest),
                              fm_file_info_get_disp_mtime(dest));
    }
    else
    {
        tmp = g_strdup_printf("Type: %s\nModified: %s",
                              fm_file_info_get_desc(dest),
                              fm_file_info_get_disp_mtime(dest));
    }

    gtk_label_set_text(GTK_LABEL(dest_fi), tmp);
    g_free(tmp);

    gtk_entry_set_text(GTK_ENTRY(filename), dest->disp_name);
    g_object_set_data(G_OBJECT(filename), "old_name", dest->disp_name);
    g_signal_connect(filename, "changed", on_filename_changed, gtk_builder_get_object(builder, "rename"));

    g_object_unref(builder);

    res = gtk_dialog_run(GTK_DIALOG(dlg));
    switch(res)
    {
    case RESPONSE_RENAME:
        *new_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(filename)));
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

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(apply_all)))
    {
        if(res == RESPONSE_OVERWRITE || res == RESPONSE_SKIP)
            data->default_opt = res;
    }

    gtk_widget_destroy(dlg);

    if(data->timer)
        g_timer_continue(data->timer);

    return res;
}

static void on_finished(FmFileOpsJob* job, FmProgressDisplay* data)
{
    if(data->update_timeout)
    {
        g_source_remove(data->update_timeout);
        data->update_timeout = 0;
    }

    if(data->dlg)
    {
        /* errors happened */
        if(data->has_error && data->dlg)
        {
            gtk_label_set_text(GTK_LABEL(data->current), "");
            gtk_label_set_text(GTK_LABEL(data->remaining_time), "00:00:00");
            gtk_dialog_set_response_sensitive(GTK_DIALOG(data->dlg), GTK_RESPONSE_CANCEL, FALSE);
            gtk_dialog_add_button(GTK_DIALOG(data->dlg), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

            gtk_image_set_from_stock(data->icon, GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);

            gtk_widget_show(data->msg);
            if(fm_job_is_cancelled(data->job))
            {
                gtk_label_set_text(GTK_LABEL(data->msg), _("The file operation is cancelled and there are some errors."));
                gtk_window_set_title(GTK_WINDOW(data->dlg),
                                     _("Cancelled"));
            }
            else
            {
                gtk_label_set_text(GTK_LABEL(data->msg), _("The file operation is finished, but there are some errors."));
                gtk_window_set_title(GTK_WINDOW(data->dlg),
                                     _("Finished"));
            }
        }
        else
            fm_progress_display_destroy(data);
        g_debug("file operation is finished!");
    }
    else
        fm_progress_display_destroy(data);
}

static void on_cancelled(FmFileOpsJob* job, FmProgressDisplay* data)
{
    g_debug("file operation is cancelled!");
}

static void on_response(GtkDialog* dlg, gint id, FmProgressDisplay* data)
{
    /* cancel the job */
    if(id == GTK_RESPONSE_CANCEL || id == GTK_RESPONSE_DELETE_EVENT)
    {
        if(data->job)
        {
            fm_job_cancel(FM_JOB(data->job));
            if(id != GTK_RESPONSE_CANCEL)
                fm_progress_display_destroy(data);
        }
    }
    else if(id == GTK_RESPONSE_CLOSE)
        fm_progress_display_destroy(data);
}

static gboolean on_update_dlg(FmProgressDisplay* data)
{
    if(data->old_cur_file != data->cur_file)
    {
        gtk_label_set_text(GTK_LABEL(data->current), data->cur_file);
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
    GtkTextTagTable* tag_table = gtk_text_tag_table_new();

    gtk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);
    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/progress.ui", NULL);

    data->dlg = (GtkWidget*)gtk_builder_get_object(builder, "dlg");

    g_signal_connect(data->dlg, "response", on_response, data);

    to_label = (GtkWidget*)gtk_builder_get_object(builder, "to_label");
    to = (GtkWidget*)gtk_builder_get_object(builder, "dest");
    data->icon = (GtkWidget*)gtk_builder_get_object(builder, "icon");
    data->msg = (GtkWidget*)gtk_builder_get_object(builder, "msg");
    data->act = (GtkWidget*)gtk_builder_get_object(builder, "action");
    data->src = (GtkWidget*)gtk_builder_get_object(builder, "src");
    data->dest = (GtkWidget*)gtk_builder_get_object(builder, "dest");
    data->current = (GtkWidget*)gtk_builder_get_object(builder, "current");
    data->progress = (GtkWidget*)gtk_builder_get_object(builder, "progress");
    data->error_pane = (GtkWidget*)gtk_builder_get_object(builder, "error_pane");
    data->error_msg = (GtkWidget*)gtk_builder_get_object(builder, "error_msg");
    data->remaining_time = (GtkWidget*)gtk_builder_get_object(builder, "remaining_time");

    data->bold_tag = gtk_text_tag_new("bold");
    g_object_set(data->bold_tag, "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_tag_table_add(tag_table, data->bold_tag);
    data->error_buf = gtk_text_buffer_new(tag_table);
    g_object_unref(tag_table);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(data->error_msg), data->error_buf);

    g_object_unref(builder);

    /* set the src label */
    if(data->job->srcs)
    {
        GList* l = fm_list_peek_head_link(data->job->srcs);
        int i;
        char* disp;
        FmPath* path;
        GString* str = g_string_sized_new(512);
        path = FM_PATH(l->data);
        disp = fm_path_display_basename(path);
        g_string_assign(str, disp);
        g_free(disp);
        for( i =1, l=l->next; i < 10 && l; l=l->next, ++i)
        {
            path = FM_PATH(l->data);
            g_string_append(str, _(", "));
            disp = fm_path_display_basename(path);
            g_string_append(str, disp);
            g_free(disp);
        }
        if(l)
            g_string_append(str, "...");
        gtk_label_set_text(data->src, str->str);
        g_string_free(str, TRUE);
    }

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
	case FM_FILE_OP_CHANGE_ATTR:
        title = _("Changing file attributes");
        break;
    }
    if(title)
    {
        gtk_window_set_title(GTK_WINDOW(data->dlg), title);
        gtk_label_set_text(GTK_LABEL(data->act), title);
    }

    if(dest = fm_file_ops_job_get_dest(data->job))
    {
        char* dest_str = fm_path_display_name(dest, TRUE);
        gtk_label_set_text(GTK_LABEL(to), dest_str);
        g_free(dest_str);
    }
    else
    {
        gtk_widget_destroy(data->dest);
        gtk_widget_destroy(to_label);
    }

    gtk_window_present(GTK_WINDOW(data->dlg));
    data->update_timeout = g_timeout_add(500, (GSourceFunc)on_update_dlg, data);

    data->delay_timeout = 0;
    return FALSE;
}

void ensure_dlg(FmProgressDisplay* data)
{
    if(data->delay_timeout)
    {
        g_source_remove(data->delay_timeout);
        data->delay_timeout = 0;
    }
    if(!data->dlg)
        on_show_dlg(data);
}

static void on_prepared(FmFileOpsJob* job, FmProgressDisplay* data)
{
    data->timer = g_timer_new();
}

/* Run the file operation job with a progress dialog.
 * The returned data structure will be freed in idle handler automatically
 * when it's not needed anymore.
 */
FmProgressDisplay* fm_file_ops_job_run_with_progress(FmFileOpsJob* job)
{
    FmProgressDisplay* data = g_slice_new0(FmProgressDisplay);
    data->job = (FmFileOpsJob*)g_object_ref(job);
    data->delay_timeout = g_timeout_add(SHOW_DLG_DELAY, (GSourceFunc)on_show_dlg, data);

    g_signal_connect(job, "ask", G_CALLBACK(on_ask), data);
    g_signal_connect(job, "ask-rename", G_CALLBACK(on_ask_rename), data);
    g_signal_connect(job, "error", G_CALLBACK(on_error), data);
    g_signal_connect(job, "prepared", G_CALLBACK(on_prepared), data);
    g_signal_connect(job, "cur-file", G_CALLBACK(on_cur_file), data);
    g_signal_connect(job, "percent", G_CALLBACK(on_percent), data);
    g_signal_connect(job, "finished", G_CALLBACK(on_finished), data);
    g_signal_connect(job, "cancelled", G_CALLBACK(on_cancelled), data);

	fm_job_run_async(job);

    return data;
}

void fm_progress_display_destroy(FmProgressDisplay* data)
{
    if(data->job)
    {
        fm_job_cancel(FM_JOB(data->job));

        g_signal_handlers_disconnect_by_func(data->job, on_ask, data);
        g_signal_handlers_disconnect_by_func(data->job, on_ask_rename, data);
        g_signal_handlers_disconnect_by_func(data->job, on_error, data);
        g_signal_handlers_disconnect_by_func(data->job, on_cur_file, data);
        g_signal_handlers_disconnect_by_func(data->job, on_percent, data);
        g_signal_handlers_disconnect_by_func(data->job, on_finished, data);

        g_object_unref(data->job);

        if(data->timer)
            g_timer_destroy(data->timer);
    }

    g_free(data->cur_file);

    if(data->delay_timeout)
        g_source_remove(data->delay_timeout);

    if(data->update_timeout)
        g_source_remove(data->update_timeout);

    if(data->dlg)
    {
        g_object_unref(data->error_buf);
        g_object_unref(data->bold_tag);
        gtk_widget_destroy(data->dlg);
    }

    g_slice_free(FmProgressDisplay, data);
}
