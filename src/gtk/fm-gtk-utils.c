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

#include <glib/gi18n-lib.h>
#include <gio/gdesktopappinfo.h>

#include "fm-gtk-utils.h"
#include "fm-file-ops-job.h"
#include "fm-progress-dlg.h"
#include "fm-path-entry.h"
#include "fm-app-chooser-dlg.h"

#include "fm-config.h"

static GtkDialog* 	_fm_get_user_input_dialog	(GtkWindow* parent, const char* title, const char* msg);
static gchar* 		_fm_user_input_dialog_run	(GtkDialog* dlg, GtkEntry *entry);

void fm_show_error(GtkWindow* parent, const char* msg)
{
    GtkWidget* dlg = gtk_message_dialog_new(parent, 0,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK, msg);
    gtk_window_set_title((GtkWindow*)dlg, _("Error"));
    gtk_dialog_run((GtkDialog*)dlg);
    gtk_widget_destroy(dlg);
}

gboolean fm_yes_no(GtkWindow* parent, const char* question, gboolean default_yes)
{
    int ret;
    GtkWidget* dlg = gtk_message_dialog_new_with_markup(parent, 0,
                                GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, question);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), default_yes ? GTK_RESPONSE_YES : GTK_RESPONSE_NO);
    ret = gtk_dialog_run((GtkDialog*)dlg);
    gtk_widget_destroy(dlg);
    return ret == GTK_RESPONSE_YES;
}

gboolean fm_ok_cancel(GtkWindow* parent, const char* question, gboolean default_ok)
{
    int ret;
    GtkWidget* dlg = gtk_message_dialog_new_with_markup(parent, 0,
                                GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, question);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), default_ok ? GTK_RESPONSE_OK : GTK_RESPONSE_CANCEL);
    ret = gtk_dialog_run((GtkDialog*)dlg);
    gtk_widget_destroy(dlg);
    return ret == GTK_RESPONSE_OK;
}

int fm_ask(GtkWindow* parent, const char* question, ...)
{
    int ret;
    va_list args;
    va_start (args, question);
    ret = fm_ask_valist(parent, question, args);
    va_end (args);
    return ret;
}

int fm_askv(GtkWindow* parent, const char* question, const char** options)
{
    int ret;
    guint id = 1;
    GtkWidget* dlg = gtk_message_dialog_new_with_markup(parent, 0,
                                GTK_MESSAGE_QUESTION, 0, question);
    /* FIXME: need to handle defualt button and alternative button
     * order problems. */
    while(*options)
    {
        /* FIXME: handle button image and stock buttons */
        GtkWidget* btn = gtk_dialog_add_button(GTK_DIALOG( dlg ), *options, id);
        ++options;
        ++id;
    }
    ret = gtk_dialog_run((GtkDialog*)dlg);
    if(ret >= 1)
        ret -= 1;
    else
        ret == -1;
    gtk_widget_destroy(dlg);
    return ret;
}

int fm_ask_valist(GtkWindow* parent, const char* question, va_list options)
{
    GArray* opts = g_array_sized_new(TRUE, TRUE, sizeof(char*), 6);
    gint ret;
    const char* opt = va_arg(options, const char*);
    while(opt)
    {
        g_array_append_val(opts, opt);
        opt = va_arg (options, const char *);
    }
    ret = fm_askv(parent, question, opts->data);
    g_array_free(opts, TRUE);
    return ret;
}



gchar* fm_get_user_input(GtkWindow* parent, const char* title, const char* msg, const char* default_text)
{
    GtkDialog* dlg = _fm_get_user_input_dialog( parent, title, msg);
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

    if(default_text && default_text[0])
        gtk_entry_set_text(GTK_ENTRY( entry ), default_text);

    return _fm_user_input_dialog_run( dlg,  GTK_ENTRY( entry ) );
}

FmPath* fm_get_user_input_path(GtkWindow* parent, const char* title, const char* msg, FmPath* default_path)
{

    GtkDialog* dlg = _fm_get_user_input_dialog( parent, title, msg);
    GtkWidget* entry = gtk_entry_new();
    char *str, *path_str = NULL;
    FmPath* path;

    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

    if(default_path)
    {
        path_str = fm_path_display_name(default_path, FALSE);
        gtk_entry_set_text(GTK_ENTRY( entry ), path_str);
    }

    str = _fm_user_input_dialog_run( dlg,  GTK_ENTRY( entry ) );
    path = fm_path_new(str);

    g_free(path_str);
    g_free(str);
    return path;
}


