/*
 *      libfm-prefapps.c
 *
 *      Copyright 2010 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>

#include <menu-cache.h>
#include "fm-gtk.h"

static GtkDialog* dlg;
static GtkComboBox* browser;
static GtkComboBox* mail_client;

static GList* browsers = NULL;
static GList* mail_clients = NULL;

/* examine desktop files under Internet and Office submenus to find out browsers and mail clients */
static void init_apps(void)
{
    MenuCache* mc = menu_cache_lookup_sync("applications.menu");
    MenuCacheDir* dir;
    GKeyFile* kf;
    char* fpath;
    static const char* dir_paths[] = {"/Applications/Internet", "/Applications/Office"};
    gsize i;
    GDesktopAppInfo* app;

    kf = g_key_file_new();
    /* load additional custom apps from user config */
    fpath = g_build_filename(g_get_user_config_dir(), "libfm/pref-apps.conf", NULL);
    if(g_key_file_load_from_file(kf, fpath, 0, NULL))
    {
        char** desktop_ids;
        gsize n;
        desktop_ids = g_key_file_get_string_list(kf, "Preferred Applications", "CustomWebBrowsers", &n, NULL);
        for(i=0; i<n; ++i)
        {
            app = g_desktop_app_info_new(desktop_ids[i]);
            if(app)
                browsers = g_list_prepend(browsers, app);
        }
        g_strfreev(desktop_ids);

        desktop_ids = g_key_file_get_string_list(kf, "Preferred Applications", "CustomMailClients", &n, NULL);
        for(i=0; i<n; ++i)
        {
            app = g_desktop_app_info_new(desktop_ids[i]);
            if(app)
                mail_clients = g_list_prepend(mail_clients, app);
        }
        g_strfreev(desktop_ids);
    }
    g_free(fpath);

    if(!mc)
        return;

    for(i = 0; i < G_N_ELEMENTS(dir_paths); ++i)
    {
        dir = menu_cache_get_dir_from_path(mc, dir_paths[i]);
        if(dir)
        {
            GSList* l;
            for(l=menu_cache_dir_get_children(dir);l;l=l->next)
            {
                MenuCacheItem* item = MENU_CACHE_ITEM(l->data);
                if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP)
                {
                    /* workarounds for firefox since it doesn't have correct categories. */
                    if((strcmp(menu_cache_item_get_id(item), "firefox.desktop") == 0) || (strcmp(menu_cache_item_get_id(item), "MozillaFirefox.desktop") == 0))
                    {
                        app = g_desktop_app_info_new(menu_cache_item_get_id(item));
                        if(app)
                            browsers = g_list_prepend(browsers, app);
                        continue;
                    }
                    fpath = menu_cache_item_get_file_path(item);
                    if(g_key_file_load_from_file(kf, fpath, 0, NULL))
                    {
                        gsize n;
                        char** cats = g_key_file_get_string_list(kf, "Desktop Entry", "Categories", &n, NULL);
                        if(cats)
                        {
                            char** cat;
                            for(cat = cats; *cat; ++cat)
                            {
                                if(strcmp(*cat, "WebBrowser")==0)
                                {
                                    app = g_desktop_app_info_new(menu_cache_item_get_id(item));
                                    if(app)
                                        browsers = g_list_prepend(browsers, app);
                                }
                                else if(strcmp(*cat, "Email")==0)
                                {
                                    app = g_desktop_app_info_new(menu_cache_item_get_id(item));
                                    if(app)
                                        mail_clients = g_list_prepend(mail_clients, app);
                                }
                            }
                            g_strfreev(cats);
                        }
                    }
                    g_free(fpath);
                }
            }
        }
    }
    g_key_file_free(kf);

    menu_cache_unref(mc);
}

