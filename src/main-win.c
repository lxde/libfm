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

#include <glib/gi18n.h>

#include "main-win.h"
#include "fm-folder-view.h"
#include "fm-path-entry.h"
#include "fm-file-menu.h"

static void fm_main_win_finalize  			(GObject *object);
G_DEFINE_TYPE(FmMainWin, fm_main_win, GTK_TYPE_WINDOW);

static void on_new_win(GtkAction* act, FmMainWin* win);
static void on_close_win(GtkAction* act, FmMainWin* win);
static void on_go(GtkAction* act, FmMainWin* win);
static void on_go_up(GtkAction* act, FmMainWin* win);
static void on_go_home(GtkAction* act, FmMainWin* win);
static void on_go_desktop(GtkAction* act, FmMainWin* win);
static void on_go_trash(GtkAction* act, FmMainWin* win);
static void on_go_computer(GtkAction* act, FmMainWin* win);
static void on_go_network(GtkAction* act, FmMainWin* win);
static void on_show_hidden(GtkToggleAction* act, FmMainWin* win);
static void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_about(GtkAction* act, FmMainWin* win);

static const char main_menu_xml[] = 
"<menubar>"
  "<menu action='FileMenu'>"
    "<menuitem action='New'/>"
    "<menuitem action='Close'/>"
  "</menu>"
  "<menu action='GoMenu'>"
    "<menuitem action='Prev'/>"
    "<menuitem action='Next'/>"
    "<menuitem action='Up'/>"
	"<separator/>"
    "<menuitem action='Home'/>"
    "<menuitem action='Desktop'/>"
    "<menuitem action='Computer'/>"
    "<menuitem action='Trash'/>"
    "<menuitem action='Network'/>"
  "</menu>"
  "<menu action='ViewMenu'>"
    "<menuitem action='ShowHidden'/>"
    "<separator/>"
    "<menuitem action='IconView'/>"
    "<menuitem action='CompactView'/>"
    "<menuitem action='ListView'/>"
	"<separator/>"
	"<menu action='Sort'>"
	  "<menuitem action='Desc'/>"
	  "<menuitem action='Asc'/>"
	  "<separator/>"
	  "<menuitem action='ByName'/>"
	  "<menuitem action='ByMTime'/>"
	"</menu>"
  "</menu>"
  "<menu action='HelpMenu'>"
    "<menuitem action='About'/>"
  "</menu>"
"</menubar>"
"<toolbar>"
	"<toolitem action='New'/>"
	"<toolitem action='Prev'/>"
	"<toolitem action='Next'/>"
	"<toolitem action='Up'/>"
	"<toolitem action='Home'/>"
	"<toolitem action='Go'/>"
"</toolbar>";

static GtkActionEntry main_win_actions[]=
{
	{"FileMenu", NULL, N_("_File"), NULL, NULL, NULL},
		{"New", GTK_STOCK_NEW, N_("_New Window"), "<CTRL>N", NULL, on_new_win},
		{"Close", GTK_STOCK_CLOSE, N_("_Close Window"), "<Ctrl>W", NULL, on_close_win},
	{"ViewMenu", NULL, N_("_View"), NULL, NULL, NULL},
		{"Sort", NULL, N_("_Sort Files"), NULL, NULL, NULL},
	{"HelpMenu", NULL, N_("_Help"), NULL, NULL, NULL},
		{"About", GTK_STOCK_ABOUT, NULL, NULL, NULL, on_about},
	{"GoMenu", NULL, N_("_Go"), NULL, NULL, NULL},
		{"Prev", GTK_STOCK_GO_BACK, N_("Previous Folder"), "<Alt>Right", N_("Previous Folder"), on_go},
		{"Next", GTK_STOCK_GO_FORWARD, N_("Next Folder"), "<Alt>Left", N_("Next Folder"), on_go},
		{"Up", GTK_STOCK_GO_UP, N_("Parent Folder"), "<Alt>Up", N_("Go to parent Folder"), on_go_up},
		{"Home", "user-home", N_("Home Folder"), "<Alt>Home", N_("Home Folder"), on_go_home},
		{"Desktop", "user-desktop", N_("Desktop"), NULL, N_("Desktop Folder"), on_go_desktop},
		{"Computer", "computer", N_("My Computer"), NULL, NULL, on_go_computer},
		{"Trash", "user-trash", N_("Trash Can"), NULL, NULL, on_go_trash},
		{"Network", GTK_STOCK_NETWORK, N_("Network Drives"), NULL, NULL, on_go_network},
		{"Go", GTK_STOCK_JUMP_TO, NULL, NULL, NULL, on_go}
};