gchar* fm_get_user_input_rename(GtkWindow* parent, const char* title, const char* msg, const char* default_text)
{
    GtkDialog* dlg = _fm_get_user_input_dialog( parent, title, msg);
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

    if(default_text && default_text[0])
    {
        gtk_entry_set_text(GTK_ENTRY( entry ), default_text);
        /* only select filename part without extension name. */
        if(default_text[1])
        {
            /* FIXME: handle the special case for *.tar.gz or *.tar.bz2 */
            const char* dot = g_utf8_strrchr(default_text, -1, '.');
/*
            const char* dot = default_text;
            while( dot = g_utf8_strchr(dot + 1, -1, '.') )
            {
                gboolean uncertain;
                if(g_content_type_guess(dot, NULL, 0, &uncertain))
                {
                    gtk_editable_select_region(entry, 0, g_utf8_pointer_to_offset(default_text, dot));
                    break;
                }
            }
*/
            if(dot)
                gtk_editable_select_region(GTK_EDITABLE(entry), 0, g_utf8_pointer_to_offset(default_text, dot));
            else
                gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
        }
    }

    return _fm_user_input_dialog_run( dlg,  GTK_ENTRY( entry ) );
}

static GtkDialog* _fm_get_user_input_dialog(GtkWindow* parent, const char* title, const char* msg)
{
    GtkWidget* dlg = gtk_dialog_new_with_buttons(title, parent, GTK_DIALOG_NO_SEPARATOR,
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
    GtkWidget* label = gtk_label_new(msg);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_dialog_set_alternative_button_order(GTK_DIALOG(dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
    gtk_box_set_spacing((GtkBox*)gtk_dialog_get_content_area(GTK_DIALOG(dlg)), 6);
    gtk_box_pack_start((GtkBox*)gtk_dialog_get_content_area(GTK_DIALOG(dlg)), label, FALSE, TRUE, 6);

    gtk_container_set_border_width(GTK_CONTAINER((GtkBox*)gtk_dialog_get_content_area(GTK_DIALOG(dlg))), 12);
    gtk_container_set_border_width(GTK_CONTAINER(dlg), 5);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 480, -1);

    return dlg;
}

static gchar* _fm_user_input_dialog_run( GtkDialog* dlg, GtkEntry *entry)
{
    char* str = NULL;

    gtk_box_pack_start(GTK_BOX( GTK_DIALOG(dlg)->vbox ), GTK_WIDGET( entry ), FALSE, TRUE, 6);
    gtk_widget_show_all(GTK_WIDGET(dlg));
    while(gtk_dialog_run(dlg) == GTK_RESPONSE_OK)
    {
        const char* pstr = gtk_entry_get_text(entry);
        if( pstr && *pstr )
        {
            str = g_strdup(pstr);
            break;
        }
    }
    gtk_widget_destroy(GTK_WIDGET(dlg));
    return str;
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

typedef enum
{
    MOUNT_VOLUME,
    MOUNT_GFILE,
    UMOUNT_MOUNT,
    EJECT_MOUNT,
    EJECT_VOLUME
}MountAction;

struct MountData
{
    GMainLoop *loop;
    MountAction action;
    GError* err;
    gboolean ret;
};

static void on_mount_action_finished(GObject* src, GAsyncResult *res, gpointer user_data)
{
    struct MountData* data = user_data;

    switch(data->action)
    {
    case MOUNT_VOLUME:
        data->ret = g_volume_mount_finish(G_VOLUME(src), res, &data->err);
        break;
    case MOUNT_GFILE:
        data->ret = g_file_mount_enclosing_volume_finish(G_FILE(src), res, &data->err);
        break;
    case UMOUNT_MOUNT:
#if GLIB_CHECK_VERSION(2, 22, 0)
        data->ret = g_mount_unmount_with_operation_finish(G_MOUNT(src), res, &data->err);
#else
        data->ret = g_mount_unmount_finish(G_MOUNT(src), res, &data->err);
#endif
        break;
    case EJECT_MOUNT:
#if GLIB_CHECK_VERSION(2, 22, 0)
        data->ret = g_mount_eject_with_operation_finish(G_MOUNT(src), res, &data->err);
#else
        data->ret = g_mount_eject_finish(G_MOUNT(src), res, &data->err);
#endif
        break;
    case EJECT_VOLUME:
#if GLIB_CHECK_VERSION(2, 22, 0)
        data->ret = g_volume_eject_with_operation_finish(G_VOLUME(src), res, &data->err);
#else
        data->ret = g_volume_eject_finish(G_VOLUME(src), res, &data->err);
#endif
        break;
    }
    g_main_loop_quit(data->loop);
}

gboolean fm_do_mount(GtkWindow* parent, GObject* obj, MountAction action, gboolean interactive)
{
    gboolean ret;
    struct MountData* data = g_new0(struct MountData, 1);
    GMountOperation* op = interactive ? gtk_mount_operation_new(parent) : NULL;
    GCancellable* cancellable = g_cancellable_new();

    data->loop = g_main_loop_new (NULL, TRUE);
    data->action = action;

    switch(data->action)
    {
    case MOUNT_VOLUME:
        g_volume_mount(G_VOLUME(obj), 0, op, cancellable, on_mount_action_finished, data);
        break;
    case MOUNT_GFILE:
        g_file_mount_enclosing_volume(G_FILE(obj), 0, op, cancellable, on_mount_action_finished, data);
        break;
    case UMOUNT_MOUNT:
#if GLIB_CHECK_VERSION(2, 22, 0)
        g_mount_unmount_with_operation(G_MOUNT(obj), G_MOUNT_UNMOUNT_NONE, op, cancellable, on_mount_action_finished, data);
#else
        g_mount_unmount(G_MOUNT(obj), G_MOUNT_UNMOUNT_NONE, cancellable, on_mount_action_finished, data);
#endif
        break;
    case EJECT_MOUNT:
#if GLIB_CHECK_VERSION(2, 22, 0)
        g_mount_eject_with_operation(G_MOUNT(obj), G_MOUNT_UNMOUNT_NONE, op, cancellable, on_mount_action_finished, data);
#else
        g_mount_eject(G_MOUNT(obj), G_MOUNT_UNMOUNT_NONE, cancellable, on_mount_action_finished, data);
#endif
        break;
    case EJECT_VOLUME:
#if GLIB_CHECK_VERSION(2, 22, 0)
        g_volume_eject_with_operation(G_VOLUME(obj), G_MOUNT_UNMOUNT_NONE, op, cancellable, on_mount_action_finished, data);
#else
        g_volume_eject(G_VOLUME(obj), G_MOUNT_UNMOUNT_NONE, cancellable, on_mount_action_finished, data);
#endif
        break;
    }

    if (g_main_loop_is_running(data->loop))
    {
        GDK_THREADS_LEAVE();
        g_main_loop_run(data->loop);
        GDK_THREADS_ENTER();
    }
    g_main_loop_unref(data->loop);

    ret = data->ret;
    if(data->err)
    {
        if(interactive)
        {
            if(data->err->domain == G_IO_ERROR && data->err->code == G_IO_ERROR_FAILED)
            {
                /* Generate a more human-readable error message instead of using a gvfs one. */

                /* The original error message is something like:
                 * Error unmounting: umount exited with exit code 1:
                 * helper failed with: umount: only root can unmount
                 * UUID=18cbf00c-e65f-445a-bccc-11964bdea05d from /media/sda4 */

                /* Why they pass this back to us?
                 * This is not human-readable for the users at all. */

                if(strstr(data->err->message, "only root can "))
                {
                    g_debug("%s", data->err->message);
                    g_free(data->err->message);
                    data->err->message = g_strdup(_("Only system administrators have the permission to do this."));
                }
            }
            fm_show_error(parent, data->err->message);
        }
        g_error_free(data->err);
    }

    g_free(data);
    g_object_unref(cancellable);
    g_object_unref(op);
    return ret;
}

gboolean fm_mount_path(GtkWindow* parent, FmPath* path, gboolean interactive)
{
    GFile* gf = fm_path_to_gfile(path);
    gboolean ret = fm_do_mount(parent, G_OBJECT(gf), MOUNT_GFILE, interactive);
    g_object_unref(gf);
    return ret;
}

gboolean fm_mount_volume(GtkWindow* parent, GVolume* vol, gboolean interactive)
{
    return fm_do_mount(parent, G_OBJECT(vol), MOUNT_VOLUME, interactive);
}

gboolean fm_unmount_mount(GtkWindow* parent, GMount* mount, gboolean interactive)
{
    return fm_do_mount(parent, G_OBJECT(mount), UMOUNT_MOUNT, interactive);
}

gboolean fm_unmount_volume(GtkWindow* parent, GVolume* vol, gboolean interactive)
{
    GMount* mount = g_volume_get_mount(vol);
    gboolean ret;
    if(!mount)
        return FALSE;
    ret = fm_do_mount(parent, G_OBJECT(vol), UMOUNT_MOUNT, interactive);
    g_object_unref(mount);
    return ret;
}

gboolean fm_eject_mount(GtkWindow* parent, GMount* mount, gboolean interactive)
{
    return fm_do_mount(parent, G_OBJECT(mount), EJECT_MOUNT, interactive);
}

gboolean fm_eject_volume(GtkWindow* parent, GVolume* vol, gboolean interactive)
{
    return fm_do_mount(parent, G_OBJECT(vol), EJECT_VOLUME, interactive);
}


/* File operations */
/* FIXME: only show the progress dialog if the job isn't finished
 * in 1 sec. */

void fm_copy_files(FmPathList* files, FmPath* dest_dir)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_COPY, files);
	fm_file_ops_job_set_dest(FM_FILE_OPS_JOB(job), dest_dir);
    fm_file_ops_job_run_with_progress(FM_FILE_OPS_JOB(job));
}

void fm_move_files(FmPathList* files, FmPath* dest_dir)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_MOVE, files);
	fm_file_ops_job_set_dest(FM_FILE_OPS_JOB(job), dest_dir);
    fm_file_ops_job_run_with_progress(FM_FILE_OPS_JOB(job));
}

