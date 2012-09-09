/*
 *      fm-clipboard.c
 *
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
 *      Copyright 2012 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

#include "gtk-compat.h"
#include "fm-clipboard.h"
#include "fm-gtk-utils.h"

enum {
    URI_LIST = 1,
    GNOME_COPIED_FILES,
    KDE_CUT_SEL,
    UTF8_STRING,
    N_CLIPBOARD_TARGETS
};

static GtkTargetEntry targets[]=
{
    {"text/uri-list", 0, URI_LIST},
    {"x-special/gnome-copied-files", 0, GNOME_COPIED_FILES},
    {"application/x-kde-cutselection", 0, KDE_CUT_SEL},
    { "UTF8_STRING", 0, UTF8_STRING }
};

static GdkAtom target_atom[N_CLIPBOARD_TARGETS];

static gboolean got_atoms = FALSE;

static gboolean is_cut = FALSE;

static inline void check_atoms(void)
{
    if(!got_atoms)
    {
        guint i;

        for(i = 0; i < N_CLIPBOARD_TARGETS; i++)
            target_atom[i] = GDK_NONE;
        for(i = 0; i < G_N_ELEMENTS(targets); i++)
            target_atom[targets[i].info] = gdk_atom_intern_static_string(targets[i].target);
        got_atoms = TRUE;
    }
}

static void get_data(GtkClipboard *clip, GtkSelectionData *sel, guint info, gpointer user_data)
{
    FmPathList* files = (FmPathList*)user_data;
    GString* uri_list;
    GdkAtom target = gtk_selection_data_get_target(sel);

    if(info == KDE_CUT_SEL)
    {
        /* set application/kde-cutselection data */
        if(is_cut)
            gtk_selection_data_set(sel, target, 8, (guchar*)"1", 2);
        return;
    }

    uri_list = g_string_sized_new(4096);
    if(info == GNOME_COPIED_FILES)
        g_string_append(uri_list, is_cut ? "cut\n" : "copy\n");
    if(info == UTF8_STRING)
    {
        GList* l = fm_path_list_peek_head_link(files);
        while(l)
        {
            FmPath* path = (FmPath*)l->data;
            char* str = fm_path_to_str(path);
            g_string_append(uri_list, str);
            g_string_append_c(uri_list, '\n');
            g_free(str);
            l=l->next;
        }
    }
    else/* text/uri-list format */
    {
        fm_path_list_write_uri_list(files, uri_list);
    }
    gtk_selection_data_set(sel, target, 8, (guchar*)uri_list->str, uri_list->len + 1);
    g_string_free(uri_list, TRUE);
    if(is_cut)
        gtk_clipboard_clear(clip);
    is_cut = FALSE;
}

static void clear_data(GtkClipboard* clip, gpointer user_data)
{
    FmPathList* files = (FmPathList*)user_data;
    fm_path_list_unref(files);
    is_cut = FALSE;
}

gboolean fm_clipboard_cut_or_copy_files(GtkWidget* src_widget, FmPathList* files, gboolean _is_cut)
{
    GdkDisplay* dpy = src_widget ? gtk_widget_get_display(src_widget) : gdk_display_get_default();
    GtkClipboard* clip = gtk_clipboard_get_for_display(dpy, GDK_SELECTION_CLIPBOARD);
    gboolean ret;

    ret = gtk_clipboard_set_with_data(clip, targets, G_N_ELEMENTS(targets),
                                      get_data, clear_data, fm_path_list_ref(files));
    is_cut = _is_cut;
    return ret;
}

static gboolean check_kde_curselection(GtkClipboard* clip)
{
    /* Check application/x-kde-cutselection:
     * If the content of this format is string "1", that means the
     * file is cut in KDE (Dolphin). */
    gboolean ret = FALSE;
    GdkAtom atom = gdk_atom_intern_static_string(targets[KDE_CUT_SEL-1].target);
    GtkSelectionData* data = gtk_clipboard_wait_for_contents(clip, atom);
    if(data)
    {
        gint length, format;
        const gchar* pdata;

        pdata = (const gchar*)gtk_selection_data_get_data_with_length(data, &length);
        format = gtk_selection_data_get_format(data);
        if(length > 0 && format == 8 && pdata[0] == '1')
            ret = TRUE;
        gtk_selection_data_free(data);
    }
    return ret;
}

