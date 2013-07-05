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

/**
 * SECTION:fm-file-menu
 * @short_description: Simple context menu for files.
 * @title: FmFileMenu
 *
 * @include: libfm/fm-gtk.h
 *
 * The #FmFileMenu can be used to create context menu on some file(s).
 *
 * The menu consists of items:
 * |[
 * Open
 * ------------------------
 * &lt;placeholder name='ph1'/&gt;
 * ------------------------
 * &lt;placeholder name='ph2'/&gt;
 * ------------------------
 * Cut
 * Copy
 * Paste
 * Del
 * ------------------------
 * AddBookmark
 * Rename
 * ------------------------
 * &lt;placeholder name='ph3'/&gt;
 * ------------------------
 * Prop
 * ]|
 * You can modity the menu replacing placeholders. Note that internally
 * the menu constructor also puts some conditional elements into those
 * placeholders:
 * - ph2: 'OpenWith' list+selector (optionally in submenu 'OpenWithMenu');
 * - ph3: custom (user defined) menu elements
 *  
 *        'Compress' if there is archiver defined;
 *  
 *        'Extract' if this is an archive
 *
 * Element 'AddBookmark' is visible only if menu is created for one directory.
 * Element 'Rename' is hidden if menu is created for more than one file.
 *
 * Elements 'Cut', 'Copy', 'Paste', and 'Del' are hidden for any virtual
 * place but trash can.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include "../gtk-compat.h"

#include "fm.h"

#include "fm-file-menu.h"

#include "fm-clipboard.h"
#include "fm-file-properties.h"
#include "fm-gtk-utils.h"
#include "fm-app-chooser-dlg.h"
#include "fm-gtk-file-launcher.h"

struct _FmFileMenu
{
    FmFileInfoList* file_infos;
    gboolean same_type : 1;
    //gboolean disable_archiving : 1;
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkMenu* menu;

    FmLaunchFolderFunc folder_func;
    gpointer folder_func_data;

    FmPath* cwd;
};

static void on_open(GtkAction* action, gpointer user_data);
static void on_open_with_app(GtkAction* action, gpointer user_data);
static void on_open_with(GtkAction* action, gpointer user_data);
static void on_cut(GtkAction* action, gpointer user_data);
static void on_copy(GtkAction* action, gpointer user_data);
static void on_paste(GtkAction* action, gpointer user_data);
static void on_delete(GtkAction* action, gpointer user_data);
static void on_add_bookmark(GtkAction* action, FmFileMenu* menu);
static void on_rename(GtkAction* action, gpointer user_data);
static void on_compress(GtkAction* action, gpointer user_data);
static void on_extract_here(GtkAction* action, gpointer user_data);
static void on_extract_to(GtkAction* action, gpointer user_data);
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
  "<menuitem action='Del'/>"
  "<separator/>"
  "<menuitem action='AddBookmark'/>"
  "<menuitem action='Rename'/>"
/* TODO: implement symlink creation and "send to".
  "<menuitem action='Link'/>"
  "<menu action='SendTo'>"
  "</menu>"
*/
  "<separator/>"
  "<placeholder name='ph3'/>"
  "<separator/>"
  "<menuitem action='Prop'/>"
"</popup>";

