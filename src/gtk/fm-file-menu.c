/*
 *      fm-file-menu.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "fm.h"
#include "fm-config.h"

#include "fm-file-menu.h"
#include "fm-path.h"

#include "fm-clipboard.h"
#include "fm-file-properties.h"
#include "fm-utils.h"
#include "fm-gtk-utils.h"
#include "fm-app-chooser-dlg.h"
#include "fm-archiver.h"

static void on_open(GtkAction* action, gpointer user_data);
static void on_open_with_app(GtkAction* action, gpointer user_data);
static void on_open_with(GtkAction* action, gpointer user_data);
static void on_cut(GtkAction* action, gpointer user_data);
static void on_copy(GtkAction* action, gpointer user_data);
static void on_paste(GtkAction* action, gpointer user_data);
static void on_delete(GtkAction* action, gpointer user_data);
static void on_rename(GtkAction* action, gpointer user_data);
static void on_compress(GtkAction* action, gpointer user_data);
static void on_extract_here(GtkAction* action, gpointer user_data);
static void on_extract_to(GtkAction* action, gpointer user_data);
static void on_prop(GtkAction* action, gpointer user_data);

const char base_menu_xml[]=
"<popup>"
  "<menuitem action='Open'/>"
  "<separator/>"
  "<placeholder name='ph1'/>"
  "<separator/>"
  "<placeholder name='ph2'/>"
  "<separator/>"
  "<menuitem action='Cut'/>"
  "<menuitem action='Copy'/>"
  "<menuitem action='Paste'/>"
  "<menuitem action='Del'/>"
  "<separator/>"
  "<menuitem action='Rename'/>"
/* TODO: implement symlink creation and "send to".
  "<menuitem action='Link'/>"
  "<menu action='SendTo'>"
  "</menu>"
*/
  "<separator/>"
  "<placeholder name='ph3'/>"
  "<separator/>"
  "<menuitem action='Prop'/>"
"</popup>";

/* FIXME: how to show accel keys in the popup menu? */
GtkActionEntry base_menu_actions[]=
{
    {"Open", GTK_STOCK_OPEN, NULL, NULL, NULL, on_open},
    {"OpenWith", NULL, N_("Open With..."), NULL, NULL, on_open_with},
    {"OpenWithMenu", NULL, N_("Open With..."), NULL, NULL, NULL},
    {"Cut", GTK_STOCK_CUT, NULL, "<Ctrl>X", NULL, on_cut},
    {"Copy", GTK_STOCK_COPY, NULL, "<Ctrl>C", NULL, on_copy},
    {"Paste", GTK_STOCK_PASTE, NULL, "<Ctrl>V", NULL, on_paste},
    {"Del", GTK_STOCK_DELETE, NULL, NULL, NULL, on_delete},
    {"Rename", NULL, N_("Rename"), "F2", NULL, on_rename},
    {"Link", NULL, N_("Create Symlink"), NULL, NULL, NULL},
    {"SendTo", NULL, N_("Send To"), NULL, NULL, NULL},
    {"Compress", NULL, N_("Compress..."), NULL, NULL, on_compress},
    {"Extract", NULL, N_("Extract Here"), NULL, NULL, on_extract_here},
    {"Extract2", NULL, N_("Extract To..."), NULL, NULL, on_extract_to},
    {"Prop", GTK_STOCK_PROPERTIES, NULL, NULL, NULL, on_prop}
};

void fm_file_menu_destroy(FmFileMenu* menu)
{
    if(menu->menu)
        gtk_widget_destroy(menu->menu);

    if(menu->file_infos)
        fm_list_unref(menu->file_infos);

    if(menu->cwd)
        fm_path_unref(menu->cwd);

    g_object_unref(menu->act_grp);
    g_object_unref(menu->ui);
    g_slice_free(FmFileMenu, menu);
}

FmFileMenu* fm_file_menu_new_for_file(FmFileInfo* fi, FmPath* cwd, gboolean auto_destroy)
{
    FmFileMenu* menu;
    FmFileInfoList* files = fm_file_info_list_new();
    fm_list_push_tail(files, fi);
    menu = fm_file_menu_new_for_files(files, cwd, auto_destroy);
    fm_list_unref(files);
    return menu;
}

