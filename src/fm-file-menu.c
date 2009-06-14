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
#include "fm-utils.h"
#include "fm-deep-count-job.h"


static void on_open(GtkAction* action, gpointer user_data);
static void on_prop(GtkAction* action, gpointer user_data);

const char base_menu_xml[]=
"<popup>"
  "<menuitem action='Open'/>"
  "<separator/>"
  "<placeholder name='ph1'/>"
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

static gboolean on_timeout(FmDeepCountJob* dc)
{
	char size_str[128];
	GtkLabel* label = g_object_get_data(dc, "total_size");
	g_debug("total_size: %p", label);
	fm_file_size_to_str(size_str, dc->total_size, FALSE);
	gtk_label_set_text(label, size_str);
	return TRUE;
}

static void on_finished(FmDeepCountJob* job, GtkLabel* label)
{
	g_debug("Finished!");
}

void on_prop(GtkAction* action, gpointer user_data)
{
	GtkBuilder* builder=gtk_builder_new();
	GtkWidget* dlg, *total_size;
	guint timeout;
	GFile* gf = g_file_new_for_path(/*g_get_home_dir()*/ "/usr/share");
	FmJob* job = fm_deep_count_job_new(gf);
	g_object_unref(gf);

	gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/file-prop.ui", NULL);
	dlg = (GtkWidget*)gtk_builder_get_object(builder, "dlg");
	total_size = (GtkWidget*)gtk_builder_get_object(builder, "total_size");
	gtk_widget_show(dlg);
	g_object_unref(builder);

	g_object_set_data(job, "total_size", total_size);
	timeout = g_timeout_add(500, (GSourceFunc)on_timeout, g_object_ref(job));
	g_signal_connect_swapped(dlg, "delete-event", g_source_remove, timeout);
	g_signal_connect(job, "finished", on_finished, total_size);

	fm_job_run(job);
}
