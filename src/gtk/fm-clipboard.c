/*
 *      fm-clipboard.c
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

#include "fm-clipboard.h"
#include "fm-file-ops.h"

enum {
	URI_LIST = 1,
	GNOME_COPIED_FILES,
	KDE_CUT_SEL,
	UTF8_STRING
};

static GtkTargetEntry targets[]=
{
	{"text/uri-list", 0, URI_LIST},
	{"text/x-special/gnome-copied-files", 0, GNOME_COPIED_FILES},
	{"application/x-kde-cutselection", 0, KDE_CUT_SEL},
	{ "UTF8_STRING", 0, UTF8_STRING }
};

static void get_data(GtkClipboard *clip, GtkSelectionData *sel, guint info, gpointer user_data)
{
	FmPathList* files = (FmPathList*)user_data;
	GString* uri_list = g_string_sized_new(4096);
	if(info == GNOME_COPIED_FILES)
		g_string_append(uri_list, "copy\n"); /* FIXME: support 'cut' */
	if(info == UTF8_STRING)
	{
		GList* l = fm_list_peek_head_link(files);
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
	else /* text/uri-list format */
	{
		fm_path_list_write_uri_list(files, uri_list);
	}
	gtk_selection_data_set(sel, sel->target, 8, uri_list->str, uri_list->len + 1);
	g_string_free(uri_list, TRUE);
}

static void clear_data(GtkClipboard* clip, gpointer user_data)
{
	FmPathList* files = (FmPathList*)user_data;
	fm_list_unref(files);
}

gboolean fm_clipboard_cut_or_copy_files(GtkWidget* src_widget, FmPathList* files, gboolean is_copy)
{
	GdkDisplay* dpy = src_widget ? gtk_widget_get_display(src_widget) : gdk_display_get_default();
	GtkClipboard* clip = gtk_clipboard_get_for_display(dpy, GDK_SELECTION_CLIPBOARD);
	return gtk_clipboard_set_with_data(clip, targets, G_N_ELEMENTS(targets),
					get_data, clear_data, fm_list_ref(files));
}

gboolean fm_clipboard_paste_files(GtkWidget* dest_widget, FmPath* dest_dir)
{
	GdkDisplay* dpy = dest_widget ? gtk_widget_get_display(dest_widget) : gdk_display_get_default();
	GtkClipboard* clip = gtk_clipboard_get_for_display(dpy, GDK_SELECTION_CLIPBOARD);
	FmPathList* files;
	char** uris = gtk_clipboard_wait_for_uris(clip), **uri;
	gboolean is_copy = TRUE; /* FIXME: distinguishing copy and cut. */

	if(!uris)
		return FALSE;
	files = fm_path_list_new();
	for(uri = uris; *uri; ++uri)
	{
		char* unescaped = g_uri_unescape_string(*uri, NULL);
		FmPath* path = fm_path_new(unescaped);
		g_free(unescaped);
		fm_list_push_tail(files, path);
		fm_path_unref(path);
	}
	g_free(uris);

	/* FIXME: distinguishing copy and cut. */
	if( is_copy )
		fm_copy_files(files, dest_dir);
	else
		fm_move_files(files, dest_dir);
	
	fm_list_unref(files);
	return TRUE;
}
