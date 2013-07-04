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

#ifdef HAVE_ACTIONS
/* disable all and module will never be loaded */

#include "fm.h"
#include "fm-folder-view.h"
#include "fm-gtk-utils.h"
#include "gtk-compat.h"

#include "fm-actions.h"

static void on_custom_action(GtkAction* act, FmFileMenu* data)
{
    FmFileActionItem* item = FM_FILE_ACTION_ITEM(g_object_get_qdata(G_OBJECT(act), fm_qdata_id));
    GdkAppLaunchContext* ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
    GList* files = fm_file_info_list_peek_head_link(fm_file_menu_get_file_info_list(data));
    char* output = NULL;
    gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(GTK_WIDGET(fm_file_menu_get_menu(data))));
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

static void add_custom_action_item(FmFileMenu* data, GString* xml, FmFileActionItem* item,
                                    GtkActionGroup* act_grp)
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
            add_custom_action_item(data, xml, subitem, act_grp);
        }
        g_string_append(xml, "</menu>");
    }
    else
    {
        g_string_append_printf(xml, "<menuitem action='%s'/>",
                               fm_file_action_item_get_id(item));
    }
}

static void fm_file_menu_add_custom_actions(FmFileMenu* data, GString* xml,
                                            FmFileInfoList* files, GtkActionGroup* act_grp)
{
    GList* files_list = fm_file_info_list_peek_head_link(files);
    GList* items = fm_get_actions_for_files(files_list);

    if(items)
    {
        g_string_append(xml, "<popup><placeholder name='ph3'>");
        GList* l;
        for(l=items; l; l=l->next)
        {
            FmFileActionItem* item = FM_FILE_ACTION_ITEM(l->data);
            add_custom_action_item(data, xml, item, act_grp);
        }
        g_string_append(xml, "</placeholder></popup>");
    }
    g_list_foreach(items, (GFunc)fm_file_action_item_unref, NULL);
    g_list_free(items);
}

static void
_fm_actions_update_file_menu_for_scheme(GtkWindow* window, GtkUIManager* ui,
                                        GString* xml, GtkActionGroup* act_grp,
                                        FmFileMenu* menu, FmFileInfoList* files,
                                        gboolean single_file)
{
    /* add custom file actions */
    fm_file_menu_add_custom_actions(menu, xml, files, act_grp);
}

static void
_fm_actions_update_folder_menu_for_scheme(FmFolderView* fv, GtkWindow* window,
                                        GtkUIManager* ui, GtkActionGroup* act_grp,
                                        FmFileInfoList* files)
{
}

/* we catch all schemes to be available on every one */
FM_DEFINE_MODULE(gtkMenuScheme, *)

FmContextMenuSchemeAddonInit fm_module_init_gtkMenuScheme = {
    _fm_file_actions_init,
    _fm_file_actions_finalize,
    _fm_actions_update_file_menu_for_scheme,
    _fm_actions_update_folder_menu_for_scheme
};
#endif /* HAVE_ACTIONS */
