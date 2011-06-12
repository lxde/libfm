//      fm-side-pane.c
//
//      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

#include <config.h>

#include "fm-side-pane.h"

#include "fm-places-view.h"
#include "fm-dir-tree-model.h"
#include "fm-dir-tree-view.h"
#include "fm-file-info-job.h"

#include <glib/gi18n-lib.h>

enum
{
    CHDIR,
    MODE_CHANGED,
    N_SIGNALS
};
static guint signals[N_SIGNALS];
static FmDirTreeModel* dir_tree_model = NULL;

static char menu_xml[] =
"<popup>"
  "<menuitem action='Places'/>"
  "<menuitem action='DirTree'/>"
//  "<menuitem action='Remote'/>"
"</popup>";

static GtkRadioActionEntry menu_actions[]=
{
    {"Places", NULL, N_("Places"), NULL, NULL, FM_SP_PLACES},
    {"DirTree", NULL, N_("Directory Tree"), NULL, NULL, FM_SP_DIR_TREE},
    {"Remote", NULL, N_("Remote"), NULL, NULL, FM_SP_REMOTE},
};


static void fm_side_pane_finalize            (GObject *object);

G_DEFINE_TYPE(FmSidePane, fm_side_pane, GTK_TYPE_VBOX)


static void fm_side_pane_class_init(FmSidePaneClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_side_pane_finalize;

    signals[CHDIR] =
        g_signal_new("chdir",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(FmSidePaneClass, chdir),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__UINT_POINTER,
                     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);

    signals[MODE_CHANGED] =
        g_signal_new("mode-changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(FmSidePaneClass, mode_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0, G_TYPE_NONE);
}


static void fm_side_pane_finalize(GObject *object)
{
    FmSidePane *sp;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_SIDE_PANE(object));
    sp = FM_SIDE_PANE(object);

    if(sp->cwd)
        fm_path_unref(sp->cwd);

    g_object_unref(sp->ui);

    G_OBJECT_CLASS(fm_side_pane_parent_class)->finalize(object);
}

/* Adopted from gtk/gtkmenutoolbutton.c
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
 */
static void menu_position_func(GtkMenu *menu, int *x, int *y, gboolean *push_in, GtkButton *button)
{
    GtkWidget *widget = GTK_WIDGET(button);
    GtkRequisition req;
    GtkRequisition menu_req;
    GtkTextDirection direction;
    GdkRectangle monitor;
    gint monitor_num;
    GdkScreen *screen;

    gtk_widget_size_request (GTK_WIDGET (menu), &menu_req);
    direction = gtk_widget_get_direction (widget);

    /* make the menu as wide as the button when needed */
    if(menu_req.width < GTK_WIDGET(button)->allocation.width)
    {
        menu_req.width = GTK_WIDGET(button)->allocation.width;
        gtk_widget_set_size_request(GTK_WIDGET(menu), menu_req.width, -1);
    }

    screen = gtk_widget_get_screen (GTK_WIDGET (menu));
    monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);
    if (monitor_num < 0)
        monitor_num = 0;
    gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

    gdk_window_get_origin (widget->window, x, y);
    *x += widget->allocation.x;
    *y += widget->allocation.y;
/*
    if (direction == GTK_TEXT_DIR_LTR)
        *x += MAX (widget->allocation.width - menu_req.width, 0);
    else if (menu_req.width > widget->allocation.width)
        *x -= menu_req.width - widget->allocation.width;
*/
    if ((*y + widget->allocation.height + menu_req.height) <= monitor.y + monitor.height)
        *y += widget->allocation.height;
    else if ((*y - menu_req.height) >= monitor.y)
        *y -= menu_req.height;
    else if (monitor.y + monitor.height - (*y + widget->allocation.height) > *y)
        *y += widget->allocation.height;
    else
        *y -= menu_req.height;
    *push_in = FALSE;
}

static void on_menu_btn_clicked(GtkButton* btn, FmSidePane* sp)
{
    gtk_menu_popup(sp->menu, NULL, NULL,
                   menu_position_func, btn,
                   1, gtk_get_current_event_time());
}

static void on_mode_changed(GtkRadioAction* act, GtkRadioAction *current, FmSidePane* sp)
{
    FmSidePaneMode mode = gtk_radio_action_get_current_value(act);
    if(mode != sp->mode)
        fm_side_pane_set_mode(sp, mode);
}

static void fm_side_pane_init(FmSidePane *sp)
{
    GtkActionGroup* act_grp = gtk_action_group_new("SidePane");
    GtkWidget* hbox;

    gtk_action_group_set_translation_domain(act_grp, GETTEXT_PACKAGE);
    sp->title_bar = gtk_hbox_new(FALSE, 0);
    sp->menu_label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(sp->menu_label), 0.0, 0.5);
    sp->menu_btn = gtk_button_new();
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), sp->menu_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE),
                       FALSE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(sp->menu_btn), hbox);
    // gtk_widget_set_tooltip_text(sp->menu_btn, _(""));

    g_signal_connect(sp->menu_btn, "clicked", G_CALLBACK(on_menu_btn_clicked), sp);
    gtk_button_set_relief(GTK_BUTTON(sp->menu_btn), GTK_RELIEF_NONE);
    gtk_box_pack_start(GTK_BOX(sp->title_bar), sp->menu_btn, TRUE, TRUE, 0);

    /* the drop down menu */
    sp->ui = gtk_ui_manager_new();
    gtk_ui_manager_add_ui_from_string(sp->ui, menu_xml, -1, NULL);
    gtk_action_group_add_radio_actions(act_grp, menu_actions, G_N_ELEMENTS(menu_actions),
                                       -1, G_CALLBACK(on_mode_changed), sp);
    gtk_ui_manager_insert_action_group(sp->ui, act_grp, -1);
    g_object_unref(act_grp);
    sp->menu = gtk_ui_manager_get_widget(sp->ui, "/popup");

    sp->scroll = gtk_scrolled_window_new(NULL, NULL);

    gtk_box_pack_start(GTK_BOX(sp), sp->title_bar, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(sp), sp->scroll, TRUE, TRUE, 0);
    gtk_widget_show_all(GTK_WIDGET(sp));
}