void fm_trash_files(FmPathList* files)
{
    if(!fm_config->confirm_del || fm_yes_no(NULL, _("Do you want to move the selected files to trash bin?"), TRUE))
    {
    	GtkWidget* dlg;
        FmJob* job = fm_file_ops_job_new(FM_FILE_OP_TRASH, files);
        fm_file_ops_job_run_with_progress(FM_FILE_OPS_JOB(job));
    }
}

static void fm_delete_files_internal(FmPathList* files)
{
    GtkWidget* dlg;
    FmJob* job = fm_file_ops_job_new(FM_FILE_OP_DELETE, files);
    fm_file_ops_job_run_with_progress(FM_FILE_OPS_JOB(job));
}

void fm_delete_files(FmPathList* files)
{
    if(!fm_config->confirm_del || fm_yes_no(NULL, _("Do you want to delete the selected files?"), TRUE))
        fm_delete_files_internal(files);
}

void fm_trash_or_delete_files(FmPathList* files)
{
    if( !fm_list_is_empty(files) )
    {
        gboolean all_in_trash = TRUE;
        if(fm_config->use_trash)
        {
            GList* l = fm_list_peek_head_link(files);
            for(;l;l=l->next)
            {
                FmPath* path = FM_PATH(l->data);
                if(!fm_path_is_trash(path))
                    all_in_trash = FALSE;
            }
        }

        /* files already in trash:/// should only be deleted and cannot be trashed again. */
        if(fm_config->use_trash && !all_in_trash)
            fm_trash_files(files);
        else
            fm_delete_files(files);
    }
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

/*
void fm_rename_files(FmPathList* files)
{

}
*/

void fm_rename_file(FmPath* file)
{
    GFile* gf = fm_path_to_gfile(file), *parent, *dest;
    GError* err = NULL;
    gchar* new_name = fm_get_user_input_rename( NULL, _("Rename File"), _("Please enter a new name:"), file->name);
    if( !new_name )
        return;
    parent = g_file_get_parent(gf);
    dest = g_file_get_child(parent, new_name);
    g_object_unref(parent);
    if(!g_file_move(gf, dest,
                G_FILE_COPY_ALL_METADATA|
                G_FILE_COPY_NO_FALLBACK_FOR_MOVE|
                G_FILE_COPY_NOFOLLOW_SYMLINKS,
                NULL, /* make this cancellable later. */
                NULL, NULL, &err))
    {
        fm_show_error(NULL, err->message);
        g_error_free(err);
    }
    g_object_unref(dest);
    g_object_unref(gf);
}

void fm_empty_trash()
{
    if(fm_yes_no(NULL, _("Are you sure you want to empty the trash bin?"), TRUE))
    {
        FmPathList* paths = fm_path_list_new();
        fm_list_push_tail(paths, fm_path_get_trash());
        fm_delete_files_internal(paths);
        fm_list_unref(paths);
    }
}

static GAppInfo* choose_app(GList* file_infos, FmMimeType* mime_type, gpointer user_data, GError** err)
{
    gpointer* data = (gpointer*)user_data;
    GtkWindow* parent = (GtkWindow*)data[0];
    return fm_choose_app_for_mime_type(parent, mime_type, mime_type != NULL);
}

static gboolean on_launch_error(GAppLaunchContext* ctx, GError* err, gpointer user_data)
{
    gpointer* data = (gpointer*)user_data;
    GtkWindow* parent = (GtkWindow*)data[0];
    fm_show_error(parent, err->message);
    return TRUE;
}

static gboolean on_open_folder(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    gpointer* data = (gpointer*)user_data;
    FmLaunchFolderFunc func = (FmLaunchFolderFunc)data[1];
    return func(ctx, folder_infos, data[2], err);
}

gboolean fm_launch_files_simple(GtkWindow* parent, GAppLaunchContext* ctx, GList* file_infos, FmLaunchFolderFunc func, gpointer user_data)
{
    FmFileLauncher launcher = {
        choose_app,
        on_open_folder,
        on_launch_error
    };
    gpointer data[] = {parent, func, user_data};
    GAppLaunchContext* _ctx = NULL;
    gboolean ret;
    if(ctx == NULL)
    {
        _ctx = ctx = gdk_app_launch_context_new();
        gdk_app_launch_context_set_screen(GDK_APP_LAUNCH_CONTEXT(ctx), parent ? gtk_widget_get_screen(GTK_WIDGET(parent)) : gdk_screen_get_default());
        gdk_app_launch_context_set_timestamp(GDK_APP_LAUNCH_CONTEXT(ctx), gtk_get_current_event_time());
        /* FIXME: how to handle gdk_app_launch_context_set_icon? */
    }
    ret = fm_launch_files(ctx, file_infos, &launcher, data);
    if(_ctx)
        g_object_unref(_ctx);
    return ret;
}

gboolean fm_launch_paths_simple(GtkWindow* parent, GAppLaunchContext* ctx, GList* paths, FmLaunchFolderFunc func, gpointer user_data)
{
    FmFileLauncher launcher = {
        choose_app,
        on_open_folder,
        on_launch_error
    };
    gpointer data[] = {parent, func, user_data};
    GAppLaunchContext* _ctx = NULL;
    gboolean ret;
    if(ctx == NULL)
    {
        _ctx = ctx = gdk_app_launch_context_new();
        gdk_app_launch_context_set_screen(GDK_APP_LAUNCH_CONTEXT(ctx), parent ? gtk_widget_get_screen(GTK_WIDGET(parent)) : gdk_screen_get_default());
        gdk_app_launch_context_set_timestamp(GDK_APP_LAUNCH_CONTEXT(ctx), gtk_get_current_event_time());
        /* FIXME: how to handle gdk_app_launch_context_set_icon? */
    }
    ret = fm_launch_paths(ctx, paths, &launcher, data);
    if(_ctx)
        g_object_unref(_ctx);
    return ret;
}

gboolean fm_launch_file_simple(GtkWindow* parent, GAppLaunchContext* ctx, FmFileInfo* file_info, FmLaunchFolderFunc func, gpointer user_data)
{
    gboolean ret;
    GList* files = g_list_prepend(NULL, file_info);
    ret = fm_launch_files_simple(parent, ctx, files, func, user_data);
    g_list_free(files);
    return ret;
}

gboolean fm_launch_path_simple(GtkWindow* parent, GAppLaunchContext* ctx, FmPath* path, FmLaunchFolderFunc func, gpointer user_data)
{
    gboolean ret;
    GList* files = g_list_prepend(NULL, path);
    ret = fm_launch_paths_simple(parent, ctx, files, func, user_data);
    g_list_free(files);
    return ret;
}
