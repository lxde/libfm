/*
 *      fm-app-menu-view.c
 *
 *      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include "fm-app-menu-view.h"
#include <menu-cache.h>
#include <glib/gi18n-lib.h>
#include <gio/gdesktopappinfo.h>
#include <string.h>

enum
{
    COL_ICON,
    COL_TITLE,
    COL_ITEM,
    N_COLS
};

static GtkTreeStore* store = NULL;
static MenuCache* menu_cache = NULL;
static gpointer menu_cache_reload_notify = NULL;

static void destroy_store(gpointer user_data)
{
    menu_cache_remove_reload_notify(menu_cache, menu_cache_reload_notify);
    menu_cache_reload_notify = NULL;
    menu_cache_unref(menu_cache);
    menu_cache = NULL;
    store = NULL;
}

static void add_menu_items(GtkTreeIter* parent_it, MenuCacheDir* dir)
{
    GtkTreeIter it;
    GSList * l;
    GIcon* gicon;
    /* Iterate over all menu items in this directory. */
    for (l = menu_cache_dir_get_children(dir); l != NULL; l = l->next)
    {
        /* Get the menu item. */
        MenuCacheItem* item = MENU_CACHE_ITEM(l->data);
        switch(menu_cache_item_get_type(item))
        {
            case MENU_CACHE_TYPE_NONE:
            case MENU_CACHE_TYPE_SEP:
                break;
            case MENU_CACHE_TYPE_APP:
            case MENU_CACHE_TYPE_DIR:
                if(menu_cache_item_get_icon(item))
                {
                    if(g_path_is_absolute(menu_cache_item_get_icon(item)))
                    {
                        GFile* gf = g_file_new_for_path(menu_cache_item_get_icon(item));
                        gicon = g_file_icon_new(gf);
                        g_object_unref(gf);
                    }
                    else
                    {
                        char* dot = strrchr((char*)menu_cache_item_get_icon(item), '.');
                        if(dot && (strcmp(dot+1, "png") == 0 || strcmp(dot+1, "svg") == 0 || strcmp(dot+1, "xpm") == 0))
                        {
                            char* name = g_strndup(menu_cache_item_get_icon(item), dot - menu_cache_item_get_icon(item));
                            gicon = g_themed_icon_new(name);
                            g_free(name);
                        }
                        else
                            gicon = g_themed_icon_new(menu_cache_item_get_icon(item));
                    }
                }
                else
                    gicon = NULL;
                gtk_tree_store_append(store, &it, parent_it);
                gtk_tree_store_set(store, &it,
                                   COL_ICON, gicon,
                                   COL_TITLE, menu_cache_item_get_name(item),
                                   COL_ITEM, item, -1);
                if(gicon)
                    g_object_unref(gicon);

                if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_DIR)
                    add_menu_items(&it, MENU_CACHE_DIR(item));
                break;
        }
    }
}

static void on_menu_cache_reload(MenuCache* mc, gpointer user_data)
{
    g_return_if_fail(store);
    gtk_tree_store_clear(store);
    MenuCacheDir* dir = menu_cache_get_root_dir(menu_cache);
    /* FIXME: preserve original selection */
    if(dir)
        add_menu_items(NULL, dir);
}

GtkWidget *fm_app_menu_view_new(void)
{
    GtkWidget* view;
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;

    if(!store)
    {
        static GType menu_cache_item_type = 0;
        char* oldenv;
        if(G_UNLIKELY(!menu_cache_item_type))
            menu_cache_item_type = g_boxed_type_register_static("MenuCacheItem",
                                            (GBoxedCopyFunc)menu_cache_item_ref,
                                            (GBoxedFreeFunc)menu_cache_item_unref);
        store = gtk_tree_store_new(N_COLS, G_TYPE_ICON, /*GDK_TYPE_PIXBUF, */G_TYPE_STRING, menu_cache_item_type);
        g_object_weak_ref(G_OBJECT(store), (GWeakNotify)destroy_store, NULL);

        /* ensure that we're using lxmenu-data */
        oldenv = g_strdup(g_getenv("XDG_MENU_PREFIX"));
        g_setenv("XDG_MENU_PREFIX", "lxde-", TRUE);
        menu_cache = menu_cache_lookup("applications.menu");
        g_setenv("XDG_MENU_PREFIX", oldenv, TRUE);
        g_free(oldenv);

        if(menu_cache)
        {
            MenuCacheDir* dir = menu_cache_get_root_dir(menu_cache);
            menu_cache_reload_notify = menu_cache_add_reload_notify(menu_cache, on_menu_cache_reload, NULL);
            if(dir) /* content of menu is already loaded */
                add_menu_items(NULL, dir);
        }
    }
    else
        g_object_ref(store);

    view = gtk_tree_view_new_with_model((GtkTreeModel*)store);

    render = gtk_cell_renderer_pixbuf_new();
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Installed Applications"));
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_set_attributes(col, render, "gicon", COL_ICON, NULL);

    render = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_set_attributes(col, render, "text", COL_TITLE, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

    g_object_unref(store);
    return view;
}

GAppInfo* fm_app_menu_view_get_selected_app(GtkTreeView* view)
{
    char* id = fm_app_menu_view_get_selected_app_desktop_id(view);
    if(id)
    {
        GDesktopAppInfo* app = g_desktop_app_info_new(id);
        g_free(id);
        return app;
    }
    return NULL;
}

char* fm_app_menu_view_get_selected_app_desktop_id(GtkTreeView* view)
{
    GtkTreeIter it;
    GtkTreeSelection* sel = gtk_tree_view_get_selection(view);
    if(gtk_tree_selection_get_selected(sel, NULL, &it))
    {
        MenuCacheItem* item;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &it, COL_ITEM, &item, -1);
        if(item && menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP)
            return g_strdup(menu_cache_item_get_id(item));
    }
    return NULL;
}

char* fm_app_menu_view_get_selected_app_desktop_file(GtkTreeView* view)
{
    GtkTreeIter it;
    GtkTreeSelection* sel = gtk_tree_view_get_selection(view);
    if(gtk_tree_selection_get_selected(sel, NULL, &it))
    {
        MenuCacheItem* item;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &it, COL_ITEM, &item, -1);
        if(item && menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP)
        {
            char* path = menu_cache_item_get_file_path(item);
            return path;
        }
    }
    return NULL;
}

gboolean fm_app_menu_view_is_item_app(GtkTreeView* view, GtkTreeIter* it)
{
    MenuCacheItem* item;
    gtk_tree_model_get(GTK_TREE_MODEL(store), it, COL_ITEM, &item, -1);
    if(item && menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP)
        return TRUE;
    return FALSE;
}

gboolean fm_app_menu_view_is_app_selected(GtkTreeView* view)
{
    GtkTreeIter it;
    GtkTreeSelection* sel = gtk_tree_view_get_selection(view);
    if(gtk_tree_selection_get_selected(sel, NULL, &it))
        return fm_app_menu_view_is_item_app(view, &it);
    return FALSE;
}
