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
#include "../gtk-compat.h"

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
#include "fm-app-info.h"
#include "fm-gtk-file-launcher.h"

#include "fm-actions.h"

static void on_open(GtkAction* action, gpointer user_data);
static void on_open_with_app(GtkAction* action, gpointer user_data);
static void on_open_with(GtkAction* action, gpointer user_data);
static void on_cut(GtkAction* action, gpointer user_data);
static void on_copy(GtkAction* action, gpointer user_data);
static void on_paste(GtkAction* action, gpointer user_data);
static void on_delete(GtkAction* action, gpointer user_data);
static void on_untrash(GtkAction* action, gpointer user_data);
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
    {"Open", GTK_STOCK_OPEN, NULL, NULL, NULL, G_CALLBACK(on_open)},
    {"OpenWith", NULL, N_("Open With..."), NULL, NULL, G_CALLBACK(on_open_with)},
    {"OpenWithMenu", NULL, N_("Open With..."), NULL, NULL, NULL},
    {"Cut", GTK_STOCK_CUT, NULL, "<Ctrl>X", NULL, G_CALLBACK(on_cut)},
    {"Copy", GTK_STOCK_COPY, NULL, "<Ctrl>C", NULL, G_CALLBACK(on_copy)},
    {"Paste", GTK_STOCK_PASTE, NULL, "<Ctrl>V", NULL, G_CALLBACK(on_paste)},
    {"Del", GTK_STOCK_DELETE, NULL, NULL, NULL, G_CALLBACK(on_delete)},
    {"Rename", NULL, N_("Rename"), "F2", NULL, G_CALLBACK(on_rename)},
    {"Link", NULL, N_("Create Symlink"), NULL, NULL, NULL},
    {"SendTo", NULL, N_("Send To"), NULL, NULL, NULL},
    {"Compress", NULL, N_("Compress..."), NULL, NULL, G_CALLBACK(on_compress)},
    {"Extract", NULL, N_("Extract Here"), NULL, NULL, G_CALLBACK(on_extract_here)},
    {"Extract2", NULL, N_("Extract To..."), NULL, NULL, G_CALLBACK(on_extract_to)},
    {"Prop", GTK_STOCK_PROPERTIES, NULL, NULL, NULL, G_CALLBACK(on_prop)}
};

void fm_file_menu_destroy(FmFileMenu* menu)
{
    if(menu->parent)
        g_object_unref(menu->parent);

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

FmFileMenu* fm_file_menu_new_for_file(GtkWindow* parent, FmFileInfo* fi, FmPath* cwd, gboolean auto_destroy)
{
    FmFileMenu* menu;
    FmFileInfoList* files = fm_file_info_list_new();
    fm_list_push_tail(files, fi);
    menu = fm_file_menu_new_for_files(parent, files, cwd, auto_destroy);
    fm_list_unref(files);
    return menu;
}

static void on_custom_action(GtkAction* act, FmFileMenu* data)
{
	FmFileActionItem* item = FM_FILE_ACTION_ITEM(g_object_get_qdata(act, fm_qdata_id));
    GdkAppLaunchContext* ctx = gdk_app_launch_context_new();
    GList* files = fm_list_peek_head_link(data->file_infos);
    char** output = NULL;
    gboolean ret;
    gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(data->menu));
    gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());

	g_debug("item: %s is activated, id:%s", fm_file_action_item_get_name(item),
			fm_file_action_item_get_id(item));
	ret = fm_file_action_item_launch(item, ctx, files, output);
	if(output)
	{
		fm_show_error(NULL, "output", output);
		g_free(output);
	}
}

static void add_custom_action_item(FmFileMenu* data, GString* xml, FmFileActionItem* item)
{
	GtkAction* act;
	if(!item) /* separator */
	{
		g_string_append(xml, "<separator/>");
		return;
	}

	if(fm_file_action_item_is_action(item))
	{
		if(!(fm_file_action_item_get_target(item) & FM_FILE_ACTION_TARGET_CONTEXT))
			return;
	}

	act = gtk_action_new(fm_file_action_item_get_id(item),
						fm_file_action_item_get_name(item),
						fm_file_action_item_get_desc(item),
						NULL);

	if(fm_file_action_item_is_action(item))
		g_signal_connect(act, "activate", G_CALLBACK(on_custom_action), data);

	gtk_action_set_icon_name(act, fm_file_action_item_get_icon(item));
	gtk_action_group_add_action(data->act_grp, act);
	/* associate the app info object with the action */
	g_object_set_qdata_full(G_OBJECT(act), fm_qdata_id, 
							fm_file_action_item_ref(item),
							(GDestroyNotify)fm_file_action_item_unref);
	if(fm_file_action_item_is_menu(item))
	{
		GList* subitems = fm_file_action_item_get_sub_items(item);
		GList* l;
		g_string_append_printf(xml, "<menu action='%s'>",
							   fm_file_action_item_get_id(item));
		for(l=subitems; l; l=l->next)
		{
			FmFileActionItem* subitem = FM_FILE_ACTION_ITEM(l->data);
			add_custom_action_item(data, xml, subitem);
		}
		g_string_append(xml, "</menu>");
	}
	else
	{
		g_string_append_printf(xml, "<menuitem action='%s'/>",
							   fm_file_action_item_get_id(item));
	}
}

