/*
 *      main-win.c
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

#include "main-win.h"
#include "fm-places-view.h"
#include "fm-folder-view.h"
#include "fm-folder-model.h"
#include "fm-path-entry.h"
#include "fm-file-menu.h"
#include "fm-clipboard.h"
#include "fm-gtk-utils.h"
#include "fm-file-properties.h"

static void fm_main_win_finalize              (GObject *object);
G_DEFINE_TYPE(FmMainWin, fm_main_win, GTK_TYPE_WINDOW);

/* static void on_new_tab(GtkAction* act, FmMainWin* win); */
static void on_new_win(GtkAction* act, FmMainWin* win);
static void on_close_win(GtkAction* act, FmMainWin* win);

/* static void on_open_in_new_tab(GtkAction* act, FmMainWin* win); */
static void on_open_in_new_win(GtkAction* act, FmMainWin* win);

static void on_cut(GtkAction* act, FmMainWin* win);
static void on_copy(GtkAction* act, FmMainWin* win);
static void on_copy_to(GtkAction* act, FmMainWin* win);
static void on_move_to(GtkAction* act, FmMainWin* win);
static void on_paste(GtkAction* act, FmMainWin* win);
static void on_del(GtkAction* act, FmMainWin* win);
static void on_rename(GtkAction* act, FmMainWin* win);

static void on_select_all(GtkAction* act, FmMainWin* win);
static void on_invert_select(GtkAction* act, FmMainWin* win);

static void on_go(GtkAction* act, FmMainWin* win);
static void on_go_back(GtkAction* act, FmMainWin* win);
static void on_go_forward(GtkAction* act, FmMainWin* win);
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

static void on_create_new(GtkAction* action, FmMainWin* win);
static void on_prop(GtkAction* action, FmMainWin* win);

#include "main-win-ui.c" /* ui xml definitions and actions */

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
    fm_folder_view_chdir_by_name(FM_FOLDER_VIEW(self->folder_view), gtk_entry_get_text(entry));
}

static void on_view_loaded( FmFolderView* view, FmPath* path, gpointer user_data)
{
    const FmNavHistoryItem* item;
    FmMainWin* win = (FmMainWin*)user_data;
    FmPathEntry* entry = FM_PATH_ENTRY(win->location);
    fm_path_entry_set_model( entry, path, view->model );

    /* scroll to recorded position */
    item = fm_nav_history_get_cur(win->nav_history);
    gtk_adjustment_set_value( gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(view)), item->scroll_pos);
}

static gboolean open_folder_func(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    FmMainWin* win = FM_MAIN_WIN(user_data);
    GList* l = folder_infos;
    FmFileInfo* fi = (FmFileInfo*)l->data;
    fm_main_win_chdir(win, fi->path);
    l=l->next;
    for(; l; l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        /* FIXME: open in new window */
    }
    return TRUE;
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
            fm_launch_file_simple(GTK_WINDOW(win), NULL, fi, open_folder_func, win);
        }
        break;
    case FM_FV_CONTEXT_MENU:
        if(fi)
        {
            FmFileMenu* menu;
            GtkMenu* popup;
            FmFileInfoList* files = fm_folder_view_get_selected_files(fv);
            menu = fm_file_menu_new_for_files(files, fm_folder_view_get_cwd(fv), TRUE);
            fm_file_menu_set_folder_func(menu, open_folder_func, win);
            fm_list_unref(files);

            /* merge some specific menu items for folders */
            if(fm_file_menu_is_single_file_type(menu) && fm_file_info_is_dir(fi))
            {
                GtkUIManager* ui = fm_file_menu_get_ui(menu);
                GtkActionGroup* act_grp = fm_file_menu_get_action_group(menu);
                gtk_action_group_add_actions(act_grp, folder_menu_actions, G_N_ELEMENTS(folder_menu_actions), win);
                gtk_ui_manager_add_ui_from_string(ui, folder_menu_xml, -1, NULL);
            }

            popup = fm_file_menu_get_menu(menu);
            gtk_menu_popup(popup, NULL, NULL, NULL, fi, 3, gtk_get_current_event_time());
        }
        else /* no files are selected. Show context menu of current folder. */
        {
            gtk_menu_popup(GTK_MENU(win->popup), NULL, NULL, NULL, NULL, 3, gtk_get_current_event_time());
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
	gtk_statusbar_pop(GTK_STATUSBAR(win->statusbar), win->statusbar_ctx2);
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
        gtk_statusbar_push(GTK_STATUSBAR(win->statusbar), win->statusbar_ctx2, msg);
        g_free(msg);
    }
}