FmFileMenu* fm_file_menu_new_for_files(FmFileInfoList* files, FmPath* cwd, gboolean auto_destroy)
{
    GtkWidget* menu;
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkAccelGroup* accel_grp;
    FmFileInfo* fi;
    FmFileMenu* data = g_slice_new0(FmFileMenu);
    GString* xml;

    data->auto_destroy = auto_destroy;
    data->ui = ui = gtk_ui_manager_new();
    data->act_grp = act_grp = gtk_action_group_new("Popup");
    gtk_action_group_set_translation_domain(act_grp, GETTEXT_PACKAGE);

    data->file_infos = fm_list_ref(files);
    if(cwd)
        data->cwd = fm_path_ref(cwd);

    gtk_action_group_add_actions(act_grp, base_menu_actions, G_N_ELEMENTS(base_menu_actions), data);
    gtk_ui_manager_add_ui_from_string(ui, base_menu_xml, -1, NULL);
    gtk_ui_manager_insert_action_group(ui, act_grp, 0);

    /* check if the files are of the same type */
    data->same_type = fm_file_info_list_is_same_type(files);

    xml = g_string_new("<popup><placeholder name='ph2'>");
    if(data->same_type) /* add specific menu items for this mime type */
    {
        fi = (FmFileInfo*)fm_list_peek_head(files);
        if(fi->type)
        {
            GtkAction* act;
            GList* apps = g_app_info_get_all_for_type(fi->type->type);
            GList* l;
            gboolean use_sub = g_list_length(apps) > 5;
            if(use_sub)
                g_string_append(xml, "<menu action='OpenWithMenu'>");

            for(l=apps;l;l=l->next)
            {
                GAppInfo* app = l->data;
                act = gtk_action_new(g_app_info_get_id(app),
                            g_app_info_get_name(app),
                            g_app_info_get_description(app),
                            NULL);
                g_signal_connect(act, "activate", G_CALLBACK(on_open_with_app), data);
                gtk_action_set_gicon(act, g_app_info_get_icon(app));
                gtk_action_group_add_action(act_grp, act);
                /* associate the app info object with the action */
                g_object_set_qdata_full(G_OBJECT(act), fm_qdata_id, app, (GDestroyNotify)g_object_unref);
                g_string_append_printf(xml, "<menuitem action='%s'/>", g_app_info_get_id(app));
            }

            g_list_free(apps);
            if(use_sub)
            {
                g_string_append(xml,
                    "<separator/>"
                    "<menuitem action='OpenWith'/>"
                    "</menu>");
            }
            else
                g_string_append(xml, "<menuitem action='OpenWith'/>");
        }
    }
    else
        g_string_append(xml, "<menuitem action='OpenWith'/>");
    g_string_append(xml, "</placeholder></popup>");

    /* archiver integration */
    g_string_append(xml, "<popup><placeholder name='ph3'>");
    if(data->same_type)
    {
        FmArchiver* archiver = fm_archiver_get_default();
        if(archiver)
        {
            FmFileInfo* fi = (FmFileInfo*)fm_list_peek_head(data->file_infos);
            if(fm_archiver_is_mime_type_supported(archiver, fi->type->type))
            {
                if(data->cwd && archiver->extract_to_cmd)
                    g_string_append(xml, "<menuitem action='Extract'/>");
                if(archiver->extract_cmd)
                    g_string_append(xml, "<menuitem action='Extract2'/>");
            }
            else
                g_string_append(xml, "<menuitem action='Compress'/>");
        }
    }
    else
        g_string_append(xml, "<menuitem action='Compress'/>");
    g_string_append(xml, "</placeholder></popup>");

    gtk_ui_manager_add_ui_from_string(ui, xml->str, xml->len, NULL);

    g_string_free(xml, TRUE);
    return data;
}

GtkUIManager* fm_file_menu_get_ui(FmFileMenu* menu)
{
    return menu->ui;
}

GtkActionGroup* fm_file_menu_get_action_group(FmFileMenu* menu)
{
    return menu->act_grp;
}

FmFileInfoList* fm_file_menu_get_file_info_list(FmFileMenu* menu)
{
    return menu->file_infos;
}

