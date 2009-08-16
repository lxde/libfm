/*
 *      fm-gtk-bookmarks.c
 *      
 *      Copyright 2009 PCMan <pcman@debian>
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

#include "fm-gtk-bookmarks.h"
#include <stdio.h>
#include <string.h>

static FmGtkBookmarks*	fm_gtk_bookmarks_new (void);

static void fm_gtk_bookmarks_finalize  			(GObject *object);

G_DEFINE_TYPE(FmGtkBookmarks, fm_gtk_bookmarks, G_TYPE_OBJECT);

static FmGtkBookmarks* singleton = NULL;

static void fm_gtk_bookmarks_class_init(FmGtkBookmarksClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_gtk_bookmarks_finalize;

}


static void fm_gtk_bookmarks_finalize(GObject *object)
{
	FmGtkBookmarks *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_GTK_BOOKMARKS(object));

	self = FM_GTK_BOOKMARKS(object);
    g_object_unref(self->mon);

	G_OBJECT_CLASS(fm_gtk_bookmarks_parent_class)->finalize(object);
}

static void on_changed( GFileMonitor* mon, GFile* gf, GFile* other, 
                    GFileMonitorEvent evt, FmGtkBookmarks* bookmarks )
{
    /* reload bookmarks */
}

static FmGtkBookmarkItem* new_item(char* line)
{
    FmGtkBookmarkItem* item = g_slice_new0(FmGtkBookmarkItem);
    char* sep;
    sep = strchr(line, '\n');
    if(sep)
        *sep = '\0';
    sep = strchr(line, ' ');
    if(sep)
        *sep = '\0';

    /* FIXME: this is no longer needed once fm_path_new can convert file:/// to / */
    if(g_str_has_prefix(line, "file:/"))
    {
        char* fpath = g_filename_from_uri(line, NULL, NULL);
        item->path = fm_path_new(fpath);
        g_free(fpath);
    }
    else
    {
        /* FIXME: is unescape needed? */
        item->path = fm_path_new(line);
    }

    if(sep)
        item->name = g_strdup(sep+1);
    else
        item->name = g_filename_display_name(item->path->name);

    return item;
}

GList* load_bookmarks(const char* fpath)
{
    FILE* f;
    char buf[1024];
    FmGtkBookmarkItem* item;
    GList* items = NULL;

    /* load the file */
    if( f = fopen(fpath, "r") )
    {
        while(fgets(buf, 1024, f))
        {
            item = new_item(buf);
            items = g_list_prepend(items, item);
        }
    }
    return items;
}

static void fm_gtk_bookmarks_init(FmGtkBookmarks *self)
{
    FILE* f;
    char buf[1024];
    FmGtkBookmarkItem* item;
    GList* items = NULL;
    char* fpath = g_build_filename(g_get_home_dir(), ".gtk-bookmarks", NULL);
    GFile* gf = g_file_new_for_path(fpath);
	self->mon = g_file_monitor_file(gf, 0, NULL, NULL);
    g_object_unref(gf);
    g_signal_connect(self->mon, "changed", G_CALLBACK(on_changed), self);

    self->items = load_bookmarks(fpath);
    g_free(fpath);
}

FmGtkBookmarks *fm_gtk_bookmarks_new(void)
{
	return g_object_new(FM_GTK_BOOKMARKS_TYPE, NULL);
}

FmGtkBookmarks* fm_gtk_bookmarks_get(void)
{
    if( G_LIKELY(singleton) )
        g_object_ref(singleton);
    else
    {
        singleton = fm_gtk_bookmarks_new();
        g_object_add_weak_pointer(singleton, &singleton);
    }
    return singleton;
}

const GList* fm_gtk_bookmarks_list_all(FmGtkBookmarks* bookmarks)
{
    return bookmarks->items;
}
