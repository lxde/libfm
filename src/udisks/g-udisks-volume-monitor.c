/*
 *      g-udisks-volume-monitor.c
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
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

#include "g-udisks-volume-monitor.h"
#include <dbus/dbus-glib.h>

struct _GUdisksVolumeMonitor
{
    GNativeVolumeMonitor parent;
    DBusGConnection* con;
    DBusGProxy* udisks_proxy;

    GList* devices;
};


static void g_udisks_volume_monitor_finalize            (GObject *object);
static GMount* get_mount_for_mount_path(const char *mount_path, GCancellable *cancellable);

static gboolean is_supported(void);
static GList* get_connected_drives(GVolumeMonitor *volume_monitor);
static GList* get_volumes(GVolumeMonitor *volume_monitor);
static GList* get_mounts(GVolumeMonitor *volume_monitor);
static GVolume *get_volume_for_uuid(GVolumeMonitor *volume_monitor, const char *uuid);
static GMount *get_mount_for_uuid(GVolumeMonitor *volume_monitor, const char *uuid);
static void drive_eject_button(GVolumeMonitor *volume_monitor, GDrive *drive);

static void on_device_added(DBusGProxy* proxy, const char* obj_path, gpointer user_data);
static void on_device_removed(DBusGProxy* proxy, const char* obj_path, gpointer user_data);
static void on_device_changed(DBusGProxy* proxy, const char* obj_path, gpointer user_data);


G_DEFINE_TYPE(GUdisksVolumeMonitor, g_udisks_volume_monitor, G_TYPE_NATIVE_VOLUME_MONITOR);


static void g_udisks_volume_monitor_class_init(GUdisksVolumeMonitorClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GNativeVolumeMonitorClass *parent_class = G_NATIVE_VOLUME_MONITOR_CLASS(klass);
    GVolumeMonitorClass *monitor_class = G_VOLUME_MONITOR_CLASS (klass);

    g_object_class->finalize = g_udisks_volume_monitor_finalize;
    parent_class->get_mount_for_mount_path = get_mount_for_mount_path;

    monitor_class->get_mounts = get_mounts;
    monitor_class->get_volumes = get_volumes;
    monitor_class->get_connected_drives = get_connected_drives;
    monitor_class->get_volume_for_uuid = get_volume_for_uuid;
    monitor_class->get_mount_for_uuid = get_mount_for_uuid;
    monitor_class->is_supported = is_supported;
    monitor_class->drive_eject_button = drive_eject_button;
}


static void g_udisks_volume_monitor_finalize(GObject *object)
{
    GUdisksVolumeMonitor *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_VOLUME_MONITOR(object));

    self = G_UDISKS_VOLUME_MONITOR(object);
    if(self->udisks_proxy)
    {
        dbus_g_proxy_disconnect_signal(self->udisks_proxy, "DeviceAdded", G_CALLBACK(on_device_added), self);
        dbus_g_proxy_disconnect_signal(self->udisks_proxy, "DeviceRemoved", G_CALLBACK(on_device_removed), self);
        dbus_g_proxy_disconnect_signal(self->udisks_proxy, "DeviceChanged", G_CALLBACK(on_device_changed), self);
        g_object_unref(self->udisks_proxy);
    }

    G_OBJECT_CLASS(g_udisks_volume_monitor_parent_class)->finalize(object);
}


static void g_udisks_volume_monitor_init(GUdisksVolumeMonitor *self)
{
    self->con = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
    if(self->con)
    {
        DBusGProxy* proxy;
        GPtrArray* ret;
        self->udisks_proxy = dbus_g_proxy_new_for_name(self->con, "org.freedesktop.UDisks", "/org/freedesktop/UDisks", "org.freedesktop.UDisks");
        if(dbus_g_proxy_call(self->udisks_proxy, "EnumerateDevices", NULL, G_TYPE_INVALID,
                             dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH), &ret,
                             G_TYPE_INVALID))
        {
            int i;
            char** paths = (char**)ret->pdata;
            for(i=0; i<ret->len;++i)
                on_device_added(self->udisks_proxy, paths[i], self);
        }

        dbus_g_proxy_add_signal(self->udisks_proxy, "DeviceAdded", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
        dbus_g_proxy_add_signal(self->udisks_proxy, "DeviceRemoved", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
        dbus_g_proxy_add_signal(self->udisks_proxy, "DeviceChanged", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);

        dbus_g_proxy_connect_signal(self->udisks_proxy, "DeviceAdded", G_CALLBACK(on_device_added), self, NULL);
        dbus_g_proxy_connect_signal(self->udisks_proxy, "DeviceRemoved", G_CALLBACK(on_device_removed), self, NULL);
        dbus_g_proxy_connect_signal(self->udisks_proxy, "DeviceChanged", G_CALLBACK(on_device_changed), self, NULL);
    }
}


GNativeVolumeMonitor *g_udisks_volume_monitor_new(void)
{
    return g_object_new(G_UDISKS_VOLUME_MONITOR_TYPE, NULL);
}

GMount* get_mount_for_mount_path(const char *mount_path, GCancellable *cancellable)
{
    return NULL;
}


gboolean is_supported(void)
{
    return TRUE;
}

GList* get_connected_drives(GVolumeMonitor *volume_monitor)
{
    return NULL;
}

GList* get_volumes(GVolumeMonitor *volume_monitor)
{
    return NULL;
}

GList* get_mounts(GVolumeMonitor *volume_monitor)
{
    return NULL;
}

GVolume *get_volume_for_uuid(GVolumeMonitor *volume_monitor, const char *uuid)
{
    return NULL;
}

GMount *get_mount_for_uuid(GVolumeMonitor *volume_monitor, const char *uuid)
{
    return NULL;
}

/* signal added in 2.17 */
void drive_eject_button(GVolumeMonitor *volume_monitor, GDrive *drive)
{

}

static update_props()
{

}

void on_device_added(DBusGProxy* proxy, const char* obj_path, gpointer user_data)
{
    GUdisksVolumeMonitor* mon = G_UDISKS_VOLUME_MONITOR(user_data);
    DBusGProxy *dev_proxy = dbus_g_proxy_new_for_name(mon->con,
                                        "org.freedesktop.UDisks",
                                        obj_path,
                                        "org.freedesktop.DBus.Properties");
    GHashTable* props;
    GError* err = NULL;

    if(dbus_g_proxy_call(dev_proxy, "GetAll", &err,
                      G_TYPE_STRING, "org.freedesktop.UDisks.Device", G_TYPE_INVALID,
                      dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &props, G_TYPE_INVALID))
    {
        g_debug("%d properties are returned", g_hash_table_size(props));
        GValue* val = (GValue*)g_hash_table_lookup(props, "IdLabel");
        g_debug("val = %s", g_value_get_string(val));
        g_hash_table_destroy(props);
    }
    else
        g_debug("%s", err->message);

    g_object_unref(dev_proxy);

    g_debug("added");
    g_debug("%s", obj_path);

}

void on_device_removed(DBusGProxy* proxy, const char* obj_path, gpointer user_data)
{
    g_debug("device_removed");
}

void on_device_changed(DBusGProxy* proxy, const char* obj_path, gpointer user_data)
{
    g_debug("device_changed");
}