/* FIXME: how to show accel keys in the popup menu? */
GtkActionEntry base_menu_actions[]=
{
    {"Open", GTK_STOCK_OPEN, N_("_Open"), NULL, NULL, G_CALLBACK(on_open)},
    {"OpenWith", NULL, N_("Open _With..."), NULL, NULL, G_CALLBACK(on_open_with)},
    {"OpenWithMenu", NULL, N_("Open _With..."), NULL, NULL, NULL},
    {"Cut", GTK_STOCK_CUT, NULL, NULL, NULL, G_CALLBACK(on_cut)},
    {"Copy", GTK_STOCK_COPY, NULL, NULL, NULL, G_CALLBACK(on_copy)},
    {"Paste", GTK_STOCK_PASTE, NULL, NULL, NULL, G_CALLBACK(on_paste)},
    {"Del", GTK_STOCK_DELETE, NULL, NULL, NULL, G_CALLBACK(on_delete)},
    {"AddBookmark", GTK_STOCK_ADD, N_("_Add to Bookmarks"), NULL, NULL, G_CALLBACK(on_add_bookmark)},
    {"Rename", NULL, N_("_Rename"), NULL, NULL, G_CALLBACK(on_rename)},
    {"Link", NULL, N_("Create _Symlink"), NULL, NULL, NULL},
    {"SendTo", NULL, N_("Se_nd To"), NULL, NULL, NULL},
    {"Compress", NULL, N_("Co_mpress..."), NULL, NULL, G_CALLBACK(on_compress)},
    {"Extract", NULL, N_("Extract _Here"), NULL, NULL, G_CALLBACK(on_extract_here)},
    {"Extract2", NULL, N_("E_xtract To..."), NULL, NULL, G_CALLBACK(on_extract_to)},
    {"Prop", GTK_STOCK_PROPERTIES, N_("Prop_erties"), NULL, NULL, G_CALLBACK(on_prop)}
};

/**
 * fm_file_menu_destroy
 * @menu: a menu
 *
 * Destroys menu object.
 *
 * Since: 0.1.0
 */
void fm_file_menu_destroy(FmFileMenu* menu)
{
    gtk_widget_destroy(GTK_WIDGET(menu->menu));

    if(menu->file_infos)
        fm_file_info_list_unref(menu->file_infos);

    if(menu->cwd)
        fm_path_unref(menu->cwd);

    g_object_unref(menu->act_grp);
    g_object_unref(menu->ui);
    g_slice_free(FmFileMenu, menu);
}

/**
 * fm_file_menu_new_for_file
 * @parent: window to place menu over
 * @fi: target file
 * @cwd: working directory
 * @auto_destroy: %TRUE if manu should be destroyed after some action was activated
 *
 * Creates new menu for the file.
 *
 * Returns: a new #FmFileMenu object.
 *
 * Since: 0.1.0
 */
FmFileMenu* fm_file_menu_new_for_file(GtkWindow* parent, FmFileInfo* fi, FmPath* cwd, gboolean auto_destroy)
{
    FmFileMenu* menu;
    FmFileInfoList* files = fm_file_info_list_new();
    fm_file_info_list_push_tail(files, fi);
    menu = fm_file_menu_new_for_files(parent, files, cwd, auto_destroy);
    fm_file_info_list_unref(files);
    return menu;
}

/**
 * fm_file_menu_new_for_files
 * @parent: window to place menu over
 * @files: target files
 * @cwd: working directory
 * @auto_destroy: %TRUE if manu should be destroyed after some action was activated
 *
 * Creates new menu for some files list.
 *
 * Returns: a new #FmFileMenu object.
 *
 * Since: 0.1.0
 */
