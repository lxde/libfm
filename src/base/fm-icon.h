/*
 *      fm-icon.h
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


#ifndef __FM_ICON_H__
#define __FM_ICON_H__

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _FmIcon			FmIcon;

struct _FmIcon
{
    guint n_ref;
    GIcon* gicon;
    /* FIXME: should we utilize g_object_set_qdata to
              store those data in gicon object instead? */
    gpointer user_data;
};

/* must be called before using FmIcon */
void fm_icon_init();
void fm_icon_finalize();

FmIcon* fm_icon_from_gicon(GIcon* gicon);
FmIcon* fm_icon_ref(FmIcon* icon);
void fm_icon_unref(FmIcon* icon);

/* Those APIs are used by GUI toolkits to cache loaded icon pixmaps
 * or GdkPixbuf in the FmIcon objects. Fox example, libfm-gtk stores
 * a list of GdkPixbuf objects in FmIcon::user_data.
 * It shouldn't be used in other ways by application developers. */
gpointer fm_icon_get_user_data(FmIcon* icon);
void fm_icon_set_user_data(FmIcon* icon, gpointer user_data);
void fm_icon_set_user_data_destroy(GDestroyNotify func);

void fm_icon_unload_cache();

G_END_DECLS

#endif /* __FM_ICON_H__ */
