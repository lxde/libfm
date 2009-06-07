/*
 *      main-win.c
 *      
 *      Copyright 2009 PCMan <pcman@thinkpad>
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

#include "main-win.h"
#include "fm-folder-view.h"
#include <glib/gi18n.h>

static void fm_main_win_finalize  			(GObject *object);
G_DEFINE_TYPE(FmMainWin, fm_main_win, GTK_TYPE_WINDOW);

static void on_new_win(GtkAction* act, FmMainWin* win);
static void on_close_win(GtkAction* act, FmMainWin* win);
static void on_change_mode(GtkAction* act, FmMainWin* win);
static void on_sort(GtkAction* act, FmMainWin* win);
static void on_about(GtkAction* act, FmMainWin* win);

static const char main_menu_xml[] = 
"<menubar>"
  "<menu name='File' action='FileMenu'>"
    "<menuitem name='New' action='New' />"
    "<menuitem name='Close' action='Close' />"
  "</menu>"
  "<menu name='View' action='ViewMenu'>"
    "<menuitem name='IconView' action='IconView' />"
    "<menuitem name='ListView' action='ListView' />"
	"<separator />"
	"<menu name='Sort' action='Sort'>"
	  "<menuitem name='ByName' action='ByName' />"
	  "<menuitem name='ByMTime' action='ByMTime' />"
	"</menu>"
  "</menu>"
  "<menu name='Help' action='HelpMenu'>"
    "<menuitem name='About' action='About' />"
  "</menu>"
"</menubar>";

static GtkActionEntry main_win_actions[]=
{
	{"FileMenu", NULL, N_("_File"), NULL, NULL, NULL},
		{"New", NULL, N_("_New Window"), "<CTRL>N", NULL, on_new_win},
		{"Close", NULL, N_("_Close Window"), "<Ctrl>W", NULL, on_close_win},
	{"ViewMenu", NULL, N_("_View"), NULL, NULL, NULL},
		{"IconView", NULL, N_("_Icon View"), NULL, NULL, on_change_mode},
		{"ListView", NULL, N_("Detailed _List View"), NULL, NULL, on_change_mode},
		{"Sort", NULL, N_("_Sort Files"), NULL, NULL, NULL},
			{"ByName", NULL, N_("By _Name"), NULL, NULL, on_sort},
			{"ByMTime", NULL, N_("By _Modification Time"), NULL, NULL, on_sort},
	{"HelpMenu", NULL, N_("_Help"), NULL, NULL, NULL},
		{"About", GTK_STOCK_ABOUT, NULL, NULL, NULL, on_about}
};

static guint n_wins = 0;

static void fm_main_win_class_init(FmMainWinClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = fm_main_win_finalize;

	fm_main_win_parent_class = (GtkWindowClass*)g_type_class_peek(GTK_TYPE_WINDOW);
}

static void on_entry_activate(GtkEntry* entry, FmMainWin* self)
{
	fm_folder_view_chdir(self->folder_view, gtk_entry_get_text(entry));
}

static void on_file_clicked(FmFolderView* fv, int btn, FmFileInfo* fi, FmMainWin* win)
{
	char* fpath, *uri;
	GAppLaunchContext* ctx;
	switch(btn)
	{
	case GDK_2BUTTON_PRESS: /* file activated */
		g_debug("file clicked: %s", fi->disp_name);
		fpath = g_build_filename(fv->cwd, fi->name, NULL);
		if(fm_file_info_is_dir(fi))
		{
			FmMainWin* win = fm_main_win_new();
			gtk_window_set_default_size(win, 640, 480);
			fm_folder_view_chdir(win->folder_view, fpath);
			gtk_window_present(win);
		}
		else
		{
			uri = g_filename_to_uri(fpath, NULL, NULL);
			ctx = gdk_app_launch_context_new();
			gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());
			g_app_info_launch_default_for_uri(uri, ctx, NULL);
			g_object_unref(ctx);

			g_free(uri);
		}
		g_free(fpath);
		break;
	case GDK_BUTTON_PRESS:
		break;
	}
}

