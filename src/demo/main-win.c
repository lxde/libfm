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
#include "fm-folder-model.h"
#include "fm-path-entry.h"
#include "fm-file-menu.h"
#include "fm-clipboard.h"
#include "fm-file-ops.h"

static void fm_main_win_finalize              (GObject *object);
G_DEFINE_TYPE(FmMainWin, fm_main_win, GTK_TYPE_WINDOW);

static void on_new_win(GtkAction* act, FmMainWin* win);
static void on_close_win(GtkAction* act, FmMainWin* win);

static void on_cut(GtkAction* act, FmMainWin* win);
static void on_copy(GtkAction* act, FmMainWin* win);
static void on_copy_to(GtkAction* act, FmMainWin* win);
static void on_move_to(GtkAction* act, FmMainWin* win);
static void on_paste(GtkAction* act, FmMainWin* win);
static void on_del(GtkAction* act, FmMainWin* win);

static void on_select_all(GtkAction* act, FmMainWin* win);
static void on_invert_select(GtkAction* act, FmMainWin* win);

static void on_go(GtkAction* act, FmMainWin* win);
static void on_go_up(GtkAction* act, FmMainWin* win);
static void on_go_home(GtkAction* act, FmMainWin* win);
static void on_go_desktop(GtkAction* act, FmMainWin* win);
static void on_go_trash(GtkAction* act, FmMainWin* win);
static void on_go_computer(GtkAction* act, FmMainWin* win);
static void on_go_network(GtkAction* act, FmMainWin* win);
static void on_go_apps(GtkAction* act, FmMainWin* win);
static void on_show_hidden(GtkToggleAction* act, FmMainWin* win);
static void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_about(GtkAction* act, FmMainWin* win);

static void on_location(GtkAction* act, FmMainWin* win);

static const char main_menu_xml[] = 
"<accelerator action='Location'/>"
"<menubar>"
  "<menu action='FileMenu'>"
    "<menuitem action='New'/>"
    "<menuitem action='Close'/>"
  "</menu>"
  "<menu action='EditMenu'>"
    "<menuitem action='Cut'/>"
    "<menuitem action='Copy'/>"
    "<menuitem action='Paste'/>"
    "<menuitem action='Del'/>"
    "<separator/>"
    "<menuitem action='Rename'/>"
    "<menuitem action='Link'/>"
    "<menuitem action='MoveTo'/>"
    "<menuitem action='CopyTo'/>"
    "<separator/>"
    "<menuitem action='SelAll'/>"
    "<menuitem action='InvSel'/>"
    "<separator/>"
    "<menuitem action='Pref'/>"
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
    "<menuitem action='Apps'/>"
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
        {"New", GTK_STOCK_NEW, N_("_New Window"), "<Ctrl>N", NULL, on_new_win},
        {"Close", GTK_STOCK_CLOSE, N_("_Close Window"), "<Ctrl>W", NULL, on_close_win},
    {"EditMenu", NULL, N_("_Edit"), NULL, NULL, NULL},
        {"Cut", GTK_STOCK_CUT, NULL, NULL, NULL, on_cut},
        {"Copy", GTK_STOCK_COPY, NULL, NULL, NULL, on_copy},
        {"Paste", GTK_STOCK_PASTE, NULL, NULL, NULL, on_paste},
        {"Del", GTK_STOCK_DELETE, NULL, NULL, NULL, on_del},
        {"Rename", NULL, N_("Rename"), "F2", NULL, NULL},
        {"Link", NULL, N_("Create Symlink"), NULL, NULL, NULL},
        {"MoveTo", NULL, N_("Move To..."), NULL, NULL, on_move_to},
        {"CopyTo", NULL, N_("Copy To..."), NULL, NULL, on_copy_to},
        {"SelAll", GTK_STOCK_SELECT_ALL, NULL, NULL, NULL, on_select_all},
        {"InvSel", NULL, N_("Invert Selection"), NULL, NULL, on_invert_select},
        {"Pref", GTK_STOCK_PREFERENCES, NULL, NULL, NULL, NULL},
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
        {"Apps", "system-software-install", N_("Applications"), NULL, N_("Installed Applications"), on_go_apps},
        {"Go", GTK_STOCK_JUMP_TO, NULL, NULL, NULL, on_go},
    /* FIXME: why this accelerator key doesn't work? */
    {"Location", NULL, "Location", "<Alt>d", NULL, on_location}
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
    {"ByName", NULL, N_("By _Name"), NULL, NULL, COL_FILE_NAME},
    {"ByMTime", NULL, N_("By _Modification Time"), NULL, NULL, COL_FILE_MTIME}
};


