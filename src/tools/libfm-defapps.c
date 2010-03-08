/*
 *      libfm-defapps.c
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
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
#include "fm-gtk.h"

static GtkWidget* dlg;
static GtkWidget* browser;
static GtkWidget*mail_client;

int main(int argc, char** argv)
{
    GtkBuilder* b;
    GAppInfo* app;
    char* cmd;
    gtk_init(&argc, &argv);

    b = gtk_builder_new();
    gtk_builder_add_from_file(b, PACKAGE_UI_DIR "/default-apps.ui", NULL);
    dlg = GTK_WIDGET(gtk_builder_get_object(b, "dlg"));
    browser = GTK_WIDGET(gtk_builder_get_object(b, "browser"));
    mail_client = GTK_WIDGET(gtk_builder_get_object(b, "mail_client"));
    g_object_unref(b);

    app = g_app_info_get_default_for_uri_scheme("http");
    if(app)
    {
        cmd = g_app_info_get_commandline(app);
        gtk_entry_set_text(browser, cmd ? cmd : "");
        g_object_unref(app);
    }

    app = g_app_info_get_default_for_uri_scheme("mailto");
    if(app)
    {
        cmd = g_app_info_get_commandline(app);
        gtk_entry_set_text(mail_client, cmd ? cmd : "");
        g_object_unref(app);
    }

    if(gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
    {
        GKeyFile* kf = g_key_file_new();
        const char* new_cmd;
        char* buf;
        gsize len;
        char* dir = g_build_filename(g_get_user_config_dir(), "libfm", NULL);
        char* fname = g_build_filename(dir, "pref-apps.conf", NULL);

        g_mkdir_with_parents(dir, 0700);
        g_free(dir);

        g_key_file_load_from_file(kf, fname, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

        new_cmd = gtk_entry_get_text(browser);
        g_key_file_set_string(kf, "Preferred Applications", "WebBrowser", new_cmd);

        new_cmd = gtk_entry_get_text(mail_client);
        g_key_file_set_string(kf, "Preferred Applications", "MailClient", new_cmd);

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
    gtk_widget_destroy(dlg);

	return 0;
}