/* build the menu with GtkUIManager */
GtkMenu* fm_file_menu_get_menu(FmFileMenu* menu)
{
    if( ! menu->menu )
    {
        menu->menu = gtk_ui_manager_get_widget(menu->ui, "/popup");
        if(menu->auto_destroy)
            g_signal_connect_swapped(menu->menu, "selection-done",
                            G_CALLBACK(fm_file_menu_destroy), menu);
    }
    return menu->menu;
}

void on_open(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    GList* l = fm_list_peek_head_link(data->file_infos);
    GError* err = NULL;
    fm_launch_files_simple(GTK_WINDOW(gtk_widget_get_toplevel(data->menu)), NULL, l, data->folder_func, data->folder_func_data);
}

static void open_with_app(FmFileMenu* data, GAppInfo* app)
{
    GdkAppLaunchContext* ctx;
    FmFileInfoList* files = data->file_infos;
    GList* l = fm_list_peek_head_link(files);
    char** uris = g_new0(char*, fm_list_get_length(files) + 1);
    int i;
    for(i=0; l; ++i, l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        FmPath* path = fi->path;
        char* uri = fm_path_to_uri(path);
        uris[i] = uri;
    }

    ctx = gdk_app_launch_context_new();
    gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(data->menu));
    gdk_app_launch_context_set_icon(ctx, g_app_info_get_icon(app));
    gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());

    /* FIXME: error handling. */
    g_app_info_launch_uris(app, uris, ctx, NULL);
    g_object_unref(ctx);

    g_free(uris);
}

void on_open_with_app(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    GAppInfo* app = (GAppInfo*)g_object_get_qdata(G_OBJECT(action), fm_qdata_id);
    g_debug("%s", gtk_action_get_name(action));
    open_with_app(data, app);
}

void on_open_with(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmFileInfoList* files = data->file_infos;
    FmFileInfo* fi = (FmFileInfo*)fm_list_peek_head(files);
    FmMimeType* mime_type;
    GAppInfo* app;

    if(data->same_type && fi->type && fi->type->type)
        mime_type = fi->type;
    else
        mime_type = NULL;

    app = fm_choose_app_for_mime_type(NULL, mime_type, TRUE);

    if(app)
    {
        open_with_app(data, app);
        g_object_unref(app);
    }
}

void on_cut(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_clipboard_cut_files(data->menu, files);
    fm_list_unref(files);
}

void on_copy(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_clipboard_copy_files(data->menu, files);
    fm_list_unref(files);
}

void on_paste(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    /* fm_clipboard_paste_files(data->menu, ); */
}

void on_delete(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_trash_or_delete_files(files);
    fm_list_unref(files);
}

void on_rename(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmFileInfo* fi = fm_list_peek_head(data->file_infos);
    if(fi)
        fm_rename_file(fi->path);
    /* FIXME: is it ok to only rename the first selected file here. */
/*
    FmPathList* files;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    if( !fm_list_is_empty(files) )
        fm_delete_files(files);
    fm_list_unref(files);
*/
}

void on_compress(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    GAppLaunchContext* ctx = gdk_app_launch_context_new();
    FmArchiver* archiver = fm_archiver_get_default();
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_archiver_create_archive(archiver, ctx, files);
    fm_list_unref(files);
    g_object_unref(ctx);
}

void on_extract_here(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    GAppLaunchContext* ctx = gdk_app_launch_context_new();
    FmArchiver* archiver = fm_archiver_get_default();
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_archiver_extract_archives_to(archiver, ctx, files, data->cwd);
    fm_list_unref(files);
    g_object_unref(ctx);
}

void on_extract_to(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    GAppLaunchContext* ctx = gdk_app_launch_context_new();
    FmArchiver* archiver = fm_archiver_get_default();
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_archiver_extract_archives(archiver, ctx, files);
    fm_list_unref(files);
    g_object_unref(ctx);
}

void on_prop(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    fm_show_file_properties(data->file_infos);
}

gboolean fm_file_menu_is_single_file_type(FmFileMenu* menu)
{
    return menu->same_type;
}

void fm_file_menu_set_folder_func(FmFileMenu* menu, FmLaunchFolderFunc func, gpointer user_data)
{
    menu->folder_func = func;
    menu->folder_func_data = user_data;
}


