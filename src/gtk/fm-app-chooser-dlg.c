/*
 *      fm-app-chooser-dlg.c
 *
 *      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
#include <string.h>
#include <unistd.h>
#include "fm.h"
#include "fm-app-chooser-dlg.h"
#include "fm-app-menu-view.h"
#include <menu-cache.h>
#include <gio/gdesktopappinfo.h>

typedef struct _AppChooserData AppChooserData;
struct _AppChooserData
{
    GtkWidget* dlg;
    GtkWidget* notebook;
    GtkWidget* apps_view;
    GtkWidget* cmdline;
    GtkWidget* set_default;
    GtkWidget* status;
    GtkWidget* use_terminal;
    FmMimeType* mime_type;
};

GAppInfo* fm_app_info_create_from_commandline(const char *commandline,
                                               const char *application_name,
                                               gboolean terminal)
{
    GAppInfo* app = NULL;
    char* dirname = g_build_filename (g_get_user_data_dir (), "applications", NULL);

    if(g_mkdir_with_parents(dirname, 0700) == 0)
    {
        char* filename = g_strdup_printf ("%s/userapp-%s-XXXXXX.desktop", dirname, application_name);
        int fd = g_mkstemp (filename);
        if(fd != -1)
        {
            GString* content = g_string_sized_new(256);
            g_string_printf(content,
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=%s\n"
                "Exec=%s\n"
                "NoDisplay=true\n",
                application_name,
                commandline
            );
            if(terminal)
                g_string_append_printf(content,
                    "Terminal=%s\n", terminal ? "true" : "false");
            if(g_file_set_contents(filename, content->str, content->len, NULL))
            {
                char* desktop_id = g_path_get_basename(filename);
                app = g_desktop_app_info_new(desktop_id);
                g_free(desktop_id);
            }
            close(fd);
        }
        g_free(filename);
    }
    g_free(dirname);
    return app;
}

static void on_dlg_destroy(AppChooserData* data, GObject* dlg)
{
    g_slice_free(AppChooserData, data);
}

static void on_switch_page(GtkNotebook* nb, GtkWidget* page, gint num, AppChooserData* data)
{
    if(num == 0) /* list of installed apps */
    {
        gtk_label_set_text(GTK_LABEL(data->status), _("Use selected application to open files"));
        gtk_dialog_set_response_sensitive(data->dlg, GTK_RESPONSE_OK,
                        fm_app_menu_view_is_app_selected(GTK_TREE_VIEW(data->apps_view)));
    }
    else /* custom app */
    {
        const char* cmd = gtk_entry_get_text(GTK_ENTRY(data->cmdline));
        gtk_label_set_text(GTK_LABEL(data->status), _("Execute custom command line to open files"));
        gtk_dialog_set_response_sensitive(GTK_DIALOG(data->dlg), GTK_RESPONSE_OK, (cmd && cmd[0]));
    }
}

static void on_apps_view_sel_changed(GtkTreeSelection* tree_sel, AppChooserData* data)
{
    if(gtk_notebook_get_current_page(GTK_NOTEBOOK(data->notebook)) == 0)
    {
        gtk_dialog_set_response_sensitive(data->dlg, GTK_RESPONSE_OK,
                        fm_app_menu_view_is_app_selected(GTK_TREE_VIEW(data->apps_view)));
    }
}

static void on_cmdline_changed(GtkEditable* cmdline, AppChooserData* data)
{
    if(gtk_notebook_get_current_page(GTK_NOTEBOOK(data->notebook)) == 1)
    {
        const char* cmd = gtk_entry_get_text(GTK_ENTRY(data->cmdline));
        gtk_dialog_set_response_sensitive(GTK_DIALOG(data->dlg), GTK_RESPONSE_OK, (cmd && cmd[0]));
    }
}

GtkWidget *fm_app_chooser_dlg_new(FmMimeType* mime_type, gboolean can_set_default)
{
    GtkWidget* scroll;
    GtkWidget* file_type;
    GtkTreeSelection* tree_sel;
    GtkBuilder* builder = gtk_builder_new();
    AppChooserData* data = g_slice_new0(AppChooserData);

    gtk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);
    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/app-chooser.ui", NULL);
    data->dlg = (GtkWidget*)gtk_builder_get_object(builder, "dlg");
    data->notebook = (GtkWidget*)gtk_builder_get_object(builder, "notebook");
    scroll = (GtkWidget*)gtk_builder_get_object(builder, "apps_scroll");
    file_type = (GtkWidget*)gtk_builder_get_object(builder, "file_type");
    data->cmdline = (GtkWidget*)gtk_builder_get_object(builder, "cmdline");
    data->set_default = (GtkWidget*)gtk_builder_get_object(builder, "set_default");
    data->use_terminal = (GtkWidget*)gtk_builder_get_object(builder, "use_terminal");
    data->status = (GtkWidget*)gtk_builder_get_object(builder, "status");
    data->mime_type = mime_type;

    gtk_dialog_set_alternative_button_order(GTK_DIALOG(data->dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);

    if(!can_set_default)
        gtk_widget_hide(data->set_default);

    if(mime_type && mime_type->type && mime_type->description)
        gtk_label_set_text(GTK_LABEL(file_type), mime_type->description);
    else
    {
        GtkWidget* hbox = (GtkWidget*)gtk_builder_get_object(builder, "file_type_hbox");
        gtk_widget_destroy(hbox);
        gtk_widget_hide(data->set_default);
    }

    data->apps_view = fm_app_menu_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->apps_view), FALSE);
    gtk_widget_show(data->apps_view);
    gtk_container_add(GTK_CONTAINER(scroll), data->apps_view);
    gtk_widget_grab_focus(data->apps_view);

    g_object_unref(builder);

    g_object_set_qdata_full(G_OBJECT(data->dlg), fm_qdata_id, data, (GDestroyNotify)on_dlg_destroy);
    g_signal_connect(data->notebook, "switch-page", G_CALLBACK(on_switch_page), data);
    on_switch_page(GTK_NOTEBOOK(data->notebook), NULL, 0, data);
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->apps_view));
    g_signal_connect(tree_sel, "changed", G_CALLBACK(on_apps_view_sel_changed), data);
    g_signal_connect(data->cmdline, "changed", G_CALLBACK(on_cmdline_changed), data);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(data->dlg), GTK_RESPONSE_OK, FALSE);

    return data->dlg;
}

