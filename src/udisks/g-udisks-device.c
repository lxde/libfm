//      g-udisks-device.c
//
//      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//      Copyright 2021 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "g-udisks-mount.h"
#include "g-udisks-drive.h"
#include "dbus-utils.h"

#include <string.h>

struct _GUDisksDevice
{
    GObject parent;
    char* obj_path; /* dbus object path */
    GDBusProxy *proxy; /* dbus proxy for org.freedesktop.UDisks2.Block */
    GDBusProxy *fsproxy; /* dbus proxy for org.freedesktop.UDisks2.Filesystem */

    gboolean is_sys_internal : 1;
    gboolean is_hidden : 1;
    gboolean auto_mount : 1;

    char** mount_paths;

    GVolume *volume;
    GDrive *drive;
};

struct _GUDisksDeviceClass
{
    GObjectClass parent_class;
    void (*changed)(GUDisksDevice* dev);
    void (*mount_added)(GUDisksDevice* dev, GUDisksMount* mnt);
    void (*mount_preunmount)(GUDisksDevice* dev, GUDisksMount* mnt);
    void (*mount_removed)(GUDisksDevice* dev, GUDisksMount* mnt);
};

static guint sig_changed;
static guint sig_mount_added;
static guint sig_mount_preunmount;
static guint sig_mount_removed;

static void g_udisks_device_finalize            (GObject *object);

G_DEFINE_TYPE(GUDisksDevice, g_udisks_device, G_TYPE_OBJECT)