FmFileMenu* fm_file_menu_new_for_files(GtkWindow* parent, FmFileInfoList* files, FmPath* cwd, gboolean auto_destroy)
{
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkAction* act;
    FmFileInfo* fi = fm_file_info_list_peek_head(files);
    FmFileMenu* data = g_slice_new0(FmFileMenu);
    GString* xml;
    GList* mime_types = NULL;
    GList* l;
    GList* apps = NULL;
    FmPath* path = fm_file_info_get_path(fi);
    /* FIXME: three unused bits with extensions implemented */
    gboolean same_fs, all_virtual, all_trash;
    gboolean all_native = TRUE;
    unsigned items_num = fm_file_info_list_get_length(files);

    data->file_infos = fm_file_info_list_ref(files);

    /* check if the files are on the same filesystem */
    same_fs = fm_file_info_list_is_same_fs(files);

    all_virtual = same_fs && !fm_path_is_native(path);
    all_trash = same_fs && fm_path_is_trash(path);

    /* create list of mime types */
    for(l = fm_file_info_list_peek_head_link(files); l; l = l->next)
    {
        FmMimeType* mime_type;
        GList* l2;

        fi = l->data;
        if (!fm_file_info_is_native(fi))
            all_native = FALSE;
        mime_type = fm_file_info_get_mime_type(fi);
        if(mime_type == NULL || !fm_file_info_is_native(fi))
            continue;
        for(l2 = mime_types; l2; l2 = l2->next)
            if(l2->data == mime_type)
                break;
        if(l2) /* already added */
            continue;
        mime_types = g_list_prepend(mime_types, fm_mime_type_ref(mime_type));
    }
    /* create apps list */
    if(mime_types)
    {
        data->same_type = (mime_types->next == NULL);
        apps = g_app_info_get_all_for_type(fm_mime_type_get_type(mime_types->data));
        for(l = mime_types->next; l; l = l->next)
        {
            GList *apps2, *l2, *l3;
            apps2 = g_app_info_get_all_for_type(fm_mime_type_get_type(l->data));
            for(l2 = apps; l2; )
            {
                for(l3 = apps2; l3; l3 = l3->next)
                    if(g_app_info_equal(l2->data, l3->data))
                        break;
                if(l3) /* this app supports all files */
                {
                    /* g_debug("%s supports %s", g_app_info_get_id(l2->data), fm_mime_type_get_type(l->data)); */
                    l2 = l2->next;
                    continue;
                }
                /* g_debug("%s invalid for %s", g_app_info_get_id(l2->data), fm_mime_type_get_type(l->data)); */
                g_object_unref(l2->data);
                l3 = l2->next; /* save for next iter */
                apps = g_list_delete_link(apps, l2);
                l2 = l3; /* continue with next item */
            }
            g_list_foreach(apps2, (GFunc)g_object_unref, NULL);
            g_list_free(apps2);
        }
    }

    data->ui = ui = gtk_ui_manager_new();
    data->act_grp = act_grp = gtk_action_group_new("Popup");
    gtk_action_group_set_translation_domain(act_grp, GETTEXT_PACKAGE);

    if(cwd)
        data->cwd = fm_path_ref(cwd);

    gtk_action_group_add_actions(act_grp, base_menu_actions, G_N_ELEMENTS(base_menu_actions), data);
    gtk_ui_manager_add_ui_from_string(ui, base_menu_xml, -1, NULL);
    gtk_ui_manager_insert_action_group(ui, act_grp, 0);

    xml = g_string_new("<popup><placeholder name='ph2'>");
    if(apps) /* add specific menu items for those files */
    {
            gboolean use_sub = g_list_length(apps) > 5;
            if(use_sub)
                g_string_append(xml, "<menu action='OpenWithMenu'>");

            for(l=apps;l;l=l->next)
            {
                GAppInfo* app = l->data;

                /*g_debug("app %s, executable %s, command %s\n",
                    g_app_info_get_name(app),
                    g_app_info_get_executable(app),
                    g_app_info_get_commandline(app));*/

                gchar * program_path = g_find_program_in_path(g_app_info_get_executable(app));
                if (!program_path)
                    continue;
                g_free(program_path);

                act = gtk_action_new(g_app_info_get_id(app),
                                     g_app_info_get_name(app),
                                     g_app_info_get_description(app),
                                     NULL);
                g_signal_connect(act, "activate", G_CALLBACK(on_open_with_app), data);
                gtk_action_set_gicon(act, g_app_info_get_icon(app));
                gtk_action_group_add_action(act_grp, act);
                /* associate the app info object with the action */
                g_object_set_qdata_full(G_OBJECT(act), fm_qdata_id, app, g_object_unref);
                g_string_append_printf(xml, "<menuitem action='%s'/>", g_app_info_get_id(app));
            }

            g_list_free(apps); /* don't unref GAppInfos now */
            if(use_sub)
            {
                g_string_append(xml,
                                "<separator/>"
                                "<menuitem action='OpenWith'/>"
                                "</menu>");
            }
            else
                g_string_append(xml, "<menuitem action='OpenWith'/>");
    }
    else
    {
        act = gtk_ui_manager_get_action(ui, "/popup/Open");
        gtk_action_set_visible(act, FALSE);
        g_string_append(xml, "<menuitem action='OpenWith'/>");
    }
    g_string_append(xml, "</placeholder></popup>");
    //if (data->same_type) ...run mime-specific extensions...

    /* archiver integration */
    if (all_native)
    {
            FmArchiver* archiver = fm_archiver_get_default();
            if(archiver)
            {
                g_string_append(xml, "<popup><placeholder name='ph3'>");
                for (l = mime_types; l; l = l->next)
                    if (!fm_archiver_is_mime_type_supported(archiver, fm_mime_type_get_type(l->data)))
                        break;
                if (mime_types && l == NULL) /* all are archives */
                {
                    if(data->cwd && archiver->extract_to_cmd)
                        g_string_append(xml, "<menuitem action='Extract'/>");
                    if(archiver->extract_cmd)
                        g_string_append(xml, "<menuitem action='Extract2'/>");
                }
                else
                    g_string_append(xml, "<menuitem action='Compress'/>");
        g_string_append(xml, "</placeholder></popup>");
            }
    }
    g_list_foreach(mime_types, (GFunc)fm_mime_type_unref, NULL);
    g_list_free(mime_types);
    mime_types = NULL;

    /* Special handling for some virtual filesystems */
    /* FIXME: it should be done on per-scheme basis */
    if(all_virtual)
    {
        if (!all_trash)
        {
            /* do not provide these items for other virtual files */
            /* FIXME: this is invalid way to do it, many virtual paths still
               support getting files content and some can deletion as well.
               Cut/Paste/Rename/Del should be disabled only if FS is R/O and
               Copy only for very few FS such as trash:// or menu://
               Since query if FS is R/O is async operation we have to get
               such info from FmFolder therefore do this in fm-folder-view.c */
            act = gtk_ui_manager_get_action(ui, "/popup/Cut");
            gtk_action_set_visible(act, FALSE);
            act = gtk_ui_manager_get_action(ui, "/popup/Copy");
            gtk_action_set_visible(act, FALSE);
            act = gtk_ui_manager_get_action(ui, "/popup/Paste");
            gtk_action_set_visible(act, FALSE);
            act = gtk_ui_manager_get_action(ui, "/popup/Del");
            gtk_action_set_visible(act, FALSE);
        }
        act = gtk_ui_manager_get_action(ui, "/popup/Rename");
        gtk_action_set_visible(act, FALSE);
    }

    /* shadow 'Paste' if clipboard is empty and unshadow if not */
    act = gtk_ui_manager_get_action(ui, "/popup/Paste");
    if(items_num != 1 || !fm_file_info_is_dir(fm_file_info_list_peek_head(files)))
    {
        gtk_action_set_visible(act, FALSE);
        act = gtk_ui_manager_get_action(ui, "/popup/AddBookmark");
        gtk_action_set_visible(act, FALSE);
    }
    else
        gtk_action_set_sensitive(act, fm_clipboard_have_files(GTK_WIDGET(parent)));

    if (items_num != 1)
    {
        act = gtk_ui_manager_get_action(ui, "/popup/Rename");
        gtk_action_set_visible(act, FALSE);
    }

    gtk_ui_manager_add_ui_from_string(ui, xml->str, xml->len, NULL);

    data->menu = GTK_MENU(gtk_ui_manager_get_widget(data->ui, "/popup"));
    gtk_menu_attach_to_widget(data->menu, GTK_WIDGET(parent), NULL);
    fm_widget_menu_fix_tooltips(data->menu);

    if(auto_destroy)
    {
        g_signal_connect_swapped(data->menu, "selection-done",
                                 G_CALLBACK(fm_file_menu_destroy), data);
    }

    g_string_free(xml, TRUE);
    return data;
}