const char folder_menu_xml[]=
"<popup>"
  "<placeholder name='ph1'>"
    "<menuitem action='NewTab'/>"
    "<menuitem action='NewWin'/>"
    "<menuitem action='Search'/>"
  "</placeholder>"
"</popup>";

/* Action entries for pupup menus */
static GtkActionEntry folder_menu_actions[]=
{
    {"NewTab", GTK_STOCK_NEW, N_("Open in New Tab"), NULL, NULL, NULL},
    {"NewWin", GTK_STOCK_NEW, N_("Open in New Window"), NULL, NULL, NULL},
    {"Search", GTK_STOCK_FIND, NULL, NULL, NULL, NULL}
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
    fm_folder_view_chdir_by_name(self->folder_view, gtk_entry_get_text(entry));
}

static void on_file_clicked(FmFolderView* fv, FmFolderViewClickType type, FmFileInfo* fi, FmMainWin* win)
{
    char* fpath, *uri;
    GAppLaunchContext* ctx;
    switch(type)
    {
    case FM_FV_ACTIVATED: /* file activated */
        if(fm_file_info_is_dir(fi))
        {
            fm_main_win_chdir( win, fi->path);
        }
        else if(fi->target) /* FIXME: use accessor functions. */
        {
			/* FIXME: use FmPath here. */
            fm_main_win_chdir_by_name( win, fi->target);
        }
        else
        {
            uri = fm_path_to_uri(fi->path);
            ctx = gdk_app_launch_context_new();
            gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());
            g_app_info_launch_default_for_uri(uri, ctx, NULL);
            g_object_unref(ctx);
            g_free(uri);
        }
        break;
    case FM_FV_CONTEXT_MENU:
        if(fi)
        {
            FmFileMenu* menu;
            GtkMenu* popup;
            FmFileInfoList* files = fm_folder_view_get_selected_files(fv);
            menu = fm_file_menu_new_for_files(files, TRUE);
            fm_list_unref(files);

            /* merge some specific menu items for folders */
            if(fm_file_menu_is_single_file_type(menu) && fm_file_info_is_dir(fi))
            {
                GtkUIManager* ui = fm_file_menu_get_ui(menu);
                GtkActionGroup* act_grp = fm_file_menu_get_action_group(menu);
                gtk_action_group_add_actions(act_grp, folder_menu_actions, G_N_ELEMENTS(folder_menu_actions), NULL);
                gtk_ui_manager_add_ui_from_string(ui, folder_menu_xml, -1, NULL);
            }

            popup = fm_file_menu_get_menu(menu);
            gtk_menu_popup(popup, NULL, NULL, NULL, fi, 3, gtk_get_current_event_time());
        }
        else /* no files are selected. Show context menu of current folder. */
        {

        }
        break;
    case FM_FV_MIDDLE_CLICK:
        g_debug("middle click!");
        break;
    }
}

static void on_sel_changed(FmFolderView* fv, FmFileInfoList* files, FmMainWin* win)
{
	/* popup previous message if there is any */
	gtk_statusbar_pop(win->statusbar, win->statusbar_ctx2);
    if(files)
    {
        char* msg;
        /* FIXME: display total size of all selected files. */
        if(fm_list_get_length(files) == 1) /* only one file is selected */
        {
            FmFileInfo* fi = fm_list_peek_head(files);
            const char* size_str = fm_file_info_get_disp_size(fi);
            if(size_str)
            {
                msg = g_strdup_printf("\"%s\" (%s) %s", 
                            fm_file_info_get_disp_name(fi),
                            size_str ? size_str : "",
                            fm_file_info_get_desc(fi));
            }
            else
            {
                msg = g_strdup_printf("\"%s\" %s", 
                            fm_file_info_get_disp_name(fi),
                            fm_file_info_get_desc(fi));
            }
        }
        else
            msg = g_strdup_printf("%d items selected", fm_list_get_length(files));
        gtk_statusbar_push(win->statusbar, win->statusbar_ctx2, msg);
        g_free(msg);
    }
}

