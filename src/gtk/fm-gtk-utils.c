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

static GtkDialog*   _fm_get_user_input_dialog   (GtkWindow* parent, const char* title, const char* msg);
static gchar*       _fm_user_input_dialog_run   (GtkDialog* dlg, GtkEntry *entry);

void fm_show_error(GtkWindow* parent, const char* title, const char* msg)
{
    GtkWidget* dlg = gtk_message_dialog_new(parent, 0,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK, msg);
    gtk_window_set_title((GtkWindow*)dlg, title ? title : _("Error"));
    gtk_dialog_run((GtkDialog*)dlg);
    gtk_widget_destroy(dlg);
}

gboolean fm_yes_no(GtkWindow* parent, const char* title, const char* question, gboolean default_yes)
{
    int ret;
    GtkWidget* dlg = gtk_message_dialog_new_with_markup(parent, 0,
                                GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, question);
    gtk_window_set_title(GTK_WINDOW(dlg), title ? title : _("Confirm"));
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), default_yes ? GTK_RESPONSE_YES : GTK_RESPONSE_NO);
    ret = gtk_dialog_run((GtkDialog*)dlg);
    gtk_widget_destroy(dlg);
    return ret == GTK_RESPONSE_YES;
}

gboolean fm_ok_cancel(GtkWindow* parent, const char* title, const char* question, gboolean default_ok)
{
    int ret;
    GtkWidget* dlg = gtk_message_dialog_new_with_markup(parent, 0,
                                GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, question);
    gtk_window_set_title(GTK_WINDOW(dlg), title ? title : _("Confirm"));
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), default_ok ? GTK_RESPONSE_OK : GTK_RESPONSE_CANCEL);
    ret = gtk_dialog_run((GtkDialog*)dlg);
    gtk_widget_destroy(dlg);
    return ret == GTK_RESPONSE_OK;
}

/**
 * fm_ask
 * Ask the user a question with several options provided.
 * @parent: toplevel parent widget
 * @question: the question to show to the user
 * @...: a NULL terminated list of button labels
 * Returns: the index of selected button, or -1 if the dialog is closed.
 */
int fm_ask(GtkWindow* parent, const char* title, const char* question, ...)
{
    int ret;
    va_list args;
    va_start (args, question);
    ret = fm_ask_valist(parent, title, question, args);
    va_end (args);
    return ret;
}

/**
 * fm_askv
 * Ask the user a question with several options provided.
 * @parent: toplevel parent widget
 * @question: the question to show to the user
 * @options: a NULL terminated list of button labels
 * Returns: the index of selected button, or -1 if the dialog is closed.
 */
int fm_askv(GtkWindow* parent, const char* title, const char* question, const char** options)
{
    int ret;
    guint id = 1;
    GtkWidget* dlg = gtk_message_dialog_new_with_markup(parent, 0,
                                GTK_MESSAGE_QUESTION, 0, question);
    gtk_window_set_title(GTK_WINDOW(dlg), title ? title : _("Question"));
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
        ret = -1;
    gtk_widget_destroy(dlg);
    return ret;
}

/**
 * fm_ask_valist
 * Ask the user a question with several options provided.
 * @parent: toplevel parent widget
 * @question: the question to show to the user
 * @options: a NULL terminated list of button labels
 * Returns: the index of selected button, or -1 if the dialog is closed.
 */