static GtkActionEntry main_win_toggle_actions[]=
{
	{"ShowHidden", NULL, N_("Show _Hidden"), "<Ctrl>H", NULL, on_show_hidden, FALSE}
};

static GtkRadioActionEntry main_win_mode_actions[]=
{
	{"IconView", NULL, N_("_Icon View"), NULL, NULL, FM_FV_ICON_VIEW},
	{"CompactView", NULL, N_("_Compact View"), NULL, NULL, FM_FV_COMPACT_VIEW},
	{"ListView", NULL, N_("Detailed _List View"), NULL, NULL, FM_FV_LIST_VIEW},
};

static GtkRadioActionEntry main_win_sort_type_actions[]=
{
	{"Asc", GTK_STOCK_SORT_ASCENDING, NULL, NULL, NULL, GTK_SORT_ASCENDING},
	{"Desc", GTK_STOCK_SORT_DESCENDING, NULL, NULL, NULL, GTK_SORT_DESCENDING},
};

static GtkRadioActionEntry main_win_sort_by_actions[]=
{
	{"ByName", NULL, N_("By _Name"), NULL, NULL, 0},
	{"ByMTime", NULL, N_("By _Modification Time"), NULL, NULL, 1}
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

static void on_file_clicked(FmFolderView* fv, FmFileInfo* fi, int type, int btn, FmMainWin* win)
{
	char* fpath, *uri;
	GAppLaunchContext* ctx;
	switch(type)
	{
	case GDK_2BUTTON_PRESS: /* file activated */
		if( btn == 1 ) /* left click */
		{
			g_debug("file clicked: %s", fi->disp_name);
			fpath = g_build_filename(fv->cwd, fi->name, NULL);
			if(fm_file_info_is_dir(fi))
			{
				fm_main_win_chdir( win, fpath);
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
		}
		break;
	case GDK_BUTTON_PRESS:
		if( btn == 3 ) /* right click */
		{
			if(fi)
			{
				GtkWidget* popup;
				popup = fm_file_menu_new_for_file(fi);
				gtk_menu_popup(popup, NULL, NULL, NULL, fi, 3, gtk_get_current_event_time());
				g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);
			}
			else
			{
				
			}
		}
		else if( btn == 2) /* middle click */
		{
			g_debug("middle click!");
		}
		break;
	}
}

static void fm_main_win_init(FmMainWin *self)
{
	GtkWidget* vbox, *menubar, *toolitem;
	GtkUIManager* ui;
	GtkActionGroup* act_grp;
	GtkAccelGroup* accel_grp;

	++n_wins;

	vbox = gtk_vbox_new(FALSE, 0);
	self->location = fm_path_entry_new();
	g_signal_connect(self->location, "activate", on_entry_activate, self);

	self->folder_view = fm_folder_view_new( FM_FV_LIST_VIEW );
	fm_folder_view_set_selection_mode(self->folder_view, GTK_SELECTION_MULTIPLE);
	g_signal_connect(self->folder_view, "clicked", on_file_clicked, self);

	/* create menu bar and toolbar */
	ui = gtk_ui_manager_new();
	act_grp = gtk_action_group_new("Main");
	gtk_action_group_add_actions(act_grp, main_win_actions, G_N_ELEMENTS(main_win_actions), self);
	gtk_action_group_add_toggle_actions(act_grp, main_win_toggle_actions, G_N_ELEMENTS(main_win_toggle_actions), self);
	gtk_action_group_add_radio_actions(act_grp, main_win_mode_actions, G_N_ELEMENTS(main_win_mode_actions), FM_FV_ICON_VIEW, on_change_mode, self);
	gtk_action_group_add_radio_actions(act_grp, main_win_sort_type_actions, G_N_ELEMENTS(main_win_sort_type_actions), GTK_SORT_DESCENDING, on_sort_type, self);
	gtk_action_group_add_radio_actions(act_grp, main_win_sort_by_actions, G_N_ELEMENTS(main_win_sort_by_actions), 0, on_sort_by, self);
	gtk_ui_manager_insert_action_group(ui, act_grp, 0);

	gtk_ui_manager_add_ui_from_string(ui, main_menu_xml, -1, NULL);
	menubar = gtk_ui_manager_get_widget(ui, "/menubar");
	self->toolbar = gtk_ui_manager_get_widget(ui, "/toolbar");
	accel_grp = gtk_ui_manager_get_accel_group(ui);
	gtk_window_add_accel_group(self, accel_grp);
	gtk_box_pack_start( (GtkBox*)vbox, menubar, FALSE, TRUE, 0 );
	gtk_box_pack_start( (GtkBox*)vbox, self->toolbar, FALSE, TRUE, 0 );

	toolitem = gtk_tool_item_new();
	gtk_container_add( toolitem, self->location );
	gtk_tool_item_set_expand(toolitem, TRUE);
	gtk_toolbar_insert((GtkToolbar*)self->toolbar, toolitem, gtk_toolbar_get_n_items(self->toolbar) - 1 );

	gtk_box_pack_start( (GtkBox*)vbox, self->folder_view, TRUE, TRUE, 0 );

	self->statusbar = gtk_statusbar_new();
	gtk_box_pack_start( (GtkBox*)vbox, self->statusbar, FALSE, TRUE, 0 );

	g_object_unref(act_grp);
	g_object_unref(ui);

	gtk_container_add( (GtkContainer*)self, vbox );
	gtk_widget_show_all(vbox);

	fm_folder_view_set_show_hidden(self->folder_view, FALSE);
	fm_main_win_chdir(self, g_get_home_dir());
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

void on_show_hidden(GtkToggleAction* act, FmMainWin* win)
{
	gboolean active = gtk_toggle_action_get_active(act);
	fm_folder_view_set_show_hidden( win->folder_view, active );
}

void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
	int mode = gtk_radio_action_get_current_value(cur);
	fm_folder_view_set_mode( win->folder_view, mode );
}

void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
	int val = gtk_radio_action_get_current_value(cur);
//	fm_folder_view_set_sort_by(win->folder_view, val);
}