static void on_status(FmFolderView* fv, const char* msg, FmMainWin* win)
{
    gtk_statusbar_pop(win->statusbar, win->statusbar_ctx);
    gtk_statusbar_push(win->statusbar, win->statusbar_ctx, msg);
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

    self->folder_view = fm_folder_view_new( FM_FV_ICON_VIEW );
    fm_folder_view_set_show_hidden(self->folder_view, FALSE);
    fm_folder_view_sort(self->folder_view, GTK_SORT_DESCENDING, COL_FILE_NAME);
    fm_folder_view_set_selection_mode(self->folder_view, GTK_SELECTION_MULTIPLE);
    g_signal_connect(self->folder_view, "clicked", on_file_clicked, self);
    g_signal_connect(self->folder_view, "status", on_status, self);
    g_signal_connect(self->folder_view, "sel-changed", on_sel_changed, self);

    /* create menu bar and toolbar */
    ui = gtk_ui_manager_new();
    act_grp = gtk_action_group_new("Main");
    gtk_action_group_add_actions(act_grp, main_win_actions, G_N_ELEMENTS(main_win_actions), self);
    gtk_action_group_add_toggle_actions(act_grp, main_win_toggle_actions, G_N_ELEMENTS(main_win_toggle_actions), self);
    gtk_action_group_add_radio_actions(act_grp, main_win_mode_actions, G_N_ELEMENTS(main_win_mode_actions), FM_FV_ICON_VIEW, on_change_mode, self);
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_type_actions, G_N_ELEMENTS(main_win_sort_type_actions), GTK_SORT_ASCENDING, on_sort_type, self);
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_by_actions, G_N_ELEMENTS(main_win_sort_by_actions), 0, on_sort_by, self);
    gtk_ui_manager_insert_action_group(ui, act_grp, 0);

    gtk_ui_manager_add_ui_from_string(ui, main_menu_xml, -1, NULL);
    menubar = gtk_ui_manager_get_widget(ui, "/menubar");
    self->toolbar = gtk_ui_manager_get_widget(ui, "/toolbar");
    accel_grp = gtk_ui_manager_get_accel_group(ui);
    gtk_window_add_accel_group(self, accel_grp);
    gtk_box_pack_start( (GtkBox*)vbox, menubar, FALSE, TRUE, 0 );
    gtk_box_pack_start( (GtkBox*)vbox, self->toolbar, FALSE, TRUE, 0 );

    /* the location bar */
    toolitem = gtk_tool_item_new();
    gtk_container_add( toolitem, self->location );
    gtk_tool_item_set_expand(toolitem, TRUE);
    gtk_toolbar_insert((GtkToolbar*)self->toolbar, toolitem, gtk_toolbar_get_n_items(self->toolbar) - 1 );

    /* folder view */
    gtk_box_pack_start( (GtkBox*)vbox, self->folder_view, TRUE, TRUE, 0 );

    /* status bar */
    self->statusbar = gtk_statusbar_new();
    gtk_box_pack_start( (GtkBox*)vbox, self->statusbar, FALSE, TRUE, 0 );
    self->statusbar_ctx = gtk_statusbar_get_context_id(self->statusbar, "status");
    self->statusbar_ctx2 = gtk_statusbar_get_context_id(self->statusbar, "status2");

    g_object_unref(act_grp);
    g_object_unref(ui);

    gtk_container_add( (GtkContainer*)self, vbox );
    gtk_widget_show_all(vbox);

    fm_folder_view_set_show_hidden(self->folder_view, FALSE);
    fm_main_win_chdir(self, fm_path_get_home());

    gtk_widget_grab_focus(self->folder_view);
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
    fm_folder_view_sort(win->folder_view, -1, val);
}