/**
 * fm_file_menu_get_ui
 * @menu: a menu
 *
 * Retrieves UI manager object for @menu. Returned data are owned by
 * @menu and should be not freed by caller.
 *
 * Returns: (transfer none): UI manager.
 *
 * Since: 0.1.0
 */
GtkUIManager* fm_file_menu_get_ui(FmFileMenu* menu)
{
    return menu->ui;
}

/**
 * fm_file_menu_get_action_group
 * @menu: a menu
 *
 * Retrieves action group for @menu. Returned data are owned by
 * @menu and should be not freed by caller.
 *
 * Returns: (transfer none): the action group.
 *
 * Since: 0.1.0
 */
GtkActionGroup* fm_file_menu_get_action_group(FmFileMenu* menu)
{
    return menu->act_grp;
}

/**
 * fm_file_menu_get_file_info_list
 * @menu: a menu
 *
 * Retrieves list of files @menu was created for. Returned data are owned
 * by @menu and should be not freed by caller.
 *
 * Returns: (transfer none): list of files.
 *
 * Since: 0.1.0
 */
FmFileInfoList* fm_file_menu_get_file_info_list(FmFileMenu* menu)
{
    return menu->file_infos;
}

/**
 * fm_file_menu_get_menu
 * @menu: a menu
 *
 * Retrieves #GtkMenu widget built with GtkUIManager. Returned data are
 * owned by @menu and should be not freed by caller.
 *
 * Returns: (transfer none): created #GtkMenu widget.
 *
 * Since: 0.1.0
 */
