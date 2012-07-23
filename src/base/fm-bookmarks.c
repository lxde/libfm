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

/**
 * SECTION:fm-bookmarks
 * @short_description: Bookmarks support for libfm.
 * @title: FmBookmarks
 *
 * @include: libfm/fm-bookmarks.h
 *
 * The application that uses libfm can use user-wide bookmark list via
 * class FmBookmarks.
 */

#include "fm-bookmarks.h"
#include <stdio.h>
#include <string.h>

enum
{
    CHANGED,
    N_SIGNALS
};

static FmBookmarks* fm_bookmarks_new (void);

static void fm_bookmarks_finalize           (GObject *object);
static GList* load_bookmarks(const char* fpath);
static void free_item(FmBookmarkItem* item);
static char* get_bookmarks_file();

G_DEFINE_TYPE(FmBookmarks, fm_bookmarks, G_TYPE_OBJECT);

static FmBookmarks* singleton = NULL;
static guint signals[N_SIGNALS];

static guint idle_handler = 0;

static void fm_bookmarks_class_init(FmBookmarksClass *klass)
{
    GObjectClass *g_object_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_bookmarks_finalize;

    /**
     * FmBookmarks::changed:
     * @bookmarks: pointer to bookmarks list singleton descriptor
     *
     * The "changed" signal is emitted when some bookmark item is
     * changed, added, or removed.
     *
     * Since: 0.1.0
     */
    signals[CHANGED] =
        g_signal_new("changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmBookmarksClass, changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0 );
}

static void fm_bookmarks_finalize(GObject *object)
{
    FmBookmarks *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_BOOKMARKS(object));

    self = FM_BOOKMARKS(object);

    if(idle_handler)
    {
        g_source_remove(idle_handler);
        idle_handler = 0;
    }

    g_list_foreach(self->items, (GFunc)free_item, NULL);
    g_list_free(self->items);

    g_object_unref(self->mon);

    G_OBJECT_CLASS(fm_bookmarks_parent_class)->finalize(object);
}

static char* get_bookmarks_file()
{
    return g_build_filename(g_get_home_dir(), ".gtk-bookmarks", NULL);
}

static void free_item(FmBookmarkItem* item)
{
    if(item->name != item->path->name)
        g_free(item->name);
    fm_path_unref(item->path);
    g_slice_free(FmBookmarkItem, item);
}

static void on_changed( GFileMonitor* mon, GFile* gf, GFile* other,
                    GFileMonitorEvent evt, FmBookmarks* bookmarks )
{
    char* fpath;
    /* reload bookmarks */
    g_list_foreach(bookmarks->items, (GFunc)free_item, NULL);
    g_list_free(bookmarks->items);

    fpath = get_bookmarks_file();
    bookmarks->items = load_bookmarks(fpath);
    g_free(fpath);
    g_signal_emit(bookmarks, signals[CHANGED], 0);
}

static FmBookmarkItem* new_item(char* line)
{
    FmBookmarkItem* item = g_slice_new0(FmBookmarkItem);
    char* sep;
    sep = strchr(line, '\n');
    if(sep)
        *sep = '\0';
    sep = strchr(line, ' ');
    if(sep)
        *sep = '\0';

    item->path = fm_path_new_for_uri(line);
    if(sep)
        item->name = g_strdup(sep+1);
    else
        item->name = g_filename_display_name(item->path->name);

    return item;
}

static GList* load_bookmarks(const char* fpath)
{
    FILE* f;
    char buf[1024];
    FmBookmarkItem* item;
    GList* items = NULL;

    /* load the file */
    f = fopen(fpath, "r");
    if(f)
    {
        while(fgets(buf, 1024, f))
        {
            item = new_item(buf);
            items = g_list_prepend(items, item);
        }
        fclose(f);
    }
    items = g_list_reverse(items);
    return items;
}

static void fm_bookmarks_init(FmBookmarks *self)
{
    char* fpath = get_bookmarks_file();
    GFile* gf = g_file_new_for_path(fpath);
    self->mon = g_file_monitor_file(gf, 0, NULL, NULL);
    g_object_unref(gf);
    g_signal_connect(self->mon, "changed", G_CALLBACK(on_changed), self);

    self->items = load_bookmarks(fpath);
    g_free(fpath);
}

static FmBookmarks *fm_bookmarks_new(void)
{
    return g_object_new(FM_BOOKMARKS_TYPE, NULL);
}