void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
	int val = gtk_radio_action_get_current_value(cur);
//	fm_folder_view_set_sort_type(win->folder_view, val);
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

void on_go(GtkAction* act, FmMainWin* win)
{
	fm_main_win_chdir( win, gtk_entry_get_text(win->location));
}

void on_go_up(GtkAction* act, FmMainWin* win)
{
	char* parent = g_path_get_dirname(fm_folder_view_get_cwd(win->folder_view));
	if(parent)
	{
		fm_main_win_chdir( win, parent);
		g_free(parent);
	}
}

void on_go_home(GtkAction* act, FmMainWin* win)
{
	fm_main_win_chdir( win, g_get_home_dir());
}

void on_go_desktop(GtkAction* act, FmMainWin* win)
{
	fm_main_win_chdir( win, g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));
}

void on_go_trash(GtkAction* act, FmMainWin* win)
{
	fm_main_win_chdir( win, "trash:/");
}

void on_go_computer(GtkAction* act, FmMainWin* win)
{
	fm_main_win_chdir( win, "computer:/");
}

void on_go_network(GtkAction* act, FmMainWin* win)
{
	fm_main_win_chdir( win, "network:/");
}

void fm_main_win_chdir(FmMainWin* win, const char* path)
{
	gtk_entry_set_text(win->location, path);
	fm_folder_view_chdir(win->folder_view, path);
}