GtkMenu* fm_file_menu_get_menu(FmFileMenu* menu)
{
    return menu->menu;
}

static void on_open(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    GList* l = fm_file_info_list_peek_head_link(data->file_infos);
    GtkWindow *window = GTK_WINDOW(gtk_menu_get_attach_widget(data->menu));
    fm_launch_files_simple(window, NULL, l, data->folder_func, data->folder_func_data);
}

static void open_with_app(FmFileMenu* data, GAppInfo* app)
{
    GdkAppLaunchContext* ctx;
    FmFileInfoList* files = data->file_infos;
    GList* l = fm_file_info_list_peek_head_link(files);
    GList* uris = NULL;
    int i;
    for(i=0; l; ++i, l=l->next)
    {
        FmFileInfo* fi = FM_FILE_INFO(l->data);
        FmPath* path = fm_file_info_get_path(fi);
        char* uri = fm_path_to_uri(path);
        uris = g_list_prepend(uris, uri);
    }
    uris = g_list_reverse(uris);

    ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
    gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(GTK_WIDGET(data->menu)));
    gdk_app_launch_context_set_icon(ctx, g_app_info_get_icon(app));
    gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());

    /* FIXME: error handling. */
    fm_app_info_launch_uris(app, uris, G_APP_LAUNCH_CONTEXT(ctx), NULL);
    g_object_unref(ctx);

    g_list_foreach(uris, (GFunc)g_free, NULL);
    g_list_free(uris);
}

static void on_open_with_app(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    GAppInfo* app = G_APP_INFO(g_object_get_qdata(G_OBJECT(action), fm_qdata_id));
    /* g_debug("%s", gtk_action_get_name(action)); */
    open_with_app(data, app);
}

static void on_open_with(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmFileInfoList* files = data->file_infos;
    FmFileInfo* fi = fm_file_info_list_peek_head(files);
    FmMimeType* mime_type;
    GAppInfo* app;
    GtkWindow *window = GTK_WINDOW(gtk_menu_get_attach_widget(data->menu));

    if(data->same_type)
        mime_type = fm_file_info_get_mime_type(fi);
    else
        mime_type = NULL;

    app = fm_choose_app_for_mime_type(window, mime_type, TRUE);

    if(app)
    {
        open_with_app(data, app);
        /* add the app to apps that support this file type. */
        if(mime_type)
            g_app_info_add_supports_type(app, fm_mime_type_get_type(mime_type), NULL);
        g_object_unref(app);
    }
}

static void on_cut(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_clipboard_cut_files(gtk_menu_get_attach_widget(data->menu), files);
    fm_path_list_unref(files);
}

static void on_copy(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    fm_clipboard_copy_files(gtk_menu_get_attach_widget(data->menu), files);
    fm_path_list_unref(files);
}

static void on_paste(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmFileInfo* fi = fm_file_info_list_peek_head(data->file_infos);
    if (fi)
    {
        fm_clipboard_paste_files(gtk_menu_get_attach_widget(data->menu),
                                 fm_file_info_get_path(fi));
    }
}