/**
 * fm_bookmarks_dup
 *
 * Returns reference to bookmarks list singleton descriptor.
 *
 * Return value: (transfer full): a reference to bookmarks list
 *
 * Since: 0.1.99
 */
FmBookmarks* fm_bookmarks_dup(void)
{
    if( G_LIKELY(singleton) )
        g_object_ref(singleton);
    else
    {
        singleton = fm_bookmarks_new();
        g_object_add_weak_pointer(G_OBJECT(singleton), (gpointer*)&singleton);
    }
    return singleton;
}

/**
 * fm_bookmarks_list_all
 * @bookmarks: bookmarks list
 *
 * Returns list of FmBookmarkItem retrieved from bookmarks list. Returned
 * list is owned by bookmarks list and should not be freed by caller.
 *
 * Return value: (transfer none): (element-type #FmBookmarkItem): list of bookmark items
 *
 * Since: 0.1.0
 */
const GList* fm_bookmarks_list_all(FmBookmarks* bookmarks)
{
    return bookmarks->items;
}

static gboolean save_bookmarks(FmBookmarks* bookmarks)
{
    FmBookmarkItem* item;
    GList* l;
    GString* buf = g_string_sized_new(1024);
    char* fpath;

    for( l=bookmarks->items; l; l=l->next )
    {
        char* uri;
        item = (FmBookmarkItem*)l->data;
        uri = fm_path_to_uri(item->path);
        g_string_append(buf, uri);
        g_free(uri);
        g_string_append_c(buf, ' ');
        g_string_append(buf, item->name);
        g_string_append_c(buf, '\n');
    }

    fpath = get_bookmarks_file();
    g_file_set_contents(fpath, buf->str, buf->len, NULL);
    g_free(fpath);

    g_string_free(buf, TRUE);
    return FALSE;
}

static void queue_save_bookmarks(FmBookmarks* bookmarks)
{
    if(idle_handler)
        g_source_remove(idle_handler);
    idle_handler = g_idle_add((GSourceFunc)save_bookmarks, bookmarks);
}

/**
 * fm_bookmarks_insert
 * @bookmarks: bookmarks list
 * @path: path requested to add to bookmarks
 * @name: name new bookmark will be seen in list with
 * @pos: where to insert a bookmark into list
 *
 * Adds a bookmark into bookmark list. Returned structure is managed by
 * bookmarks list and should not be freed by caller.
 *
 * Return value: (transfer none): new created bookmark item
 *
 * Since: 0.1.0
 */
FmBookmarkItem* fm_bookmarks_insert(FmBookmarks* bookmarks, FmPath* path, const char* name, int pos)
{
    FmBookmarkItem* item = g_slice_new0(FmBookmarkItem);
    item->path = fm_path_ref(path);
    item->name = g_strdup(name);
    bookmarks->items = g_list_insert(bookmarks->items, item, pos);
    /* g_debug("insert %s at %d", name, pos); */
    queue_save_bookmarks(bookmarks);
    return item;
}

/**
 * fm_bookmarks_remove
 * @bookmarks: bookmarks list
 * @item: bookmark item for deletion
 *
 * Removes a bookmark from bookmark list.
 *
 * Since: 0.1.0
 */
void fm_bookmarks_remove(FmBookmarks* bookmarks, FmBookmarkItem* item)
{
    bookmarks->items = g_list_remove(bookmarks->items, item);
    free_item(item);
    queue_save_bookmarks(bookmarks);
}

/**
 * fm_bookmarks_rename
 * @bookmarks: bookmarks list
 * @item: bookmark item which will be changed
 * @new_name: new name for bookmark item to be seen in list
 *
 * Changes name of existing bookmark item.
 *
 * Since: 0.1.0
 */
void fm_bookmarks_rename(FmBookmarks* bookmarks, FmBookmarkItem* item, const char* new_name)
{
    g_free(item->name);
    item->name = g_strdup(new_name);
    queue_save_bookmarks(bookmarks);
}

/**
 * fm_bookmarks_reorder
 * @bookmarks: bookmarks list
 * @item: bookmark item which will be changed
 * @pos: new position for bookmark item in list
 *
 * Changes position of existing bookmark item.
 *
 * Since: 0.1.0
 */
void fm_bookmarks_reorder(FmBookmarks* bookmarks, FmBookmarkItem* item, int pos)
{
    bookmarks->items = g_list_remove(bookmarks->items, item);
    bookmarks->items = g_list_insert(bookmarks->items, item, pos);
    queue_save_bookmarks(bookmarks);
}
