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
#include "fm-path-list.h"

#include "fm-file-properties.h"

typedef struct _FmFileMenuData FmFileMenuData;
struct _FmFileMenuData
{
	GList* file_infos;
	gboolean same_type;
	GtkUIManager* ui;
	GtkActionGroup* act_grp;
};

static GQuark data_id = 0;
#define	get_data(menu)	(FmFileMenuData*)g_object_get_qdata(menu, data_id);

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


void fm_file_menu_data_free(FmFileMenuData* data)
{
	g_list_foreach(data->file_infos, (GFunc)fm_file_info_unref, NULL);
	g_list_free(data->file_infos);

	g_object_unref(data->act_grp);
	g_object_unref(data->ui);
	g_slice_free(FmFileMenuData, data);
}

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
	FmFileInfo* fi;
	FmFileMenuData* data = g_slice_new0(FmFileMenuData);

	data->ui = ui = gtk_ui_manager_new();
	data->act_grp = act_grp = gtk_action_group_new("Popup");

	/* deep copy */
	data->file_infos = g_list_copy(files);
	g_list_foreach(data->file_infos, (GFunc)fm_file_info_ref, NULL);

	gtk_action_group_add_actions(act_grp, base_menu_actions, G_N_ELEMENTS(base_menu_actions), data);
	gtk_ui_manager_add_ui_from_string(ui, base_menu_xml, -1, NULL);
	gtk_ui_manager_insert_action_group(ui, act_grp, 0);

	/* FIXME: check if the files are of the same type */
	data->same_type = TRUE;

	fi = (FmFileInfo*)files->data;
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

	menu = gtk_ui_manager_get_widget(ui, "/popup");
	if( G_UNLIKELY( 0 == data_id ) )
		data_id = g_quark_from_static_string("FmFileMenuData");
	g_object_set_qdata(menu, data_id, data);
	/* destroy notify of g_object_set_qdata_full doesn't work here since
	 * GtkUIManager holds reference to the menu, and when gtk_widget_destroy 
	 * is called, ref_count won't be zero and hence the data cannot be freed. */
	g_signal_connect_swapped(menu, "destroy", G_CALLBACK(fm_file_menu_data_free), data);
	return menu;
}

GtkUIManager* fm_file_menu_get_ui(GtkWidget* menu)
{
	FmFileMenuData* data = get_data(menu);
	return data->ui;
}

GtkActionGroup* fm_file_menu_get_action_group(GtkWidget* menu)
{
	FmFileMenuData* data = get_data(menu);
	return data->act_grp;
}

GList* fm_file_menu_get_file_info_list(GtkWidget* menu)
{
	FmFileMenuData* data = get_data(menu);
	return data->file_infos;
}

void on_open(GtkAction* action, gpointer user_data)
{

}

void on_prop(GtkAction* action, gpointer user_data)
{
	FmFileMenuData* data = (FmFileMenuData*)user_data;
	FmPathList* pl = fm_path_list_new_from_file_info_list(data->file_infos);
	fm_show_file_properties(pl);
	fm_path_list_unref(pl);
}