int fm_ask_valist(GtkWindow* parent, const char* title, const char* question, va_list options)
{
    GArray* opts = g_array_sized_new(TRUE, TRUE, sizeof(char*), 6);
    gint ret;
    const char* opt = va_arg(options, const char*);
    while(opt)
    {
        g_array_append_val(opts, opt);
        opt = va_arg (options, const char *);
    }
    ret = fm_askv(parent, title, question, opts->data);
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
    path = fm_path_new_for_str(str);

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
            /* FIXME: handle the special case for *.tar.gz or *.tar.bz2
             * We should exam the file extension with g_content_type_guess, and
             * find out a longest valid extension name.
             * For example, the extension name of foo.tar.gz is .tar.gz, not .gz. */
            const char* dot = g_utf8_strrchr(default_text, -1, '.');
            if(dot)
                gtk_editable_select_region(GTK_EDITABLE(entry), 0, g_utf8_pointer_to_offset(default_text, dot));
            else
                gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
            /*
            const char* dot = default_text;
            while( dot = g_utf8_strchr(dot + 1, -1, '.') )
            {
                gboolean uncertain;
                char* type = g_content_type_guess(dot-1, NULL, 0, &uncertain);
                if(!g_content_type_is_unknown(type))
                {
                    g_free(type);
                    gtk_editable_select_region(entry, 0, g_utf8_pointer_to_offset(default_text, dot));
                    break;
                }
                g_free(type);
            }
            */
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
    int sel_start, sel_end;
    gboolean has_sel;

    /* FIXME: this workaround is used to overcome bug of gtk+.
     * gtk+ seems to ignore select region and select all text for entry in dialog. */
    has_sel = gtk_editable_get_selection_bounds(GTK_EDITABLE(entry), &sel_start, &sel_end);
    gtk_box_pack_start(GTK_BOX( GTK_DIALOG(dlg)->vbox ), GTK_WIDGET( entry ), FALSE, TRUE, 6);
    gtk_widget_show_all(GTK_WIDGET(dlg));

    if(has_sel)
        gtk_editable_select_region(GTK_EDITABLE(entry), sel_start, sel_end);

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

FmPath* fm_select_folder(GtkWindow* parent, const char* title)
{
    FmPath* path;
    GtkFileChooser* chooser;
    chooser = (GtkFileChooser*)gtk_file_chooser_dialog_new(
                                        title ? title : _("Please select a folder"),
                                        parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OK, GTK_RESPONSE_OK,
                                        NULL);
    gtk_dialog_set_alternative_button_order((GtkDialog*)chooser,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_RESPONSE_OK, NULL);
    if( gtk_dialog_run((GtkDialog*)chooser) == GTK_RESPONSE_OK )
    {
        GFile* file = gtk_file_chooser_get_file(chooser);
        path = fm_path_new_for_gfile(file);
        g_object_unref(file);
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
g_debug("on_mount_action_finished");
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

static void prepare_unmount(GMount* mount)
{
    /* ensure that CWD is not on the mounted filesystem. */
    char* cwd_str = g_get_current_dir();
    GFile* cwd = g_file_new_for_path(cwd_str);
    GFile* root = g_mount_get_root(mount);
    g_free(cwd_str);
    /* FIXME: This cannot cover 100% cases since symlinks are not checked.
     * There may be other cases that cwd is actually under mount root
     * but checking prefix is not enough. We already did our best, though. */
    if(g_file_has_prefix(cwd, root))
        g_chdir("/");
    g_object_unref(cwd);
    g_object_unref(root);
}

static gboolean fm_do_mount(GtkWindow* parent, GObject* obj, MountAction action, gboolean interactive)
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
        prepare_unmount(G_MOUNT(obj));
#if GLIB_CHECK_VERSION(2, 22, 0)
        g_mount_unmount_with_operation(G_MOUNT(obj), G_MOUNT_UNMOUNT_NONE, op, cancellable, on_mount_action_finished, data);
#else
        g_mount_unmount(G_MOUNT(obj), G_MOUNT_UNMOUNT_NONE, cancellable, on_mount_action_finished, data);
#endif
        break;
    case EJECT_MOUNT:
        prepare_unmount(G_MOUNT(obj));
#if GLIB_CHECK_VERSION(2, 22, 0)
        g_mount_eject_with_operation(G_MOUNT(obj), G_MOUNT_UNMOUNT_NONE, op, cancellable, on_mount_action_finished, data);
#else
        g_mount_eject(G_MOUNT(obj), G_MOUNT_UNMOUNT_NONE, cancellable, on_mount_action_finished, data);
#endif
        break;
    case EJECT_VOLUME:
        {
            GMount* mnt = g_volume_get_mount(G_VOLUME(obj));
            prepare_unmount(mnt);
            g_object_unref(mnt);
#if GLIB_CHECK_VERSION(2, 22, 0)
            g_volume_eject_with_operation(G_VOLUME(obj), G_MOUNT_UNMOUNT_NONE, op, cancellable, on_mount_action_finished, data);
#else
            g_volume_eject(G_VOLUME(obj), G_MOUNT_UNMOUNT_NONE, cancellable, on_mount_action_finished, data);
#endif
        }
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
            if(data->err->domain == G_IO_ERROR)
            {
                if(data->err->code == G_IO_ERROR_FAILED)
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
                else if(data->err->code == G_IO_ERROR_FAILED_HANDLED)
                    interactive = FALSE;
            }
            if(interactive)
                fm_show_error(parent, NULL, data->err->message);
        }
        g_error_free(data->err);
    }

    g_free(data);
    g_object_unref(cancellable);
    if(op)
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

void fm_copy_files(GtkWindow* parent, FmPathList* files, FmPath* dest_dir)
{
    FmJob* job = fm_file_ops_job_new(FM_FILE_OP_COPY, files);
    fm_file_ops_job_set_dest(FM_FILE_OPS_JOB(job), dest_dir);
    fm_file_ops_job_run_with_progress(parent, FM_FILE_OPS_JOB(job));
}

void fm_move_files(GtkWindow* parent, FmPathList* files, FmPath* dest_dir)
{
    FmJob* job = fm_file_ops_job_new(FM_FILE_OP_MOVE, files);
    fm_file_ops_job_set_dest(FM_FILE_OPS_JOB(job), dest_dir);
    fm_file_ops_job_run_with_progress(parent, FM_FILE_OPS_JOB(job));
}

void fm_trash_files(GtkWindow* parent, FmPathList* files)
{
    if(!fm_config->confirm_del || fm_yes_no(parent, NULL, _("Do you want to move the selected files to trash can?"), TRUE))
    {
        FmJob* job = fm_file_ops_job_new(FM_FILE_OP_TRASH, files);
        fm_file_ops_job_run_with_progress(parent, FM_FILE_OPS_JOB(job));
    }
}

void fm_untrash_files(GtkWindow* parent, FmPathList* files)
{
    FmJob* job = fm_file_ops_job_new(FM_FILE_OP_UNTRASH, files);
    fm_file_ops_job_run_with_progress(parent, FM_FILE_OPS_JOB(job));
}

static void fm_delete_files_internal(GtkWindow* parent, FmPathList* files)
{
    FmJob* job = fm_file_ops_job_new(FM_FILE_OP_DELETE, files);
    fm_file_ops_job_run_with_progress(parent, FM_FILE_OPS_JOB(job));
}

void fm_delete_files(GtkWindow* parent, FmPathList* files)
{
    if(!fm_config->confirm_del || fm_yes_no(parent, NULL, _("Do you want to delete the selected files?"), TRUE))
        fm_delete_files_internal(parent, files);
}

void fm_trash_or_delete_files(GtkWindow* parent, FmPathList* files)
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
            fm_trash_files(parent, files);
        else
            fm_delete_files(parent, files);
    }
}

