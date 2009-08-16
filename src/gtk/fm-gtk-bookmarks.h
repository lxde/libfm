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


#ifndef __FM_GTK_BOOKMARKS_H__
#define __FM_GTK_BOOKMARKS_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "fm-path.h"

G_BEGIN_DECLS

#define FM_GTK_BOOKMARKS_TYPE				(fm_gtk_bookmarks_get_type())
#define FM_GTK_BOOKMARKS(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_GTK_BOOKMARKS_TYPE, FmGtkBookmarks))
#define FM_GTK_BOOKMARKS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_GTK_BOOKMARKS_TYPE, FmGtkBookmarksClass))
#define IS_FM_GTK_BOOKMARKS(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_GTK_BOOKMARKS_TYPE))
#define IS_FM_GTK_BOOKMARKS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_GTK_BOOKMARKS_TYPE))

typedef struct _FmGtkBookmarks			FmGtkBookmarks;
typedef struct _FmGtkBookmarksClass		FmGtkBookmarksClass;
typedef struct _FmGtkBookmarkItem       FmGtkBookmarkItem;

struct _FmGtkBookmarkItem
{
    char* name;
    FmPath* path;
};

struct _FmGtkBookmarks
{
	GObject parent;
    GFileMonitor* mon;
    GList* items;
};

struct _FmGtkBookmarksClass
{
    GObjectClass parent_class;
    void (*add)(int pos, GList* l);
    void (*remove)(int pos, GList* l);
};

GType fm_gtk_bookmarks_get_type(void);
FmGtkBookmarks* fm_gtk_bookmarks_get(void);

/* list all bookmark items in current bookmarks */
const GList* fm_gtk_bookmarks_list_all(FmGtkBookmarks* bookmarks);

G_END_DECLS

#endif /* __FM_GTK_BOOKMARKS_H__ */
