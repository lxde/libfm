/*
 *      fm-gtk-utils.c
 *      
 *      Copyright 2009 PCMan <pcman@debian>
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

#include <glib/gi18n.h>
#include "fm-gtk-utils.h"
#include "fm-file-ops-job.h"
#include "fm-progress-dlg.h"

void fm_show_error(GtkWindow* parent, const char* msg)
{
    GtkWidget* dlg = gtk_message_dialog_new_with_markup(parent, 0, 
                                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, msg);
    gtk_window_set_title((GtkWindow*)dlg, _("Error"));
    gtk_dialog_run((GtkDialog*)dlg);
    gtk_widget_destroy(dlg);
}

FmPath* fm_select_folder(GtkWindow* parent)
{
    FmPath* path;
    GtkFileChooser* chooser;
    chooser = (GtkFileChooser*)gtk_file_chooser_dialog_new(_("Please select a folder"), 
                                        parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OK, GTK_RESPONSE_OK,
                                        NULL);
    gtk_dialog_set_alternative_button_order((GtkDialog*)chooser, 
                                        GTK_RESPONSE_CANCEL,
                                        GTK_RESPONSE_OK, NULL);
    if( gtk_dialog_run((GtkDialog*)chooser) == GTK_RESPONSE_OK )
    {
        char* file = gtk_file_chooser_get_filename(chooser);
        if(!file)
            file = gtk_file_chooser_get_uri(chooser);
        path = fm_path_new(file);
        g_free(file);
    }
    else
        path = NULL;
    gtk_widget_destroy((GtkWidget*)chooser);
    return path;
}


struct MountData
{
    GMainLoop *loop;
    GError* err;
};

void on_mounted(GFile *gf, GAsyncResult *res, struct MountData* data)
{
    GError* err = NULL;
    if( !g_file_mount_enclosing_volume_finish(gf, res, &err) )
        data->err = err;
    else
    {
        if(err)
            g_error_free(err);
    }
    g_main_loop_quit(data->loop);
}

gboolean fm_mount_path(GtkWindow* parent, FmPath* path)
{
    gboolean ret = FALSE;
    struct MountData* data = g_new0(struct MountData, 1);
    GFile* gf = fm_path_to_gfile(path);
    GMountOperation* op = gtk_mount_operation_new(parent);
    GCancellable* cancel = g_cancellable_new();
    data->loop = g_main_loop_new (NULL, TRUE);

    g_file_mount_enclosing_volume(gf, 0, op, cancel, (GAsyncReadyCallback)on_mounted, data);

    if (g_main_loop_is_running(data->loop))
    {
        GDK_THREADS_LEAVE();
        g_main_loop_run(data->loop);
        GDK_THREADS_ENTER();
    }
    g_main_loop_unref(data->loop);

    if(data->err)
    {
        fm_show_error(parent, data->err->message);
        g_error_free(data->err);
    }
    else
        ret = TRUE;

    g_free(data);
    g_object_unref(cancel);
    g_object_unref(op);
    g_object_unref(gf);
    return ret;
}

/* File operations */
/* FIXME: only show the progress dialog if the job isn't finished 
 * in 1 sec. */

void fm_copy_files(FmPathList* files, FmPath* dest_dir)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_COPY, files);
	fm_file_ops_job_set_dest(job, dest_dir);
	dlg = fm_progress_dlg_new(job);
	gtk_window_present(dlg);
	fm_job_run_async(job);
}

void fm_move_files(FmPathList* files, FmPath* dest_dir)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_MOVE, files);
	fm_file_ops_job_set_dest(job, dest_dir);
	dlg = fm_progress_dlg_new(job);
	gtk_window_present(dlg);
	fm_job_run_async(job);
}

void fm_trash_files(FmPathList* files)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_TRASH, files);
	dlg = fm_progress_dlg_new(job);
	gtk_window_present(dlg);
	fm_job_run_async(job);
}

void fm_delete_files(FmPathList* files)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_DELETE, files);
	dlg = fm_progress_dlg_new(job);
	gtk_window_present(dlg);
	fm_job_run_async(job);
}

void fm_move_or_copy_files_to(FmPathList* files, gboolean is_move)
{
    FmPath* dest = fm_select_folder(NULL);
    if(dest)
    {
        if(is_move)
            fm_move_files(files, dest);
        else
            fm_copy_files(files, dest);
        fm_path_unref(dest);
    }
}

void fm_rename_files(FmPathList* files)
{
    
}

void fm_rename_file(FmPath* file)
{
    FmPathList* pl = fm_path_list_new();
    fm_list_push_tail(pl, file);
    fm_rename_files(pl);
    fm_list_unref(pl);
}