static void fm_file_menu_add_custom_actions(FmFileMenu* data, GString* xml, FmFileInfoList* files)
{
	GList* files_list = fm_list_peek_head_link(files);
	GList* items = fm_get_actions_for_files(files_list);

    if(items)
    {
		g_string_append(xml, "<popup><placeholder name='ph3'>");
		GList* l;
		for(l=items; l; l=l->next)
		{
			FmFileActionItem* item = FM_FILE_ACTION_ITEM(l->data);
			add_custom_action_item(data, xml, item);
		}
		g_string_append(xml, "</placeholder></popup>");
    }
	g_list_foreach(items, (GSourceFunc)fm_file_action_item_unref, NULL);
	g_list_free(items);
}

FmFileMenu* fm_file_menu_new_for_files(GtkWindow* parent, FmFileInfoList* files, FmPath* cwd, gboolean auto_destroy)
{
    GtkWidget* menu;
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkAccelGroup* accel_grp;
    GtkAction* act;
    FmFileInfo* fi = (FmFileInfo*)fm_list_peek_head(files);
    FmFileMenu* data = g_slice_new0(FmFileMenu);
    GString* xml;

    data->parent = g_object_ref(parent); /* FIXME: is this really needed? */
    /* FIXME: should we connect to "destroy" signal of parent and set data->parent to NULL when
     * it's detroyed? */
    data->file_infos = fm_list_ref(files);

    /* check if the files are of the same type */
    data->same_type = fm_file_info_list_is_same_type(files);

    /* check if the files are on the same filesystem */
    data->same_fs = fm_file_info_list_is_same_fs(files);

    data->all_virtual = data->same_fs && fm_path_is_virtual(fi->path);
    data->all_trash = data->same_fs && fm_path_is_trash(fi->path);

    data->auto_destroy = auto_destroy;
    data->ui = ui = gtk_ui_manager_new();
    data->act_grp = act_grp = gtk_action_group_new("Popup");
    gtk_action_group_set_translation_domain(act_grp, GETTEXT_PACKAGE);

    if(cwd)
        data->cwd = fm_path_ref(cwd);

    gtk_action_group_add_actions(act_grp, base_menu_actions, G_N_ELEMENTS(base_menu_actions), data);
    gtk_ui_manager_add_ui_from_string(ui, base_menu_xml, -1, NULL);
    gtk_ui_manager_insert_action_group(ui, act_grp, 0);

    xml = g_string_new("<popup><placeholder name='ph2'>");
    if(data->same_type) /* add specific menu items for this mime type */
    {
        if(fi->type && !data->all_virtual ) /* the file has a valid mime-type and its not virtual */
        {
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

	/* add custom file actions */
	fm_file_menu_add_custom_actions(data, xml, files);

    /* archiver integration */
    if(!data->all_virtual)
    {
        g_string_append(xml, "<popup><placeholder name='ph3'>");
        if(data->same_type)
        {
            FmArchiver* archiver = fm_archiver_get_default();
            if(archiver)
            {
                fi = (FmFileInfo*)fm_list_peek_head(files);
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
    }

    /* Special handling for some virtual filesystems */
    g_string_append(xml, "<popup><placeholder name='ph1'>");
    if(data->all_virtual)
    {
        /* if all of the files are in trash */
        if(data->all_trash)
        {
            gboolean can_restore = TRUE;
            GList* l;
            /* only immediate children of trash:/// can be restored. */
            for(l = fm_list_peek_head_link(files);l;l=l->next)
            {
                FmPath* trash_path = FM_FILE_INFO(l->data)->path;
                if(!trash_path->parent || !fm_path_is_trash_root(trash_path->parent))
                {
                    can_restore = FALSE;
                    break;
                }
            }

            if(can_restore)
            {
                act = gtk_action_new("UnTrash",
                                    _("_Restore"),
                                    _("Restore trashed files to original paths"),
                            NULL);
                g_signal_connect(act, "activate", G_CALLBACK(on_untrash), data);
                gtk_action_group_add_action(act_grp, act);
                g_string_append(xml, "<menuitem action='UnTrash'/>");
            }

            act = gtk_ui_manager_get_action(ui, "/popup/Open");
            gtk_action_set_visible(act, FALSE);
        }
        else
        {
            /* do not provide these items for other virtual files */
            act = gtk_ui_manager_get_action(ui, "/popup/Cut");
            gtk_action_set_visible(act, FALSE);
            act = gtk_ui_manager_get_action(ui, "/popup/Copy");
            gtk_action_set_visible(act, FALSE);
            act = gtk_ui_manager_get_action(ui, "/popup/Paste");
            gtk_action_set_visible(act, FALSE);
            act = gtk_ui_manager_get_action(ui, "/popup/Del");
            gtk_action_set_visible(act, FALSE);
        }
        act = gtk_ui_manager_get_action(ui, "/popup/Rename");
        gtk_action_set_visible(act, FALSE);
    }
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
        gtk_menu_attach_to_widget(GTK_MENU(menu->menu), GTK_WIDGET(menu->parent), NULL);

        if(menu->auto_destroy)
        {
            g_signal_connect_swapped(menu->menu, "selection-done",
                            G_CALLBACK(fm_file_menu_destroy), menu);
        }
    }
    return menu->menu;
}

void on_open(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    GList* l = fm_list_peek_head_link(data->file_infos);
    GError* err = NULL;
    fm_launch_files_simple(data->parent, NULL, l, data->folder_func, data->folder_func_data);
}

static void open_with_app(FmFileMenu* data, GAppInfo* app)
{
    GdkAppLaunchContext* ctx;
    FmFileInfoList* files = data->file_infos;
    GList* l = fm_list_peek_head_link(files);
    GList* uris = NULL;
    int i;
    for(i=0; l; ++i, l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        FmPath* path = fi->path;
        char* uri = fm_path_to_uri(path);
        uris = g_list_prepend(uris, uri);
    }
    uris = g_list_reverse(uris);

    ctx = gdk_app_launch_context_new();
    gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(data->menu));
    gdk_app_launch_context_set_icon(ctx, g_app_info_get_icon(app));
    gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());

    /* FIXME: error handling. */
    fm_app_info_launch_uris(app, uris, ctx, NULL);
    g_object_unref(ctx);

    g_list_foreach(uris, (GFunc)g_free, NULL);
    g_list_free(uris);
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

    app = fm_choose_app_for_mime_type(data->parent, mime_type, TRUE);

    if(app)
    {
        open_with_app(data, app);
        /* add the app to apps that support this file type. */
        if(mime_type)
            g_app_info_add_supports_type(app, mime_type->type, NULL);
        /* FIXME: what to do if mime_type is NULL? */
        g_object_unref(app);
    }
}

void on_cut(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_clipboard_cut_files(data->parent, files);
    fm_list_unref(files);
}

void on_copy(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_clipboard_copy_files(data->parent, files);
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
    fm_trash_or_delete_files(data->parent, files);
    fm_list_unref(files);
}

void on_untrash(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_untrash_files(data->parent, files);
    fm_list_unref(files);
}

void on_rename(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmFileInfo* fi = fm_list_peek_head(data->file_infos);
    if(fi)
        fm_rename_file(data->parent, fi->path);
    /* FIXME: is it ok to only rename the first selected file here? */
}

void on_compress(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    FmArchiver* archiver = fm_archiver_get_default();
    if(archiver)
    {
        GAppLaunchContext* ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
        files = fm_path_list_new_from_file_info_list(data->file_infos);
        fm_archiver_create_archive(archiver, ctx, files);
        fm_list_unref(files);
        g_object_unref(ctx);
    }
}

void on_extract_here(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    FmArchiver* archiver = fm_archiver_get_default();
    if(archiver)
    {
        GAppLaunchContext* ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
        files = fm_path_list_new_from_file_info_list(data->file_infos);
        fm_archiver_extract_archives_to(archiver, ctx, files, data->cwd);
        fm_list_unref(files);
        g_object_unref(ctx);
    }
}

void on_extract_to(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    FmArchiver* archiver = fm_archiver_get_default();
    if(archiver)
    {
        GAppLaunchContext* ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
        files = fm_path_list_new_from_file_info_list(data->file_infos);
        fm_archiver_extract_archives(archiver, ctx, files);
        fm_list_unref(files);
        g_object_unref(ctx);
    }
}

void on_prop(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    fm_show_file_properties(data->parent, data->file_infos);
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