void fm_move_or_copy_files_to(GtkWindow* parent, FmPathList* files, gboolean is_move)
{
    FmPath* dest = fm_select_folder(parent, NULL);
    if(dest)
    {
        if(is_move)
            fm_move_files(parent, files, dest);
        else
            fm_copy_files(parent, files, dest);
        fm_path_unref(dest);
    }
}


void fm_rename_file(GtkWindow* parent, FmPath* file)
{
    GFile* gf = fm_path_to_gfile(file), *parent_gf, *dest;
    GError* err = NULL;
    gchar* new_name = fm_get_user_input_rename( parent, _("Rename File"), _("Please enter a new name:"), file->name);
    if( !new_name )
        return;
    parent_gf = g_file_get_parent(gf);
    dest = g_file_get_child(G_FILE(parent_gf), new_name);
    g_object_unref(parent_gf);
    if(!g_file_move(gf, dest,
                G_FILE_COPY_ALL_METADATA|
                G_FILE_COPY_NO_FALLBACK_FOR_MOVE|
                G_FILE_COPY_NOFOLLOW_SYMLINKS,
                NULL, /* make this cancellable later. */
                NULL, NULL, &err))
    {
        fm_show_error(parent, NULL, err->message);
        g_error_free(err);
    }
    g_object_unref(dest);
    g_object_unref(gf);
}

void fm_empty_trash(GtkWindow* parent)
{
    if(fm_yes_no(parent, NULL, _("Are you sure you want to empty the trash can?"), TRUE))
    {
        FmPathList* paths = fm_path_list_new();
        fm_list_push_tail(paths, fm_path_get_trash());
        fm_delete_files_internal(parent, paths);
        fm_list_unref(paths);
    }
}