static void on_status(FmFolderView* fv, const char* msg, FmMainWin* win)
{
    gtk_statusbar_pop(GTK_STATUSBAR(win->statusbar), win->statusbar_ctx);
    gtk_statusbar_push(GTK_STATUSBAR(win->statusbar), win->statusbar_ctx, msg);
}

static void on_bookmark(GtkMenuItem* mi, FmMainWin* win)
{
    FmPath* path = (FmPath*)g_object_get_data(G_OBJECT(mi), "path");
    fm_main_win_chdir(win, path);
}

static void create_bookmarks_menu(FmMainWin* win)
{
    GList* l;
    int i = 0;
    /* FIXME: direct access to data member is not allowed */
    for(l=win->bookmarks->items;l;l=l->next)
    {
        FmBookmarkItem* item = (FmBookmarkItem*)l->data;
        GtkWidget* mi = gtk_image_menu_item_new_with_label(item->name);
        gtk_widget_show(mi);
        // gtk_image_menu_item_set_image(); // FIXME: set icons for menu items
        g_object_set_qdata_full(G_OBJECT(mi), fm_qdata_id, fm_path_ref(item->path), (GDestroyNotify)fm_path_unref);
        g_signal_connect(mi, "activate", G_CALLBACK(on_bookmark), win);
        gtk_menu_shell_insert(GTK_MENU_SHELL(win->bookmarks_menu), mi, i);
        ++i;
    }
    if(i > 0)
        gtk_menu_shell_insert(GTK_MENU_SHELL(win->bookmarks_menu), gtk_separator_menu_item_new(), i);
}

static void on_bookmarks_changed(FmBookmarks* bm, FmMainWin* win)
{
    /* delete old items first. */
    GList* mis = gtk_container_get_children(GTK_CONTAINER(win->bookmarks_menu));
    GList* l;
    for(l = mis;l;l=l->next)
    {
        GtkWidget* item = (GtkWidget*)l->data;
        if( GTK_IS_SEPARATOR_MENU_ITEM(item) )
            break;
        gtk_widget_destroy(item);
    }
    g_list_free(mis);

    create_bookmarks_menu(win);
}

static void load_bookmarks(FmMainWin* win, GtkUIManager* ui)
{
    GtkWidget* mi = gtk_ui_manager_get_widget(ui, "/menubar/BookmarksMenu");
    win->bookmarks_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(mi));
    win->bookmarks = fm_bookmarks_get();
    g_signal_connect(win->bookmarks, "changed", G_CALLBACK(on_bookmarks_changed), win);

    create_bookmarks_menu(win);
}

static void on_history_item(GtkMenuItem* mi, FmMainWin* win)
{
    GList* l = g_object_get_qdata(G_OBJECT(mi), fm_qdata_id);
    const FmNavHistoryItem* item = (FmNavHistoryItem*)l->data;
    int scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(win->folder_view)));
    fm_nav_history_jump(win->nav_history, l, scroll_pos);
    item = fm_nav_history_get_cur(win->nav_history);
    /* FIXME: should this be driven by a signal emitted on FmNavHistory? */
    fm_main_win_chdir_without_history(win, item->path);
}