static void on_delete(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    GtkWindow *window = GTK_WINDOW(gtk_menu_get_attach_widget(data->menu));
    GdkModifierType mask = 0;
    files = fm_path_list_new_from_file_info_list(data->file_infos);
    /* Fix for #3436283: accept Shift to delete instead of trash */
    /* FIXME: change menu item text&icon when Shift is pressed */
    gdk_window_get_device_position (gtk_widget_get_window(GTK_WIDGET(data->menu)),
                                    gtk_get_current_event_device(),
                                    NULL, NULL, &mask);
    if(mask & GDK_SHIFT_MASK)
        fm_delete_files(window, files);
    else
        fm_trash_or_delete_files(window, files);
    fm_path_list_unref(files);
}

static void on_add_bookmark(GtkAction* action, FmFileMenu* menu)
{
    FmFileInfo* fi = fm_file_info_list_peek_head(menu->file_infos);
    FmBookmarks* bookmarks;
    if(fi)
    {
        bookmarks = fm_bookmarks_dup();
        fm_bookmarks_append(bookmarks, fm_file_info_get_path(fi),
                            fm_file_info_get_disp_name(fi));
        g_object_unref(bookmarks);
    }
}

static void on_rename(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmFileInfo* fi = fm_file_info_list_peek_head(data->file_infos);
    GtkWindow *window = GTK_WINDOW(gtk_menu_get_attach_widget(data->menu));
    if(fi)
        fm_rename_file(window, fm_file_info_get_path(fi));
    /* FIXME: is it ok to only rename the first selected file here? */
}

static void on_compress(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    FmArchiver* archiver = fm_archiver_get_default();
    if(archiver)
    {
        GAppLaunchContext* ctx = (GAppLaunchContext*)gdk_display_get_app_launch_context(gdk_display_get_default());
        files = fm_path_list_new_from_file_info_list(data->file_infos);
        fm_archiver_create_archive(archiver, ctx, files);
        fm_path_list_unref(files);
        g_object_unref(ctx);
    }
}

static void on_extract_here(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    FmArchiver* archiver = fm_archiver_get_default();
    if(archiver)
    {
        GAppLaunchContext* ctx = (GAppLaunchContext*)gdk_display_get_app_launch_context(gdk_display_get_default());
        files = fm_path_list_new_from_file_info_list(data->file_infos);
        fm_archiver_extract_archives_to(archiver, ctx, files, data->cwd);
        fm_path_list_unref(files);
        g_object_unref(ctx);
    }
}

static void on_extract_to(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    FmPathList* files;
    FmArchiver* archiver = fm_archiver_get_default();
    if(archiver)
    {
        GAppLaunchContext* ctx = (GAppLaunchContext*)gdk_display_get_app_launch_context(gdk_display_get_default());
        files = fm_path_list_new_from_file_info_list(data->file_infos);
        fm_archiver_extract_archives(archiver, ctx, files);
        fm_path_list_unref(files);
        g_object_unref(ctx);
    }
}

static void on_prop(GtkAction* action, gpointer user_data)
{
    FmFileMenu* data = (FmFileMenu*)user_data;
    GtkWindow *window = GTK_WINDOW(gtk_menu_get_attach_widget(data->menu));
    fm_show_file_properties(window, data->file_infos);
}

/**
 * fm_file_menu_is_single_file_type
 * @menu: a menu
 *
 * Checks if @menu was created for files of the same type.
 *
 * Returns: %TRUE if menu is single-type.
 *
 * Since: 0.1.0
 */
gboolean fm_file_menu_is_single_file_type(FmFileMenu* menu)
{
    return menu->same_type;
}

/**
 * fm_file_menu_set_folder_func
 * @menu: a menu
 * @func: function to open folder
 * @user_data: data supplied for @func
 *
 * Sets up function to open folders for @menu. Function will be called
 * if action 'Open' was activated for some folder.
 *
 * Since: 0.1.0
 */
void fm_file_menu_set_folder_func(FmFileMenu* menu, FmLaunchFolderFunc func, gpointer user_data)
{
    menu->folder_func = func;
    menu->folder_func_data = user_data;
}
