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

#include <glib/gi18n.h>
#include "fm-file-menu.h"
#include "fm-path.h"

#include "fm-file-properties.h"

static void on_open(GtkAction* action, gpointer user_data);
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
  "<separator/>"
  "<menuitem action='Rename'/>"
  "<menuitem action='Link'/>"
  "<menu action='SendTo'>"
  "</menu>"
  "<separator/>"
  "<placeholder name='ph3'/>"
  "<separator/>"
  "<menuitem action='Prop'/>"
"</popup>";


/* FIXME: how to show accel keys in the popup menu? */
GtkActionEntry base_menu_actions[]=
{
	{"Open", GTK_STOCK_OPEN, NULL, NULL, NULL, on_open},
	{"Cut", GTK_STOCK_CUT, NULL, "<Ctrl>X", NULL, on_open},
	{"Copy", GTK_STOCK_COPY, NULL, "<Ctrl>C", NULL, on_open},
	{"Paste", GTK_STOCK_PASTE, NULL, "<Ctrl>V", NULL, on_open},
	{"Rename", NULL, N_("Rename"), "F2", NULL, NULL},
	{"Link", NULL, N_("Create Symlink"), NULL, NULL, NULL},
	{"SendTo", NULL, N_("Send To"), NULL, NULL, NULL},
	{"Prop", GTK_STOCK_PROPERTIES, NULL, NULL, NULL, on_prop}
};


void fm_file_menu_destroy(FmFileMenu* menu)
{
	if(menu->menu)
		gtk_widget_destroy(menu->menu);

	if(menu->file_infos)
		fm_list_unref(menu->file_infos);

	g_object_unref(menu->act_grp);
	g_object_unref(menu->ui);
	g_slice_free(FmFileMenu, menu);
}

FmFileMenu* fm_file_menu_new_for_file(FmFileInfo* fi, gboolean auto_destroy)
{
	FmFileMenu* menu;
	FmFileInfoList* files = fm_file_info_list_new();
	fm_list_push_tail(files, fi);
	menu = fm_file_menu_new_for_files(files, auto_destroy);
	fm_list_unref(files);
	return menu;
}

FmFileMenu* fm_file_menu_new_for_files(FmFileInfoList* files, gboolean auto_destroy)
{
	GtkWidget* menu;
	GtkUIManager* ui;
	GtkActionGroup* act_grp;
	GtkAccelGroup* accel_grp;
	FmFileInfo* fi;
	FmFileMenu* data = g_slice_new0(FmFileMenu);

	data->auto_destroy = auto_destroy;
	data->ui = ui = gtk_ui_manager_new();
	data->act_grp = act_grp = gtk_action_group_new("Popup");

	data->file_infos = fm_list_ref(files);

	gtk_action_group_add_actions(act_grp, base_menu_actions, G_N_ELEMENTS(base_menu_actions), data);
	gtk_ui_manager_add_ui_from_string(ui, base_menu_xml, -1, NULL);
	gtk_ui_manager_insert_action_group(ui, act_grp, 0);

	/* check if the files are of the same type */
	data->same_type = fm_file_info_list_is_same_type(files);

	if(data->same_type) /* add specific menu items for this mime type */
	{
		fi = (FmFileInfo*)fm_list_peek_head(files);
		if(fi->type)
		{
			GtkAction* act;
			GList* apps = g_app_info_get_all_for_type(fi->type->type);
			GList* l;
			GString *xml = g_string_new("<popup><placeholder name='ph2'>");
			for(l=apps;l;l=l->next)
			{
				GAppInfo* app = l->data;
				act = gtk_action_new(g_app_info_get_id(app), 
							g_app_info_get_name(app),
							g_app_info_get_description(app), 
							NULL);
				gtk_action_set_gicon(act, g_app_info_get_icon(app));
				gtk_action_group_add_action(act_grp, act);
				g_string_append_printf(xml, "<menuitem action='%s'/>", g_app_info_get_id(app));
				g_object_unref(app);
			}
			g_list_free(apps);
			g_string_append(xml, "</placeholder></popup>");
			gtk_ui_manager_add_ui_from_string(ui, xml->str, xml->len, NULL);
			g_string_free(xml, TRUE);
		}
	}
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