static void on_show_history_menu(GtkMenuToolButton* btn, FmMainWin* win)
{
    GtkMenuShell* menu = (GtkMenuShell*)gtk_menu_tool_button_get_menu(btn);
    GList* l;
    GList* cur = fm_nav_history_get_cur_link(win->nav_history);

    /* delete old items */
    gtk_container_foreach(GTK_CONTAINER(menu), (GtkCallback)gtk_widget_destroy, NULL);

    for(l = fm_nav_history_list(win->nav_history); l; l=l->next)
    {
        const FmNavHistoryItem* item = (FmNavHistoryItem*)l->data;
        FmPath* path = item->path;
        char* str = fm_path_display_name(path, TRUE);
        GtkMenuItem* mi;
        if( l == cur )
        {
            mi = gtk_check_menu_item_new_with_label(str);
            gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(mi), TRUE);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), TRUE);
        }
        else
            mi = gtk_menu_item_new_with_label(str);
        g_free(str);

        g_object_set_qdata_full(G_OBJECT(mi), fm_qdata_id, l, NULL);
        g_signal_connect(mi, "activate", G_CALLBACK(on_history_item), win);
        gtk_menu_shell_append(menu, mi);
    }
    gtk_widget_show_all( GTK_WIDGET(menu) );
}

static void on_places_chdir(FmPlacesView* view, guint button, FmPath* path, FmMainWin* win)
{
    fm_main_win_chdir(win, path);
}

static void fm_main_win_init(FmMainWin *self)
{
    GtkWidget *vbox, *menubar, *toolitem, *next_btn, *scroll;
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkAction* act;
    GtkAccelGroup* accel_grp;

    ++n_wins;

    vbox = gtk_vbox_new(FALSE, 0);

    self->hpaned = gtk_hpaned_new();
    gtk_paned_set_position(GTK_PANED(self->hpaned), 150);

    /* places left pane */
    self->places_view = fm_places_view_new();
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), self->places_view);
    gtk_paned_add1(GTK_PANED(self->hpaned), scroll);

    /* folder view */
    self->folder_view = fm_folder_view_new( FM_FV_ICON_VIEW );
    fm_folder_view_set_show_hidden(FM_FOLDER_VIEW(self->folder_view), FALSE);
    fm_folder_view_sort(FM_FOLDER_VIEW(self->folder_view), GTK_SORT_ASCENDING, COL_FILE_NAME);
    fm_folder_view_set_selection_mode(FM_FOLDER_VIEW(self->folder_view), GTK_SELECTION_MULTIPLE);
    g_signal_connect(self->folder_view, "clicked", on_file_clicked, self);
    g_signal_connect(self->folder_view, "status", on_status, self);
    g_signal_connect(self->folder_view, "sel-changed", on_sel_changed, self);

    gtk_paned_add2(GTK_PANED(self->hpaned), self->folder_view);

    /* link places view with folder view. */
    g_signal_connect(self->places_view, "chdir", G_CALLBACK(on_places_chdir), self);

    /* create menu bar and toolbar */
    ui = gtk_ui_manager_new();
    act_grp = gtk_action_group_new("Main");
    gtk_action_group_add_actions(act_grp, main_win_actions, G_N_ELEMENTS(main_win_actions), self);
    gtk_action_group_add_toggle_actions(act_grp, main_win_toggle_actions, G_N_ELEMENTS(main_win_toggle_actions), self);
    gtk_action_group_add_radio_actions(act_grp, main_win_mode_actions, G_N_ELEMENTS(main_win_mode_actions), FM_FV_ICON_VIEW, on_change_mode, self);
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_type_actions, G_N_ELEMENTS(main_win_sort_type_actions), GTK_SORT_ASCENDING, on_sort_type, self);
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_by_actions, G_N_ELEMENTS(main_win_sort_by_actions), 0, on_sort_by, self);

    accel_grp = gtk_ui_manager_get_accel_group(ui);
    gtk_window_add_accel_group(GTK_WINDOW(self), accel_grp);

    gtk_ui_manager_insert_action_group(ui, act_grp, 0);
    gtk_ui_manager_add_ui_from_string(ui, main_menu_xml, -1, NULL);

    menubar = gtk_ui_manager_get_widget(ui, "/menubar");

    self->toolbar = gtk_ui_manager_get_widget(ui, "/toolbar");
    /* FIXME: should make these optional */
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(self->toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_style(GTK_TOOLBAR(self->toolbar), GTK_TOOLBAR_ICONS);

    /* create 'Next' button manually and add a popup menu to it */
    toolitem = g_object_new(GTK_TYPE_MENU_TOOL_BUTTON, NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(self->toolbar), toolitem, 2);
    gtk_widget_show(GTK_WIDGET(toolitem));
    act = gtk_ui_manager_get_action(ui, "/menubar/GoMenu/Next");
    gtk_activatable_set_related_action(GTK_ACTIVATABLE(toolitem), act);

    /* set up history menu */
    self->nav_history = fm_nav_history_new();
    self->history_menu = gtk_menu_new();
    gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolitem), self->history_menu);
    g_signal_connect(toolitem, "show-menu", G_CALLBACK(on_show_history_menu), self);

    self->popup = gtk_ui_manager_get_widget(ui, "/popup");

    gtk_box_pack_start( (GtkBox*)vbox, menubar, FALSE, TRUE, 0 );
    gtk_box_pack_start( (GtkBox*)vbox, self->toolbar, FALSE, TRUE, 0 );

    /* load bookmarks menu */
    load_bookmarks(self, ui);

    /* the location bar */
    self->location = fm_path_entry_new();
    g_signal_connect(self->location, "activate", on_entry_activate, self);
    g_signal_connect(self->folder_view, "loaded", G_CALLBACK(on_view_loaded), (gpointer) self);

    toolitem = gtk_tool_item_new();
    gtk_container_add( GTK_CONTAINER(toolitem), self->location );
    gtk_tool_item_set_expand(GTK_TOOL_ITEM(toolitem), TRUE);
    gtk_toolbar_insert((GtkToolbar*)self->toolbar, toolitem, gtk_toolbar_get_n_items(GTK_TOOLBAR(self->toolbar)) - 1 );

    gtk_box_pack_start( (GtkBox*)vbox, self->hpaned, TRUE, TRUE, 0 );

    /* status bar */
    self->statusbar = gtk_statusbar_new();
    gtk_box_pack_start( (GtkBox*)vbox, self->statusbar, FALSE, TRUE, 0 );
    self->statusbar_ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(self->statusbar), "status");
    self->statusbar_ctx2 = gtk_statusbar_get_context_id(GTK_STATUSBAR(self->statusbar), "status2");

    g_object_unref(act_grp);
    self->ui = ui;

    gtk_container_add( (GtkContainer*)self, vbox );
    gtk_widget_show_all(vbox);

    fm_folder_view_set_show_hidden(FM_FOLDER_VIEW(self->folder_view), FALSE);
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

    g_object_unref(self->nav_history);
    g_object_unref(self->ui);
    g_object_unref(self->bookmarks);

    if (G_OBJECT_CLASS(fm_main_win_parent_class)->finalize)
        (* G_OBJECT_CLASS(fm_main_win_parent_class)->finalize)(object);

    if(n_wins == 0)
        gtk_main_quit();
}

