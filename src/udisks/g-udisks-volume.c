//      g-udisks-volume.c
//
//      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include "g-udisks-volume.h"
#include <string.h>
#include "udisks.h"
#include "g-udisks-mount.h"

static guint sig_changed;
static guint sig_removed;

static void g_udisks_volume_volume_iface_init (GVolumeIface * iface);
static void g_udisks_volume_finalize            (GObject *object);

static void g_udisks_volume_eject_with_operation (GVolume* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data);

// static gboolean g_udisks_volume_eject_co (UdisksVolumeEjectData* data);
// static gboolean g_udisks_volume_eject_with_operation_co (UdisksVolumeEjectWithOperationData* data);

G_DEFINE_TYPE_EXTENDED (GUDisksVolume, g_udisks_volume, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_VOLUME,
                                               g_udisks_volume_volume_iface_init))


static void g_udisks_volume_class_init(GUDisksVolumeClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = g_udisks_volume_finalize;
}

static void g_udisks_volume_clear(GUDisksVolume* vol)
{
    if(vol->mount)
    {
        g_object_unref(vol->mount);
        vol->mount = NULL;
    }

    if(vol->icon)
    {
        g_object_unref(vol->icon);
        vol->icon = NULL;
    }

    if(vol->name)
    {
        g_free(vol->name);
        vol->name = NULL;
    }
}

static void g_udisks_volume_finalize(GObject *object)
{
    GUDisksVolume *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_VOLUME(object));

    self = G_UDISKS_VOLUME(object);
    if(self->dev)
        g_object_unref(self->dev);

    g_udisks_volume_clear(self);
    G_OBJECT_CLASS(g_udisks_volume_parent_class)->finalize(object);
}

static void g_udisks_volume_init(GUDisksVolume *self)
{

}


GVolume *g_udisks_volume_new(GUDisksVolumeMonitor* mon, GUDisksDevice* dev)
{
    GUDisksVolume* vol = (GUDisksVolume*)g_object_new(G_TYPE_UDISKS_VOLUME, NULL);
    vol->dev = g_object_ref(dev);
    vol->mon = mon;
    return (GVolume*)vol;
}

static gboolean g_udisks_volume_can_eject (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return vol->dev->is_ejectable;
}

static gboolean g_udisks_volume_can_mount (GVolume* base)
{
    /* TODO: FIXME, is this correct? */
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return !vol->dev->is_mounted;
}

static void g_udisks_volume_eject (GVolume* base, GMountUnmountFlags flags, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    g_udisks_volume_eject_with_operation(base, flags, NULL, cancellable, callback, user_data);
}

static void g_udisks_volume_eject_with_operation (GVolume* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    if(vol->drive && g_drive_can_eject(vol->drive))
        g_drive_eject(G_DRIVE(vol->drive), flags, cancellable, callback, user_data);
#if 0
    DBusGProxy* proxy = dbus_g_proxy_new_for_name(vol->mon->con,
                            "org.freedesktop.UDisks",
                            vol->dev->obj_path,
                            "org.freedesktop.UDisks.Device");
//    org_freedesktop_UDisks_Device_drive_eject_async(proxy, NULL, cb, vol);
    g_object_unref(proxy);
#endif
}

static char** g_udisks_volume_enumerate_identifiers (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    char** kinds = g_new0(char*, 4);
    kinds[0] = g_strdup(G_VOLUME_IDENTIFIER_KIND_LABEL);
    kinds[1] = g_strdup(G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    kinds[2] = g_strdup(G_VOLUME_IDENTIFIER_KIND_UUID);
    kinds[3] = NULL;
    return kinds;
}

static GFile* g_udisks_volume_get_activation_root (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    /* FIXME: is this corrcet? */
    return vol->mount ? g_mount_get_root(vol->mount) : NULL;
}

static GDrive* g_udisks_volume_get_drive (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return vol->drive ? (GDrive*)g_object_ref(vol->drive) : NULL;
}

static GIcon* g_udisks_volume_get_icon (GVolume* base)
{
    /* TODO */
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    if(!vol->icon)
    {
        /* FIXME: this is for testing only, need to be properly set later */
        vol->icon = g_themed_icon_new("drive-harddisk");
    }
    return (GIcon*)g_object_ref(vol->icon);
}

static char* g_udisks_volume_get_identifier (GVolume* base, const char* kind)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    if(kind)
    {
        if(strcmp(kind, G_VOLUME_IDENTIFIER_KIND_LABEL) == 0)
            return g_strdup(vol->dev->label);
        else if(strcmp(kind, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE) == 0)
            return g_strdup(vol->dev->dev_file);
        else if(strcmp(kind, G_VOLUME_IDENTIFIER_KIND_UUID) == 0)
            return g_strdup(vol->dev->uuid);
    }
    return NULL;
}

