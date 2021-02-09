/*
 *      dbus-utils.h
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
 *      Copyright 2021 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

#ifndef __DBUS_UTILS_H__
#define __DBUS_UTILS_H__

#include <gio/gio.h>

G_BEGIN_DECLS

static inline char* dbus_prop_dup_str(GDBusProxy* proxy, const char* name)
{
    GVariant *var = g_dbus_proxy_get_cached_property(proxy, name);
    char *str = var ? g_variant_dup_string(var, NULL) : NULL;
    if (var) g_variant_unref(var);
    return str;
}

static inline char** dbus_prop_dup_strv(GDBusProxy* proxy, const char* name)
{
    GVariant *var = g_dbus_proxy_get_cached_property(proxy, name);
    char **strv = var ? g_variant_dup_bytestring_array(var, NULL) : NULL;
    if (var) g_variant_unref(var);
    return strv;
}

static inline gboolean dbus_prop_bool(GDBusProxy* proxy, const char* name)
{
    GVariant *var = g_dbus_proxy_get_cached_property(proxy, name);
    gboolean val = var ? g_variant_get_boolean(var) : FALSE;
    if (var) g_variant_unref(var);
    return val;
}

static inline guint dbus_prop_uint(GDBusProxy* proxy, const char* name)
{
    GVariant *var = g_dbus_proxy_get_cached_property(proxy, name);
    guint val = var ? g_variant_get_uint32(var) : 0;
    if (var) g_variant_unref(var);
    return val;
}

GError* g_udisks_error_to_gio_error(GError* error);

G_END_DECLS

#endif /* __DBUS_UTILS_H__ */