void on_about(GtkAction* act, FmMainWin* win)
{
    const char* authors[]={"Hong Jen Yee <pcman.tw@gmail.com>", NULL};
    GtkWidget* dlg = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dlg), "libfm-demo");
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dlg), authors);
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dlg), "A demo program for libfm");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dlg), "http://pcmanfm.sf.net/");
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

void on_show_hidden(GtkToggleAction* act, FmMainWin* win)
{
    gboolean active = gtk_toggle_action_get_active(act);
    fm_folder_view_set_show_hidden( FM_FOLDER_VIEW(win->folder_view), active );
}

void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int mode = gtk_radio_action_get_current_value(cur);
    fm_folder_view_set_mode( FM_FOLDER_VIEW(win->folder_view), mode );
}

void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int val = gtk_radio_action_get_current_value(cur);
    fm_folder_view_sort(FM_FOLDER_VIEW(win->folder_view), -1, val);
}

void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int val = gtk_radio_action_get_current_value(cur);
    fm_folder_view_sort(FM_FOLDER_VIEW(win->folder_view), val, -1);
}

void on_new_win(GtkAction* act, FmMainWin* win)
{
    win = fm_main_win_new();
    gtk_window_set_default_size(GTK_WINDOW(win), 640, 480);
    fm_main_win_chdir(win, fm_path_get_home());
    gtk_window_present(GTK_WINDOW(win));
}