static GMount* g_udisks_volume_get_mount (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    if(vol->dev->is_mounted && !vol->mount)
    {
        /* FIXME: will this work? */
        vol->mount = g_udisks_mount_new(vol);
    }
    return vol->mount ? (GMount*)g_object_ref(vol->mount) : NULL;
}

static char* g_udisks_volume_get_name (GVolume* base)
{
    /* TODO */
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    if(!vol->name)
    {
        GUDisksDevice* dev = vol->dev;
        /* build name */

        /* FIXME: find a better way to build a nicer volume name */
        if(dev->label && *dev->label)
            vol->name = g_strdup(dev->label);
        else if(dev->dev_file_presentation && *dev->dev_file_presentation)
            vol->name = g_path_get_basename(dev->dev_file_presentation);
        else if(dev->dev_file && *dev->dev_file)
            vol->name = g_path_get_basename(vol->dev->dev_file);
        else
        {
            vol->name = g_strdup(vol->dev->obj_path);
        }
    }
    return g_strdup(vol->name);
}

static char* g_udisks_volume_get_uuid (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return g_strdup(vol->dev->uuid);
}

static void g_udisks_volume_mount_fn (GVolume* base, GMountMountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, void* callback_target)
{
    /* TODO */
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
}

static gboolean g_udisks_volume_should_automount (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return vol->dev->auto_mount;
}

static gboolean g_udisks_volume_eject_finish (GVolume* base, GAsyncResult* res, GError** error)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return FALSE;
}

static gboolean g_udisks_volume_eject_with_operation_finish (GVolume* base, GAsyncResult* res, GError** error)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
/*
    gboolean result;
    UdisksVolumeEjectWithOperationData* _data_;
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error)) {
        return FALSE;
    }
    _data_ = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));
    result = _data_->result;
    return result;
*/
    return FALSE;
}


static void g_udisks_volume_volume_iface_init (GVolumeIface * iface)
{
//    g_udisks_volume_parent_iface = g_type_interface_peek_parent (iface);
    iface->can_eject = g_udisks_volume_can_eject;
    iface->can_mount = g_udisks_volume_can_mount;
    iface->eject = g_udisks_volume_eject;
    iface->eject_finish = g_udisks_volume_eject_finish;
    iface->eject_with_operation = g_udisks_volume_eject_with_operation;
    iface->eject_with_operation_finish = g_udisks_volume_eject_with_operation_finish;
    iface->enumerate_identifiers = g_udisks_volume_enumerate_identifiers;
    iface->get_activation_root = g_udisks_volume_get_activation_root;
    iface->get_drive = g_udisks_volume_get_drive;
    iface->get_icon = g_udisks_volume_get_icon;
    iface->get_identifier = g_udisks_volume_get_identifier;
    iface->get_mount = g_udisks_volume_get_mount;
    iface->get_name = g_udisks_volume_get_name;
    iface->get_uuid = g_udisks_volume_get_uuid;
    iface->mount_fn = g_udisks_volume_mount_fn;
    iface->should_automount = g_udisks_volume_should_automount;

    sig_changed = g_signal_lookup("changed", G_TYPE_VOLUME);
    sig_removed = g_signal_lookup("removed", G_TYPE_VOLUME);
}

void g_udisks_volume_changed(GUDisksVolume* vol)
{
    g_udisks_volume_clear(vol);
    g_signal_emit(vol, sig_changed, 0);
}

void g_udisks_volume_removed(GUDisksVolume* vol)
{
    vol->drive = NULL;
    g_signal_emit(vol, sig_removed, 0);
}