static void g_udisks_device_class_init(GUDisksDeviceClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = g_udisks_device_finalize;

    sig_changed = g_signal_new("changed", G_TYPE_FROM_CLASS(klass),
                               G_SIGNAL_RUN_FIRST,
                               G_STRUCT_OFFSET (GUDisksDeviceClass, changed),
                               NULL, NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    sig_mount_added = g_signal_new("mount-added", G_TYPE_FROM_CLASS(klass),
                                   G_SIGNAL_RUN_FIRST,
                                   G_STRUCT_OFFSET (GUDisksDeviceClass, mount_added),
                                   NULL, NULL,
                                   g_cclosure_marshal_VOID__OBJECT,
                                   G_TYPE_NONE, 1, G_TYPE_OBJECT);

    sig_mount_preunmount = g_signal_new("mount-pre-unmount", G_TYPE_FROM_CLASS(klass),
                                        G_SIGNAL_RUN_FIRST,
                                        G_STRUCT_OFFSET (GUDisksDeviceClass, mount_preunmount),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE, 1, G_TYPE_OBJECT);

    sig_mount_removed = g_signal_new("mount-removed", G_TYPE_FROM_CLASS(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (GUDisksDeviceClass, mount_removed),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__OBJECT,
                                     G_TYPE_NONE, 1, G_TYPE_OBJECT);
}

static void clear_props(GUDisksDevice* dev)
{
    g_strfreev(dev->mount_paths);
}

static void set_props(GUDisksDevice* dev)
{
    dev->is_sys_internal = dbus_prop_bool(dev->proxy, "HintSystem");//
    dev->is_hidden = dbus_prop_bool(dev->proxy, "HintIgnore");//
    dev->auto_mount = dbus_prop_bool(dev->proxy, "HintAuto");//

    if (dev->fsproxy)
        dev->mount_paths = dbus_prop_dup_strv(dev->fsproxy, "MountPoints");
    else
        dev->mount_paths = NULL;

    if (dev->volume)
        g_udisks_volume_set_mounts(G_UDISKS_VOLUME(dev->volume), dev->mount_paths);
}

static void g_udisks_device_changed(GDBusProxy *proxy, GVariant *changed_properties,
                                    GStrv invalidated_properties, gpointer user_data)
{
    g_return_if_fail(G_IS_UDISKS_DEVICE(user_data));
    GUDisksDevice* dev = G_UDISKS_DEVICE(user_data);

    clear_props(dev);
    set_props(dev);
    g_signal_emit(dev, sig_changed, 0);
}

static void g_udisks_device_finalize(GObject *object)
{
    GUDisksDevice *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_DEVICE(object));

    self = G_UDISKS_DEVICE(object);

    if (self->proxy)
    {
        g_object_unref(self->proxy);
        g_signal_handlers_disconnect_by_func(self->proxy, G_CALLBACK(g_udisks_device_changed), self);
    }
    if (self->fsproxy)
    {
        g_object_unref(self->fsproxy);
        g_signal_handlers_disconnect_by_func(self->fsproxy, G_CALLBACK(g_udisks_device_changed), self);
    }

    g_free(self->obj_path);
    clear_props(self);

    if (self->volume)
        g_object_unref(self->volume);
    if (self->drive)
        g_object_unref(self->drive);

    G_OBJECT_CLASS(g_udisks_device_parent_class)->finalize(object);
}


static void g_udisks_device_init(GUDisksDevice *self)
{
}


GUDisksDevice *g_udisks_device_new(const char* obj_path, GDBusConnection* con,
                                   GCancellable* cancellable, GError** error)
{
    GUDisksDevice* dev = (GUDisksDevice*)g_object_new(G_TYPE_UDISKS_DEVICE, NULL);
    dev->obj_path = g_strdup(obj_path);
    dev->proxy = g_dbus_proxy_new_sync(con, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                       "org.freedesktop.UDisks2", obj_path,
                                       "org.freedesktop.UDisks2.Block",
                                       cancellable, error);
    if (dev->proxy)
    {
        g_object_ref_sink(dev->proxy);
        dev->fsproxy = g_dbus_proxy_new_sync(con, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                             "org.freedesktop.UDisks2", obj_path,
                                             "org.freedesktop.UDisks2.Filesystem",
                                             cancellable, error);
        set_props(dev);
        g_signal_connect(dev->proxy, "g-properties-changed",
                         G_CALLBACK(g_udisks_device_changed), dev);
        if (dev->fsproxy)
        {
            g_object_ref_sink(dev->fsproxy);
            g_signal_connect(dev->fsproxy, "g-properties-changed",
                             G_CALLBACK(g_udisks_device_changed), dev);
        }
    }
    return dev;
}

void g_udisks_device_set_drive(GUDisksDevice* dev, GDrive* drv)
{
    if (dev->drive)
        g_object_unref(dev->drive);
    dev->drive = drv ? g_object_ref_sink(drv) : NULL;
}

GDrive *g_udisks_device_get_drive(GUDisksDevice* dev)
{
    return dev->drive ? g_object_ref(dev->drive) : NULL;
}

void g_udisks_device_set_volume(GUDisksDevice* dev, GVolume* volume)
{
    GUDisksVolume *vol = G_UDISKS_VOLUME(volume);
    if (volume == dev->volume)
        return;
    if (dev->volume)
        g_object_unref(dev->volume);
    dev->volume = volume ? g_object_ref_sink(volume) : NULL;
    if (vol)
        g_udisks_volume_set_mounts(vol, dev->mount_paths);
}

GVariant *g_udisks_device_get_fstype(GUDisksDevice* dev)
{
    GVariant *var = g_dbus_proxy_get_cached_property(dev->proxy, "IdType");
    return var ? var : g_variant_ref_sink(g_variant_new_string("auto"));
}

char *g_udisks_device_get_uuid(GUDisksDevice* dev)
{
    return dbus_prop_dup_str(dev->proxy, "IdUUID");
}

char *g_udisks_device_get_label(GUDisksDevice* dev)
{
    return dbus_prop_dup_str(dev->proxy, "IdLabel");
}

char *g_udisks_device_get_dev_file(GUDisksDevice* dev)
{
    return dbus_prop_dup_str(dev->proxy, "Device");
}

char *g_udisks_device_get_dev_basename(GUDisksDevice* dev)
{
    GVariant *var = g_dbus_proxy_get_cached_property(dev->proxy, "PreferredDevice");
    char *basename;

    if (!var)
        var = g_dbus_proxy_get_cached_property(dev->proxy, "Device");
    if (var)
    {
        basename = g_path_get_basename(g_variant_get_bytestring(var));
        g_variant_unref(var);
    }
    else
        basename = g_path_get_basename(dev->obj_path);

    return basename;
}

char *g_udisks_device_get_icon_name(GUDisksDevice* dev)
{
    // FIXME: check for HintSymbolicIconName if it's not blank
    return dbus_prop_dup_str(dev->proxy, "HintIconName");
}

char *g_udisks_device_get_drive_obj_path(GUDisksDevice* dev)
{
    return dbus_prop_dup_str(dev->proxy, "Drive");
}

const char *g_udisks_device_get_obj_path(GUDisksDevice* dev)
{
    return dev->obj_path;
}

gboolean g_udisks_device_is_sys_internal(GUDisksDevice* dev)
{
    return dev->is_sys_internal;
}

gboolean g_udisks_device_is_hidden(GUDisksDevice* dev)
{
    return dev->is_hidden;
}

gboolean g_udisks_device_can_auto_mount(GUDisksDevice* dev)
{
    return dev->auto_mount;
}

GVolume *g_udisks_device_get_volume(GUDisksDevice* dev)
{
    return dev->volume ? g_object_ref(dev->volume) : NULL;
}

GDBusProxy *g_udisks_device_get_fs_proxy(GUDisksDevice* dev)
{
    return dev->fsproxy ? g_object_ref(dev->fsproxy) : NULL;
}

void g_udisks_device_mount_added(GUDisksDevice* dev, GUDisksMount* mnt)
{
    g_signal_emit(dev, sig_mount_added, 0, mnt);
}

void g_udisks_device_mount_preunmount(GUDisksDevice* dev, GUDisksMount* mnt)
{
    g_signal_emit(dev, sig_mount_preunmount, 0, mnt);
}

void g_udisks_device_mount_removed(GUDisksDevice* dev, GUDisksMount* mnt)
{
    g_signal_emit(dev, sig_mount_removed, 0, mnt);
}