void on_close_win(GtkAction* act, FmMainWin* win)
{
    gtk_widget_destroy(GTK_WIDGET(win));
}

/* void on_open_in_new_tab(GtkAction* act, FmMainWin* win);
{
}
*/

void on_open_in_new_win(GtkAction* act, FmMainWin* win)
{
    FmPathList* sels = fm_folder_view_get_selected_file_paths(FM_FOLDER_VIEW(win->folder_view));
    GList* l;
    for( l = fm_list_peek_head_link(sels); l; l=l->next )
    {
        FmPath* path = (FmPath*)l->data;
        win = fm_main_win_new();
        gtk_window_set_default_size(GTK_WINDOW(win), 640, 480);
        fm_main_win_chdir(win, path);
        gtk_window_present(GTK_WINDOW(win));
    }
    fm_list_unref(sels);
}


void on_go(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, gtk_entry_get_text(GTK_ENTRY(win->location)));
}

void on_go_back(GtkAction* act, FmMainWin* win)
{
    if(fm_nav_history_get_can_back(win->nav_history))
    {
        FmNavHistoryItem* item;
        int scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(win->folder_view)));
        fm_nav_history_back(win->nav_history, scroll_pos);
        item = fm_nav_history_get_cur(win->nav_history);
        /* FIXME: should this be driven by a signal emitted on FmNavHistory? */
        fm_main_win_chdir_without_history(win, item->path);
    }
}

void on_go_forward(GtkAction* act, FmMainWin* win)
{
    if(fm_nav_history_get_can_forward(win->nav_history))
    {
        FmNavHistoryItem* item;
        int scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(win->folder_view)));
        fm_nav_history_forward(win->nav_history, scroll_pos);
        /* FIXME: should this be driven by a signal emitted on FmNavHistory? */
        item = fm_nav_history_get_cur(win->nav_history);
        /* FIXME: should this be driven by a signal emitted on FmNavHistory? */
        fm_main_win_chdir_without_history(win, item->path);
    }
}

void on_go_up(GtkAction* act, FmMainWin* win)
{
    FmPath* parent = fm_path_get_parent(fm_folder_view_get_cwd(FM_FOLDER_VIEW(win->folder_view)));
    if(parent)
        fm_main_win_chdir( win, parent);
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
    fm_main_win_chdir(win, fm_path_get_apps_menu());
}

void fm_main_win_chdir_by_name(FmMainWin* win, const char* path_str)
{
    FmPath* path;
    gtk_entry_set_text(GTK_ENTRY(win->location), path_str);
    path = fm_path_new(path_str);
    fm_folder_view_chdir(FM_FOLDER_VIEW(win->folder_view), path);
    fm_path_unref(path);
}

void fm_main_win_chdir_without_history(FmMainWin* win, FmPath* path)
{
    fm_folder_view_chdir(FM_FOLDER_VIEW(win->folder_view), path);
    /* fm_nav_history_set_cur(); */
}

void fm_main_win_chdir(FmMainWin* win, FmPath* path)
{
    int scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(win->folder_view)));
    fm_nav_history_chdir(win->nav_history, path, scroll_pos);
    fm_main_win_chdir_without_history(win, path);
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
        FmPathList* files = fm_folder_view_get_selected_file_paths(FM_FOLDER_VIEW(win->folder_view));
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
        FmPathList* files = fm_folder_view_get_selected_file_paths(FM_FOLDER_VIEW(win->folder_view));
        if(files)
        {
            fm_clipboard_copy_files(win, files);
            fm_list_unref(files);
        }
    }
}

void on_copy_to(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(FM_FOLDER_VIEW(win->folder_view));
    if(files)
    {
        fm_copy_files_to(files);
        fm_list_unref(files);
    }
}