int main(int argc, char** argv)
{
    GtkBuilder* b;
    GAppInfo* app;

#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain( GETTEXT_PACKAGE );
#endif

    gtk_init(&argc, &argv);
    fm_gtk_init(NULL);

    b = gtk_builder_new();
    gtk_builder_add_from_file(b, PACKAGE_UI_DIR "/preferred-apps.ui", NULL);
    dlg = GTK_DIALOG(gtk_builder_get_object(b, "dlg"));
    browser = GTK_COMBO_BOX(gtk_builder_get_object(b, "browser"));
    mail_client = GTK_COMBO_BOX(gtk_builder_get_object(b, "mail_client"));
    g_object_unref(b);

    /* make sure we're using menu from lxmenu-data */
    g_setenv("XDG_MENU_PREFIX", "lxde-", TRUE);
    init_apps();

    app = g_app_info_get_default_for_uri_scheme("http");
    fm_app_chooser_combo_box_setup_custom(browser, browsers, app);
    g_list_foreach(browsers, (GFunc)g_object_unref, NULL);
    g_list_free(browsers);
    if(app)
        g_object_unref(app);

    app = g_app_info_get_default_for_uri_scheme("mailto");
    fm_app_chooser_combo_box_setup_custom(mail_client, mail_clients, app);
    g_list_foreach(mail_clients, (GFunc)g_object_unref, NULL);
    g_list_free(mail_clients);
    if(app)
        g_object_unref(app);

    if(gtk_dialog_run(dlg) == GTK_RESPONSE_OK)
    {
        GKeyFile* kf = g_key_file_new();
        char* buf;
        gsize len, i;
        gboolean is_changed;
        GAppInfo* app;
        const GList* custom_apps, *l;
        char* dir = g_build_filename(g_get_user_config_dir(), "libfm", NULL);
        char* fname = g_build_filename(dir, "pref-apps.conf", NULL);

        g_mkdir_with_parents(dir, 0700); /* ensure the user config dir */
        g_free(dir);

        g_key_file_load_from_file(kf, fname, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

        /* get currently selected web browser */
        app = fm_app_chooser_combo_box_dup_selected_app(browser, &is_changed);
        if(app)
        {
            if(is_changed)
            {
//                g_key_file_set_string(kf, "Preferred Applications", "WebBrowser", g_app_info_get_id(app));
                g_app_info_set_as_default_for_type(app, "x-scheme-handler/http", NULL);
            }
            g_object_unref(app);
        }
        custom_apps = fm_app_chooser_combo_box_get_custom_apps(browser);
        if(custom_apps)
        {
            const char** sl;
            len = g_list_length((GList*)custom_apps);
            sl = (const char**)g_new0(char*, len);
            for(i = 0, l=custom_apps;l;l=l->next, ++i)
            {
                app = G_APP_INFO(l->data);
                sl[i] = g_app_info_get_id(app);
            }
            g_key_file_set_string_list(kf, "Preferred Applications", "CustomWebBrowsers", sl, len);
            g_free(sl);
            /* custom_apps is owned by the combobox and shouldn't be freed. */
        }

        /* get selected mail client */
        app = fm_app_chooser_combo_box_dup_selected_app(mail_client, &is_changed);
        if(app)
        {
            if(is_changed)
            {
                // g_key_file_set_string(kf, "Preferred Applications", "MailClient", g_app_info_get_id(app));
                g_app_info_set_as_default_for_type(app, "x-scheme-handler/mailto", NULL);
            }
            g_object_unref(app);
        }
        custom_apps = fm_app_chooser_combo_box_get_custom_apps(mail_client);
        if(custom_apps)
        {
            const char** sl;
            len = g_list_length((GList*)custom_apps);
            sl = (const char**)g_new0(char*, len);
            for(i = 0, l=custom_apps;l;l=l->next, ++i)
            {
                app = G_APP_INFO(l->data);
                sl[i] = g_app_info_get_id(app);
            }
            g_key_file_set_string_list(kf, "Preferred Applications", "CustomMailClients", sl, len);
            g_free(sl);
            /* custom_apps is owned by the combobox and shouldn't be freed. */
        }

        buf = g_key_file_to_data(kf, &len, NULL);
        if(buf)
        {
            char* pbuf;
            /* remove leading '\n' */
            if( buf[0] == '\n' )
            {
                pbuf = buf + 1;
                --len;
            }
            else
                pbuf = buf;
            g_file_set_contents(fname, pbuf, len, NULL);
            g_free(buf);
        }
        g_key_file_free(kf);
        g_free(fname);
    }
    gtk_widget_destroy((GtkWidget*)dlg);

    fm_gtk_finalize();

    return 0;
}
