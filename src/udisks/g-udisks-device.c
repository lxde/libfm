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
};

struct _GUDisksDeviceClass
{
    GObjectClass parent_class;
    void (*changed)(GUDisksDevice* dev);
};

static guint sig_changed;

static GHashTable *dev_hash;

static void g_udisks_device_finalize            (GObject *object);

G_DEFINE_TYPE(GUDisksDevice, g_udisks_device, G_TYPE_OBJECT)


static void g_udisks_device_class_init(GUDisksDeviceClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = g_udisks_device_finalize;

    dev_hash = g_hash_table_new(g_str_hash, g_str_equal);

    sig_changed = g_signal_new("changed", G_TYPE_FROM_CLASS(klass),
                               G_SIGNAL_RUN_FIRST,
                               G_STRUCT_OFFSET (GUDisksDeviceClass, changed),
                               NULL, NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);
}

static void g_udisks_device_changed(GDBusProxy *proxy, GVariant *changed_properties,
                                    GStrv invalidated_properties, gpointer user_data)
{
    g_return_if_fail(G_IS_UDISKS_DEVICE(user_data));
    GUDisksDevice* dev = G_UDISKS_DEVICE(user_data);

    g_signal_emit(dev, sig_changed, 0);
}

static void g_udisks_device_finalize(GObject *object)
{
    GUDisksDevice *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_DEVICE(object));

    self = G_UDISKS_DEVICE(object);

    g_hash_table_remove(dev_hash, self->obj_path);

    if (self->proxy)
    {
        g_signal_handlers_disconnect_by_func(self->proxy, G_CALLBACK(g_udisks_device_changed), self);
        g_object_unref(self->proxy);
    }
    if (self->fsproxy)
    {
        g_signal_handlers_disconnect_by_func(self->fsproxy, G_CALLBACK(g_udisks_device_changed), self);
        g_object_unref(self->fsproxy);
    }

    g_free(self->obj_path);

    G_OBJECT_CLASS(g_udisks_device_parent_class)->finalize(object);
}


static void g_udisks_device_init(GUDisksDevice *self)
{
}


/**
 *
 * Returns: (transfer full): either new or existing GUDisksDevice instance.
 */
GUDisksDevice *g_udisks_device_get(const char* obj_path, GDBusConnection* con,
                                   GCancellable* cancellable, GError** error)
{
    GUDisksDevice* dev = dev_hash ? g_hash_table_lookup(dev_hash, obj_path) : NULL;

    if (dev)
        return g_object_ref(dev);

    dev = (GUDisksDevice*)g_object_new(G_TYPE_UDISKS_DEVICE, NULL);
    dev->obj_path = g_strdup(obj_path);
    g_hash_table_insert(dev_hash, dev->obj_path, dev);
    dev->proxy = g_dbus_proxy_new_sync(con, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                       "org.freedesktop.UDisks2", obj_path,
                                       "org.freedesktop.UDisks2.Block",
                                       cancellable, error);
    if (dev->proxy)
    {
        dev->fsproxy = g_dbus_proxy_new_sync(con, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                             "org.freedesktop.UDisks2", obj_path,
                                             "org.freedesktop.UDisks2.Filesystem",
                                             cancellable, error);
        g_signal_connect(dev->proxy, "g-properties-changed",
                         G_CALLBACK(g_udisks_device_changed), dev);
        if (dev->fsproxy)
            g_signal_connect(dev->fsproxy, "g-properties-changed",
                             G_CALLBACK(g_udisks_device_changed), dev);
    }
    return dev;
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

gboolean g_udisks_device_can_auto_mount(GUDisksDevice* dev)
{
    return dbus_prop_bool(dev->proxy, "HintAuto");
}

char **g_udisks_device_get_mount_paths(GUDisksDevice* dev)
{
    return dev->fsproxy ? dbus_prop_dup_strv(dev->fsproxy, "MountPoints") : NULL;
}

const char *g_udisks_device_get_obj_path(GUDisksDevice* dev)
{
    return dev->obj_path;
}

GDBusProxy *g_udisks_device_get_fs_proxy(GUDisksDevice* dev)
{
    return dev->fsproxy ? g_object_ref(dev->fsproxy) : NULL;
}
