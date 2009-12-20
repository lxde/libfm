/*
 *      fm-gtk-bookmarks.h
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


#ifndef __FM_BOOKMARKS_H__
#define __FM_BOOKMARKS_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "fm-path.h"

G_BEGIN_DECLS

#define FM_BOOKMARKS_TYPE				(fm_bookmarks_get_type())
#define FM_BOOKMARKS(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_BOOKMARKS_TYPE, FmBookmarks))
#define FM_BOOKMARKS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_BOOKMARKS_TYPE, FmBookmarksClass))
#define IS_FM_BOOKMARKS(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_BOOKMARKS_TYPE))
#define IS_FM_BOOKMARKS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_BOOKMARKS_TYPE))

typedef struct _FmBookmarks			FmBookmarks;
typedef struct _FmBookmarksClass		FmBookmarksClass;
typedef struct _FmBookmarkItem       FmBookmarkItem;

struct _FmBookmarkItem
{
    char* name;
    FmPath* path;
};

struct _FmBookmarks
{
	GObject parent;
    GFileMonitor* mon;
    GList* items;
};

struct _FmBookmarksClass
{
    GObjectClass parent_class;
    void (*changed)();
};

GType fm_bookmarks_get_type(void);
FmBookmarks* fm_bookmarks_get(void);

#define fm_bookmarks_append(bookmarks, path, name)  fm_bookmarks_insert(bookmarks, path, name, -1)
FmBookmarkItem* fm_bookmarks_insert(FmBookmarks* bookmarks, FmPath* path, const char* name, int pos);
void fm_bookmarks_remove(FmBookmarks* bookmarks, FmBookmarkItem* item);
void fm_bookmarks_rename(FmBookmarks* bookmarks, FmBookmarkItem* item, const char* new_name);

/* list all bookmark items in current bookmarks */
GList* fm_bookmarks_list_all(FmBookmarks* bookmarks);

G_END_DECLS

#endif /* __FM_BOOKMARKS_H__ */
