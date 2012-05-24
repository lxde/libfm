/*
 *      gtk-compat.h
 *
 *      Copyright 2011 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#ifndef __GTK_COMPAT_H__
#define __GTK_COMPAT_H__
#include <gtk/gtk.h>
#include "glib-compat.h"

G_BEGIN_DECLS

/* for gtk+ 3.0 migration */
#if !GTK_CHECK_VERSION(3, 0, 0)
    #define gdk_display_get_app_launch_context(dpy) gdk_app_launch_context_new()
#endif

#if !GTK_CHECK_VERSION(2, 21, 0)
    #define GDK_KEY_Left    GDK_Left
    #define GDK_KEY_Right   GDK_Right
#endif

#if !GTK_CHECK_VERSION(3, 0, 0)
    #define gtk_widget_in_destruction(widget) \
        (GTK_OBJECT_FLAGS(GTK_OBJECT(widget)) & GTK_IN_DESTRUCTION)
#endif

G_END_DECLS

#endif