void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int val = gtk_radio_action_get_current_value(cur);
    fm_folder_view_sort(win->folder_view, val, -1);
}

void on_new_win(GtkAction* act, FmMainWin* win)
{
    win = fm_main_win_new();
    gtk_window_set_default_size(win, 640, 480);
    fm_folder_view_chdir(win->folder_view, fm_path_get_home());
    gtk_window_present(win);
}

void on_close_win(GtkAction* act, FmMainWin* win)
{
    gtk_widget_destroy(win);
}

void on_go(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, gtk_entry_get_text(win->location));
}

void on_go_up(GtkAction* act, FmMainWin* win)
{
    FmPath* parent = fm_path_get_parent(fm_folder_view_get_cwd(win->folder_view));
    if(parent)
    {
        fm_main_win_chdir( win, parent);
        fm_path_unref(parent);
    }
}

void on_go_home(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, g_get_home_dir());
}

void on_go_desktop(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));
}

void on_go_trash(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "trash:///");
}

void on_go_computer(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "computer:///");
}

void on_go_network(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "network:///");
}

void on_go_apps(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "applications:///");
}

void fm_main_win_chdir_by_name(FmMainWin* win, const char* path_str)
{
    FmPath* path;
    gtk_entry_set_text(win->location, path_str);
    path = fm_path_new(path_str);
    fm_folder_view_chdir(win->folder_view, path);
    fm_path_unref(path);
}

void fm_main_win_chdir(FmMainWin* win, FmPath* path)
{
    char* path_str = fm_path_to_str(path);
    gtk_entry_set_text(win->location, path_str);
    g_free(path_str);
    fm_folder_view_chdir(win->folder_view, path);
}

void on_cut(GtkAction* act, FmMainWin* win)
{
    GtkWidget* focus = gtk_window_get_focus((GtkWindow*)win);
    if(GTK_IS_EDITABLE(focus) &&
       gtk_editable_get_selection_bounds((GtkEditable*)focus, NULL, NULL) )
    {
        gtk_editable_cut_clipboard((GtkEditable*)focus);
    }
    else
    {
        FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
        if(files)
        {
            fm_clipboard_cut_files(win, files);
            fm_list_unref(files);
        }
    }
}

void on_copy(GtkAction* act, FmMainWin* win)
{
    GtkWidget* focus = gtk_window_get_focus((GtkWindow*)win);
    if(GTK_IS_EDITABLE(focus) &&
       gtk_editable_get_selection_bounds((GtkEditable*)focus, NULL, NULL) )
    {
        gtk_editable_copy_clipboard((GtkEditable*)focus);
    }
    else
    {
        FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
        if(files)
        {
            fm_clipboard_copy_files(win, files);
            fm_list_unref(files);
        }
    }
}

void on_copy_to(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
    if(files)
    {
        fm_copy_files_to(files);
        fm_list_unref(files);
    }
}

void on_move_to(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
    if(files)
    {
        fm_move_files_to(files);
        fm_list_unref(files);
    }
}

void on_paste(GtkAction* act, FmMainWin* win)
{
    GtkWidget* focus = gtk_window_get_focus((GtkWindow*)win);
    if(GTK_IS_EDITABLE(focus) )
    {
        gtk_editable_paste_clipboard((GtkEditable*)focus);
    }
    else
    {
        FmPath* path = fm_folder_view_get_cwd(win->folder_view);
        fm_clipboard_paste_files(win->folder_view, path);
    }
}

void on_del(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
    if( !fm_list_is_empty(files) )
        fm_delete_files(files);
    fm_list_unref(files);
}

void on_select_all(GtkAction* act, FmMainWin* win)
{
    fm_folder_view_select_all(win->folder_view);
}

void on_invert_select(GtkAction* act, FmMainWin* win)
{
    fm_folder_view_select_invert(win->folder_view);
}

void on_location(GtkAction* act, FmMainWin* win)
{
    g_debug("XXXX");
    gtk_widget_grab_focus(win->location);
}