GtkWidget *fm_side_pane_new(void)
{
    return g_object_new(FM_TYPE_SIDE_PANE, NULL);
}

FmPath* fm_side_pane_get_cwd(FmSidePane* sp)
{
    return sp->cwd;
}

void fm_side_pane_chdir(FmSidePane* sp, FmPath* path)
{
    if(sp->cwd)
        fm_path_unref(sp->cwd);
    sp->cwd = fm_path_ref(path);

    switch(sp->mode)
    {
    case FM_SP_PLACES:
        fm_places_chdir(FM_PLACES_VIEW(sp->view), path);
        break;
    case FM_SP_DIR_TREE:
        fm_dir_tree_view_chdir(FM_DIR_TREE_VIEW(sp->view), path);
        break;
    default:
        break;
    }
}

static void on_places_chdir(FmPlacesView* view, guint button, FmPath* path, FmSidePane* sp)
{
//    g_signal_handlers_block_by_func(win->dirtree_view, on_dirtree_chdir, win);
//    fm_main_win_chdir(win, path);
//    g_signal_handlers_unblock_by_func(win->dirtree_view, on_dirtree_chdir, win);
    if(sp->cwd)
        fm_path_unref(sp->cwd);
    sp->cwd = fm_path_ref(path);
    g_signal_emit(sp, signals[CHDIR], 0, button, path);
}

static void on_dirtree_chdir(FmDirTreeView* view, guint button, FmPath* path, FmSidePane* sp)
{
//    g_signal_handlers_block_by_func(win->places_view, on_places_chdir, win);
//    fm_main_win_chdir(win, path);
//    g_signal_handlers_unblock_by_func(win->places_view, on_places_chdir, win);
    if(sp->cwd)
        fm_path_unref(sp->cwd);
    sp->cwd = fm_path_ref(path);
    g_signal_emit(sp, signals[CHDIR], 0, button, path);
}

void init_dir_tree(FmSidePane* sp)
{
    if(dir_tree_model)
        g_object_ref(dir_tree_model);
    else
    {
        FmFileInfoJob* job = fm_file_info_job_new(NULL, FM_FILE_INFO_JOB_NONE);
        GList* l;
        /* query FmFileInfo for home dir and root dir, and then,
         * add them to dir tree model */
        fm_file_info_job_add(job, fm_path_get_home());
        fm_file_info_job_add(job, fm_path_get_root());

        /* FIXME: maybe it's cleaner to use run_async here? */
        fm_job_run_sync_with_mainloop(FM_JOB(job));

        dir_tree_model = fm_dir_tree_model_new();
        for(l = fm_list_peek_head_link(job->file_infos); l; l = l->next)
        {
            FmFileInfo* fi = FM_FILE_INFO(l->data);
            fm_dir_tree_model_add_root(dir_tree_model, fi, NULL);
        }
        g_object_unref(job);

        g_object_add_weak_pointer(dir_tree_model, &dir_tree_model);
    }
    gtk_tree_view_set_model(FM_DIR_TREE_VIEW(sp->view), dir_tree_model);
    g_object_unref(dir_tree_model);
}

void fm_side_pane_set_mode(FmSidePane* sp, FmSidePaneMode mode)
{
    if(mode == sp->mode)
        return;
    sp->mode = mode;

    if(sp->view)
        gtk_widget_destroy(sp->view);

    switch(mode)
    {
    case FM_SP_PLACES:
        gtk_label_set_text(GTK_LABEL(sp->menu_label), _("Places"));
        /* create places view */
        sp->view = fm_places_view_new();
        fm_places_chdir(FM_PLACES_VIEW(sp->view), sp->cwd);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sp->scroll),
                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

        g_signal_connect(sp->view, "chdir", G_CALLBACK(on_places_chdir), sp);
        break;
    case FM_SP_DIR_TREE:
        gtk_label_set_text(GTK_LABEL(sp->menu_label), _("Directory Tree"));
        /* create a dir tree */
        sp->view = fm_dir_tree_view_new();
        init_dir_tree(sp);
        fm_dir_tree_view_chdir(FM_DIR_TREE_VIEW(sp->view), sp->cwd);

        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sp->scroll),
                GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

        g_signal_connect(sp->view, "chdir", G_CALLBACK(on_dirtree_chdir), sp);
        break;
    default:
        sp->view = NULL;
        /* not implemented */
        return;
        break;
    }
    gtk_widget_show(sp->view);
    gtk_container_add(GTK_CONTAINER(sp->scroll), sp->view);

    g_signal_emit(sp, signals[MODE_CHANGED], 0);

    /* update the popup menu */
    gtk_radio_action_set_current_value(gtk_ui_manager_get_action(sp->ui, "/popup/Places"),
                                       sp->mode);
}

FmSidePaneMode fm_side_pane_get_mode(FmSidePane* sp)
{
    return sp->mode;
}

GtkWidget* fm_side_pane_get_title_bar(FmSidePane* sp)
{
    return sp->title_bar;
}