inline static char* get_binary(const char* cmdline, gboolean* arg_found)
{
    /* see if command line contains %f, %F, %u, or %U. */
    const char* p = strstr(cmdline, " %");
    if(p)
    {
        if( !strchr("fFuU", *(p + 2)) )
            p = NULL;
    }
    if(arg_found)
        *arg_found = (p != NULL);
    if(p)
        return g_strndup(cmdline, p - cmdline);
    else
        return g_strdup(cmdline);
}

GAppInfo* fm_app_chooser_dlg_get_selected_app(GtkDialog* dlg, gboolean* set_default)
{
    GAppInfo* app = NULL;
    AppChooserData* data = (AppChooserData*)g_object_get_qdata(G_OBJECT(dlg), fm_qdata_id);
    switch( gtk_notebook_get_current_page(GTK_NOTEBOOK(data->notebook)) )
    {
    case 0: /* all applications */
        app = fm_app_menu_view_get_selected_app(GTK_TREE_VIEW(data->apps_view));
        break;
    case 1: /* custom cmd line */
        {
            const char* cmdline = gtk_entry_get_text(GTK_ENTRY(data->cmdline));
            if(cmdline && cmdline[0])
            {
                char* _cmdline = NULL;
                gboolean arg_found = FALSE;
                char* bin1 = get_binary(cmdline, &arg_found);
                g_debug("bin1 = %s", bin1);
                /* see if command line contains %f, %F, %u, or %U. */
                if(!arg_found)  /* append %f if no %f, %F, %u, or %U was found. */
                    cmdline = _cmdline = g_strconcat(cmdline, " %f", NULL);

                /* FIXME: is there any better way to do this? */
                /* We need to ensure that no duplicated items are added */
                if(data->mime_type)
                {
                    MenuCache* menu_cache;
                    #if GLIB_CHECK_VERSION(2, 10, 0)
                    /* see if the command is already in the list of known apps for this mime-type */
                    GList* apps = g_app_info_get_all_for_type(data->mime_type->type);
                    GList* l;
                    for(l=apps;l;l=l->next)
                    {
                        GAppInfo* app2 = (GAppInfo*)l->data;
                        const char* cmd = g_app_info_get_commandline(app2);
                        char* bin2 = get_binary(cmd, NULL);
                        if(g_strcmp0(bin1, bin2) == 0)
                        {
                            app = (GAppInfo*)g_object_ref(app2);
                            g_debug("found in app list");
                            g_free(bin2);
                            break;
                        }
                        g_free(bin2);
                    }
                    g_list_foreach(apps, (GFunc)g_object_unref, NULL);
                    g_list_free(apps);
                    if(app)
                        goto _out;
                    #endif

                    /* see if this command can be found in menu cache */
                    menu_cache = menu_cache_lookup("applications.menu");
                    if(menu_cache)
                    {
                        if(menu_cache_get_root_dir(menu_cache))
                        {
                            GSList* all_apps = menu_cache_list_all_apps(menu_cache);
                            GSList* l;
                            for(l=all_apps;l;l=l->next)
                            {
                                MenuCacheApp* ma = MENU_CACHE_APP(l->data);
                                char* bin2 = get_binary(menu_cache_app_get_exec(ma), NULL);
                                if(g_strcmp0(bin1, bin2) == 0)
                                {
                                    app = g_desktop_app_info_new(menu_cache_item_get_id(MENU_CACHE_ITEM(ma)));
                                    g_debug("found in menu cache");
                                    menu_cache_item_unref(MENU_CACHE_ITEM(ma));
                                    g_free(bin2);
                                    break;
                                }
                                menu_cache_item_unref(MENU_CACHE_ITEM(ma));
                                g_free(bin2);
                            }
                            g_slist_free(all_apps);
                        }
                        menu_cache_unref(menu_cache);
                        if(app)
                            goto _out;
                    }
                }

                /* FIXME: g_app_info_create_from_commandline force the use of %f or %u, so this is not we need */
                app = fm_app_info_create_from_commandline(cmdline, bin1, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->use_terminal)));
            _out:
                g_free(bin1);
                g_free(_cmdline);
            }
        }
        break;
    }

    if(set_default)
        *set_default = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->set_default));
    return app;
}

GAppInfo* fm_choose_app_for_mime_type(GtkWindow* parent, FmMimeType* mime_type, gboolean can_set_default)
{
    GAppInfo* app = NULL;
    GtkWidget* dlg = fm_app_chooser_dlg_new(mime_type, can_set_default);
    if(parent)
        gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);
    if(gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
    {
        gboolean set_default;
        app = fm_app_chooser_dlg_get_selected_app(GTK_DIALOG(dlg), &set_default);

        if(app && mime_type && mime_type->type)
        {
            GError* err = NULL;
            /* add this app to the mime-type */
            if(!g_app_info_add_supports_type(app, mime_type->type, &err))
            {
                g_debug("error: %s", err->message);
                g_error_free(err);
            }
            /* if need to set default */
            if(set_default)
                g_app_info_set_as_default_for_type(app, mime_type->type, NULL);
        }
    }
    gtk_widget_destroy(dlg);
    return app;
}