gboolean fm_clipboard_paste_files(GtkWidget* dest_widget, FmPath* dest_dir)
{
    GdkDisplay* dpy = dest_widget ? gtk_widget_get_display(dest_widget) : gdk_display_get_default();
    GtkClipboard* clip = gtk_clipboard_get_for_display(dpy, GDK_SELECTION_CLIPBOARD);
    FmPathList* files;
    char** uris;
    GdkAtom atom;
    int type = 0;
    GdkAtom *avail_targets;
    int n, i;

    /* get all available targets currently in the clipboard. */
    if( !gtk_clipboard_wait_for_targets(clip, &avail_targets, &n) )
        return FALSE;

    /* check gnome and xfce compatible format first */
    atom = gdk_atom_intern_static_string(targets[GNOME_COPIED_FILES-1].target);
    for(i = 0; i < n; ++i)
    {
        if(avail_targets[i] == atom)
        {
            type = GNOME_COPIED_FILES;
            break;
        }
    }
    if( 0 == type ) /* x-special/gnome-copied-files is not found. */
    {
        /* check uri-list */
        atom = gdk_atom_intern_static_string(targets[URI_LIST-1].target);
        for(i = 0; i < n; ++i)
        {
            if(avail_targets[i] == atom)
            {
                type = URI_LIST;
                break;
            }
        }
        if( 0 == type ) /* text/uri-list is not found. */
        {
            /* finally, fallback to UTF-8 string */
            atom = gdk_atom_intern_static_string(targets[UTF8_STRING-1].target);
            for(i = 0; i < n; ++i)
            {
                if(avail_targets[i] == atom)
                {
                    type = UTF8_STRING;
                    break;
                }
            }
        }
    }
    g_free(avail_targets);

    if( type )
    {
        GtkSelectionData* data = gtk_clipboard_wait_for_contents(clip, atom);
        const gchar* pdata;
        gint length;

        pdata = (const gchar*)gtk_selection_data_get_data_with_length(data, &length);
        is_cut = FALSE;

        switch(type)
        {
        case GNOME_COPIED_FILES:
            is_cut = g_str_has_prefix(pdata, "cut\n");
            while(length)
            {
                register gchar ch = *pdata++;
                length--;
                if(ch == '\n')
                    break;
            }
            /* the following parts is actually a uri-list, so don't break here. */
        case URI_LIST:
            uris = g_uri_list_extract_uris(pdata);
            if( type != GNOME_COPIED_FILES )
            {
                /* if we're not handling x-special/gnome-copied-files, check
                 * if information from KDE is available. */
                is_cut = check_kde_curselection(clip);
            }
            break;
        case UTF8_STRING:
            /* FIXME: how should we treat UTF-8 strings? URIs or filenames? */
            uris = g_uri_list_extract_uris(pdata);
            break;
        }
        gtk_selection_data_free(data);

        if(uris)
        {
            GtkWidget* parent;
            if(dest_widget)
                parent = gtk_widget_get_toplevel(dest_widget);
            else
                parent = NULL;

            files = fm_path_list_new_from_uris(uris);
            g_strfreev(uris);

            if(!fm_path_list_is_empty(files))
            {
                if( is_cut )
                    fm_move_files(GTK_WINDOW(parent), files, dest_dir);
                else
                    fm_copy_files(GTK_WINDOW(parent), files, dest_dir);
            }
            fm_path_list_unref(files);
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * fm_clipboard_have_files
 * @dest_widget: (allow-none): widget to paste files
 *
 * Checks if clipboard have data available for paste.
 *
 * Returns: %TRUE if the clipboard have data that can be handled.
 *
 * Since: 1.0.1
 */
gboolean fm_clipboard_have_files(GtkWidget* dest_widget)
{
    GdkDisplay* dpy = dest_widget ? gtk_widget_get_display(dest_widget) : gdk_display_get_default();
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(dpy, GDK_SELECTION_CLIPBOARD);
    guint i;

    check_atoms();
    for(i = 1; i < N_CLIPBOARD_TARGETS; i++)
        if(target_atom[i] != GDK_NONE
           && gtk_clipboard_wait_is_target_available(clipboard, target_atom[i]))
            return TRUE;
    return FALSE;
}
