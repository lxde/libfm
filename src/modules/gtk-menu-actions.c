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

#include "fm.h"
#include "fm-folder-view.h"
#include "fm-gtk-utils.h"
#include "gtk-compat.h"

#include "fm-actions.h"

static void on_custom_action_file(GtkAction* act, gpointer menu)
{
    FmFileActionItem* item = FM_FILE_ACTION_ITEM(g_object_get_qdata(G_OBJECT(act), fm_qdata_id));
    GdkAppLaunchContext* ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
    GList* files = fm_file_info_list_peek_head_link(fm_file_menu_get_file_info_list(menu));
    char* output = NULL;
    gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(GTK_WIDGET(fm_file_menu_get_menu(menu))));
    gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());

    /* g_debug("item: %s is activated, id:%s", fm_file_action_item_get_name(item),
        fm_file_action_item_get_id(item)); */
    fm_file_action_item_launch(item, G_APP_LAUNCH_CONTEXT(ctx), files, &output);
    if(output)
    {
        fm_show_error(NULL, "output", output);
        g_free(output);
    }
    g_object_unref(ctx);
}

static void on_custom_action_folder(GtkAction* act, gpointer folder_view)
{
    FmFileActionItem* item = FM_FILE_ACTION_ITEM(g_object_get_qdata(G_OBJECT(act), fm_qdata_id));
    GdkAppLaunchContext* ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
    GList* files = g_list_prepend(NULL, fm_folder_view_get_cwd_info(folder_view));
    char* output = NULL;
    gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(folder_view));
    gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());

    /* g_debug("item: %s is activated, id:%s", fm_file_action_item_get_name(item),
        fm_file_action_item_get_id(item)); */
    fm_file_action_item_launch(item, G_APP_LAUNCH_CONTEXT(ctx), files, &output);
    if(output)
    {
        fm_show_error(NULL, "output", output);
        g_free(output);
    }
    g_object_unref(ctx);
    g_list_free(files);
}

static void add_custom_action_item(GString* xml, FmFileActionItem* item,
                                   GtkActionGroup* act_grp,
                                   GCallback cb, gpointer cb_data)
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
        g_signal_connect(act, "activate", cb, cb_data);

    gtk_action_set_icon_name(act, fm_file_action_item_get_icon(item));
    gtk_action_group_add_action(act_grp, act);
    /* associate the app info object with the action */
    g_object_set_qdata_full(G_OBJECT(act), fm_qdata_id,
                            fm_file_action_item_ref(item),
                            fm_file_action_item_unref);
    if(fm_file_action_item_is_menu(item))
    {
        GList* subitems = fm_file_action_item_get_sub_items(item);
        GList* l;
        g_string_append_printf(xml, "<menu action='%s'>",
                               fm_file_action_item_get_id(item));
        for(l=subitems; l; l=l->next)
        {
            FmFileActionItem* subitem = FM_FILE_ACTION_ITEM(l->data);
            add_custom_action_item(xml, subitem, act_grp, cb, cb_data);
        }
        g_string_append(xml, "</menu>");
    }
    else
    {
        g_string_append_printf(xml, "<menuitem action='%s'/>",
                               fm_file_action_item_get_id(item));
    }
}

static void
_fm_actions_update_file_menu_for_scheme(GtkWindow* window, GtkUIManager* ui,
                                        GString* xml, GtkActionGroup* act_grp,
                                        FmFileMenu* menu, FmFileInfoList* files,
                                        gboolean single_file)
{
    GList* files_list = fm_file_info_list_peek_head_link(files);
    GList* items = fm_get_actions_for_files(files_list);

    /* add custom file actions */
    if(items)
    {
        g_string_append(xml, "<popup><placeholder name='ph3'>");
        GList* l;
        for(l=items; l; l=l->next)
        {
            FmFileActionItem* item = FM_FILE_ACTION_ITEM(l->data);
            add_custom_action_item(xml, item, act_grp,
                                   G_CALLBACK(on_custom_action_file), menu);
        }
        g_string_append(xml, "</placeholder></popup>");
    }
    g_list_foreach(items, (GFunc)fm_file_action_item_unref, NULL);
    g_list_free(items);
}

static void
_fm_actions_update_folder_menu_for_scheme(FmFolderView* fv, GtkWindow* window,
                                          GtkUIManager* ui, GtkActionGroup* act_grp,
                                          FmFileInfoList* files)
{
    FmFileInfo *fi = fm_folder_view_get_cwd_info(fv);
    GList *files_list, *items;

    if (fi == NULL) /* incremental folder - no info yet - ignore it */
        return;
    files_list = g_list_prepend(NULL, fm_folder_view_get_cwd_info(fv));
    items = fm_get_actions_for_files(files_list);
    if(items)
    {
        GString *xml = g_string_new("<popup><placeholder name='CustomCommonOps'>");
        GList* l;

        for(l=items; l; l=l->next)
        {
            FmFileActionItem* item = FM_FILE_ACTION_ITEM(l->data);
            add_custom_action_item(xml, item, act_grp,
                                   G_CALLBACK(on_custom_action_folder), fv);
        }
        g_string_append(xml, "</placeholder></popup>");
        gtk_ui_manager_add_ui_from_string(ui, xml->str, xml->len, NULL);
        g_string_free(xml, TRUE);
    }
    g_list_foreach(items, (GFunc)fm_file_action_item_unref, NULL);
    g_list_free(items);
    g_list_free(files_list);
}

/* we catch all schemes to be available on every one */
FM_DEFINE_MODULE(gtk_menu_scheme, *)

FmContextMenuSchemeAddonInit fm_module_init_gtk_menu_scheme = {
    .init = NULL,
    .finalize = NULL,
    _fm_actions_update_file_menu_for_scheme,
    _fm_actions_update_folder_menu_for_scheme
};