static void fm_main_win_init(FmMainWin *self)
{
	GtkWidget* vbox, *menubar;
	GtkUIManager* ui;
	GtkActionGroup* act_grp;
	
	++n_wins;

	vbox = gtk_vbox_new(FALSE, 0);
	self->location = gtk_entry_new();
	g_signal_connect(self->location, "activate", on_entry_activate, self);
	self->folder_view = fm_folder_view_new( FM_FV_LIST_VIEW );
	fm_folder_view_set_selection_mode(self->folder_view, GTK_SELECTION_MULTIPLE);
	g_signal_connect(self->folder_view, "file-clicked", on_file_clicked, self);

	/* create menu bar */
	ui = gtk_ui_manager_new();
	gtk_ui_manager_add_ui_from_string(ui, main_menu_xml, -1, NULL);
	act_grp = gtk_action_group_new("MainWin");
	gtk_action_group_add_actions(act_grp, main_win_actions, G_N_ELEMENTS(main_win_actions), self);
	gtk_ui_manager_insert_action_group(ui, act_grp, 0);
	menubar = gtk_ui_manager_get_widget(ui, "/menubar");

	gtk_box_pack_start( (GtkBox*)vbox, menubar, FALSE, TRUE, 0 );
	gtk_box_pack_start( (GtkBox*)vbox, self->location, FALSE, TRUE, 0 );
	gtk_box_pack_start( (GtkBox*)vbox, self->folder_view, TRUE, TRUE, 0 );
	gtk_container_add( (GtkContainer*)self, vbox );
	gtk_widget_show_all(vbox);

	fm_folder_view_chdir(self->folder_view, g_get_home_dir());
}


GtkWidget* fm_main_win_new(void)
{
	return (GtkWidget*)g_object_new(FM_MAIN_WIN_TYPE, NULL);
}


static void fm_main_win_finalize(GObject *object)
{
	FmMainWin *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_MAIN_WIN(object));

	--n_wins;

	self = FM_MAIN_WIN(object);

	if (G_OBJECT_CLASS(fm_main_win_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_main_win_parent_class)->finalize)(object);

	if(n_wins == 0)
		gtk_main_quit();
}

void on_about(GtkAction* act, FmMainWin* win)
{
	const char* authors[]={"Hong Jen Yee <pcman.tw@gmail.com>", NULL};
	GtkWidget* dlg = gtk_about_dialog_new();
	gtk_about_dialog_set_program_name(dlg, "libfm-demo");
	gtk_about_dialog_set_authors(dlg, authors);
	gtk_about_dialog_set_comments(dlg, "A demo program for libfm");
	gtk_about_dialog_set_website(dlg, "http://libfm.sf.net/");
	gtk_dialog_run(dlg);
	gtk_widget_destroy(dlg);
}

void on_change_mode(GtkAction* act, FmMainWin* win)
{
	const char* name = gtk_action_get_name(act);
	if(strcmp(name, "IconView") == 0)
	{
		fm_folder_view_set_mode( win->folder_view, FM_FV_ICON_VIEW );
	}
	else if(strcmp(name, "ListView") == 0)
	{
		fm_folder_view_set_mode( win->folder_view, FM_FV_LIST_VIEW );
	}
}

void on_sort(GtkAction* act, FmMainWin* win)
{
	const char* name = gtk_action_get_name(act);
	if(strcmp(name, "ByName") == 0)
	{
	}
	else if(strcmp(name, "ByMTime") == 0)
	{
	}
}


void on_new_win(GtkAction* act, FmMainWin* win)
{
	win = fm_main_win_new();
	gtk_window_set_default_size(win, 640, 480);
	fm_folder_view_chdir(win->folder_view, g_get_home_dir());
	gtk_window_present(win);
}

void on_close_win(GtkAction* act, FmMainWin* win)
{
	gtk_widget_destroy(win);
}
