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

#include <glib/gi18n.h>
#include "fm-file-menu.h"

static void on_open(GtkAction* action, gpointer user_data);

const char base_menu_xml[]=
"<popup>"
  "<menuitem action='Open'/>"
  "<separator/>"
  "<placeholder name='ph1'/>"
  "<separator/>"
  "<menuitem action='Cut'/>"
  "<menuitem action='Copy'/>"
  "<menuitem action='Paste'/>"
//  "<menuitem action='SendTo'/>"
  "<separator/>"
  "<placeholder name='ph2'/>"
  "<separator/>"
  "<menuitem action='Prop'/>"
"</popup>";

const char folder_menu_xml[]=
"<popup>"
  "<placeholder name='ph1'>"
//    "<menuitem action='NewTab'/>"
    "<menuitem action='NewWin'/>"
    "<separator/>"
//    "<menuitem action='Search'/>"
  "</placeholder>"
"</popup>";


GtkActionEntry base_menu_actions[]=
{
	{"Open", GTK_STOCK_OPEN, NULL, NULL, NULL, on_open},
	{"Cut", GTK_STOCK_CUT, NULL, NULL, NULL, on_open},
	{"Copy", GTK_STOCK_COPY, NULL, NULL, NULL, on_open},
	{"Paste", GTK_STOCK_PASTE, NULL, NULL, NULL, on_open},
	{"Prop", GTK_STOCK_PROPERTIES, NULL, NULL, NULL, on_open}
};

GtkActionEntry folder_menu_actions[]=
{
	{"NewTab", GTK_STOCK_NEW, N_("Open in New Tab"), NULL, NULL, on_open},
	{"NewWin", GTK_STOCK_NEW, N_("Open in New Window"), NULL, NULL, on_open},
	{"Search", GTK_STOCK_FIND, NULL, NULL, NULL, on_open}
};


GtkWidget* fm_file_menu_new_for_file(FmFileInfo* fi)
{
	GtkWidget* menu;
	GList* files = g_list_prepend(NULL, fm_file_info_ref(fi));
	menu = fm_file_menu_new_for_files(files);
	fm_file_info_unref(fi);
	g_list_free(files);
	return menu;
}

GtkWidget* fm_file_menu_new_for_files(GList* files)
{
	GtkWidget* menu;
	GtkUIManager* ui;
	GtkActionGroup* act_grp;
	GtkAccelGroup* accel_grp;

	ui = gtk_ui_manager_new();
	act_grp = gtk_action_group_new("Main");
	gtk_action_group_add_actions(act_grp, base_menu_actions, G_N_ELEMENTS(base_menu_actions), NULL);
	gtk_ui_manager_add_ui_from_string(ui, base_menu_xml, -1, NULL);
	gtk_ui_manager_insert_action_group(ui, act_grp, 0);

	/* if the files are of the same time */
	FmFileInfo* fi = (FmFileInfo*)files->data;
	
	if(fm_file_info_is_dir(fi))
	{
		gtk_action_group_add_actions(act_grp, folder_menu_actions, G_N_ELEMENTS(folder_menu_actions), NULL);
		gtk_ui_manager_add_ui_from_string(ui, folder_menu_xml, -1, NULL);
	}

	if(fi->type)
	{
		GtkAction* act;
		GList* apps = g_app_info_get_all_for_type(fi->type->type);
		GList* l;
		GString *xml = g_string_new("<popup><placeholder name='ph1'>");
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

	menu = gtk_ui_manager_get_widget(ui, "/popup");
	g_object_ref(menu);

	g_object_unref(act_grp);
	g_object_unref(ui);
	return menu;
}

void on_open(GtkAction* action, gpointer user_data)
{
	
}