void on_move_to(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(FM_FOLDER_VIEW(win->folder_view));
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
        FmPath* path = fm_folder_view_get_cwd(FM_FOLDER_VIEW(win->folder_view));
        fm_clipboard_paste_files(win->folder_view, path);
    }
}

void on_del(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(FM_FOLDER_VIEW(win->folder_view));
    fm_trash_or_delete_files(files);
    fm_list_unref(files);
}

void on_rename(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(FM_FOLDER_VIEW(win->folder_view));
    if( !fm_list_is_empty(files) )
    {
        fm_rename_file(fm_list_peek_head(files));
        /* FIXME: is it ok to only rename the first selected file here. */
    }
    fm_list_unref(files);
}

void on_select_all(GtkAction* act, FmMainWin* win)
{
    fm_folder_view_select_all(FM_FOLDER_VIEW(win->folder_view));
}

void on_invert_select(GtkAction* act, FmMainWin* win)
{
    fm_folder_view_select_invert(FM_FOLDER_VIEW(win->folder_view));
}

void on_location(GtkAction* act, FmMainWin* win)
{
    gtk_widget_grab_focus(win->location);
}

void on_prop(GtkAction* action, FmMainWin* win)
{
    FmFolderView* fv = FM_FOLDER_VIEW(win->folder_view);
    /* FIXME: should prevent directly accessing data members */
    FmFileInfo* fi = FM_FOLDER_MODEL(fv->model)->dir->dir_fi;
    FmFileInfoList* files = fm_file_info_list_new();
    fm_list_push_tail(files, fi);
    fm_show_file_properties(files);
    fm_list_unref(files);
}

void on_create_new(GtkAction* action, FmMainWin* win)
{
    FmFolderView* fv = FM_FOLDER_VIEW(win->folder_view);
    const char* name = gtk_action_get_name(action);
    GError* err = NULL;
    FmPath* cwd = fm_folder_view_get_cwd(fv);
    FmPath* dest;
    char* basename;
_retry:
    basename = fm_get_user_input(GTK_WINDOW(win), _("Create New..."), _("Enter a name for the newly created file:"), _("New"));
    if(!basename)
        return;

    dest = fm_path_new_child(cwd, basename);
    g_free(basename);

    if( strcmp(name, "NewFolder") == 0 )
    {
        GFile* gf = fm_path_to_gfile(dest);
        if(!g_file_make_directory(gf, NULL, &err))
        {
            if(err->domain = G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                fm_path_unref(dest);
                g_error_free(err);
                g_object_unref(gf);
                err = NULL;
                goto _retry;
            }
            fm_show_error(GTK_WINDOW(win), err->message);
            g_error_free(err);
        }

        if(!err) /* select the newly created file */
        {
            /*FIXME: this doesn't work since the newly created file will
             * only be shown after file-created event was fired on its
             * folder's monitor and after FmFolder handles it in idle
             * handler. So, we cannot select it since it's not yet in
             * the folder model now. */
            /* fm_folder_view_select_file_path(fv, dest); */
        }
        g_object_unref(gf);
    }
    else if( strcmp(name, "NewBlank") == 0 )
    {
        GFile* gf = fm_path_to_gfile(dest);
        GFileOutputStream* f = g_file_create(gf, G_FILE_CREATE_NONE, NULL, &err);
        if(f)
        {
            g_output_stream_close(G_OUTPUT_STREAM(f), NULL, NULL);
            g_object_unref(f);
        }
        else
        {
            if(err->domain = G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                fm_path_unref(dest);
                g_error_free(err);
                g_object_unref(gf);
                err = NULL;
                goto _retry;
            }
            fm_show_error(GTK_WINDOW(win), err->message);
            g_error_free(err);
        }

        if(!err) /* select the newly created file */
        {
            /*FIXME: this doesn't work since the newly created file will
             * only be shown after file-created event was fired on its
             * folder's monitor and after FmFolder handles it in idle
             * handler. So, we cannot select it since it's not yet in
             * the folder model now. */
            /* fm_folder_view_select_file_path(fv, dest); */
        }
        g_object_unref(gf);
    }
    else /* templates */
    {

    }
    fm_path_unref(dest);
}
