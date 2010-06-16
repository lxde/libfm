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

#include "g-udisks-volume.h"

struct _GUDisksVolume
{
    GObject parent;
};

static void g_udisks_volume_volume_iface_init (GVolumeIface * iface);
static void g_udisks_volume_finalize            (GObject *object);

static gboolean g_udisks_volume_can_eject (GVolume* base);
static gboolean g_udisks_volume_can_mount (GVolume* base);
static void g_udisks_volume_eject_data_free (gpointer _data);
static void g_udisks_volume_eject (GVolume* base, GMountUnmountFlags flags, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer user_data);
static void udisks_volume_eject_ready (GObject* source_object, GAsyncResult* res, gpointer user_data);
// static gboolean g_udisks_volume_eject_co (UdisksVolumeEjectData* data);
static void g_udisks_volume_eject_with_operation_data_free (gpointer _data);
static void g_udisks_volume_eject_with_operation (GVolume* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer user_data);
static void udisks_volume_eject_with_operation_ready (GObject* source_object, GAsyncResult* res, gpointer user_data);
// static gboolean g_udisks_volume_eject_with_operation_co (UdisksVolumeEjectWithOperationData* data);
static char* g_udisks_volume_enumerate_identifiers (GVolume* base);
static GFile* g_udisks_volume_get_activation_root (GVolume* base);
static GDrive* g_udisks_volume_get_drive (GVolume* base);
static GIcon* g_udisks_volume_get_icon (GVolume* base);
static char* g_udisks_volume_get_identifier (GVolume* base, const char* kind);
static GMount* g_udisks_volume_get_mount (GVolume* base);
static char* g_udisks_volume_get_name (GVolume* base);
static char* g_udisks_volume_get_uuid (GVolume* base);
static void g_udisks_volume_mount_fn (GVolume* base, GMountMountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, void* callback_target);
static gboolean g_udisks_volume_should_automount (GVolume* base);
static gboolean g_udisks_volume_eject_finish (GVolume* base, GAsyncResult* res, GError** error);
static gboolean g_udisks_volume_eject_with_operation_finish (GVolume* base, GAsyncResult* res, GError** error);


G_DEFINE_TYPE_EXTENDED (GUDisksVolume, g_udisks_volume, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_VOLUME,
                                               g_udisks_volume_volume_iface_init))


static void g_udisks_volume_class_init(GUDisksVolumeClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = g_udisks_volume_finalize;
}


static void g_udisks_volume_finalize(GObject *object)
{
    GUDisksVolume *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_VOLUME(object));

    self = G_UDISKS_VOLUME(object);

    G_OBJECT_CLASS(g_udisks_volume_parent_class)->finalize(object);
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
}

static void g_udisks_volume_init(GUDisksVolume *self)
{

}


GVolume *g_udisks_volume_new(void)
{
    return g_object_new(G_TYPE_UDISKS_VOLUME, NULL);
}

gboolean g_udisks_volume_can_eject (GVolume* base)
{
    return FALSE;
}

gboolean g_udisks_volume_can_mount (GVolume* base)
{
    return FALSE;
}

void g_udisks_volume_eject_data_free (gpointer _data)
{

}

void g_udisks_volume_eject (GVolume* base, GMountUnmountFlags flags, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer user_data)
{

}

void udisks_volume_eject_ready (GObject* source_object, GAsyncResult* res, gpointer user_data)
{

}

// gboolean g_udisks_volume_eject_co (UdisksVolumeEjectData* data)

void g_udisks_volume_eject_with_operation (GVolume* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer user_data)
{

}

void udisks_volume_eject_with_operation_ready (GObject* source_object, GAsyncResult* res, gpointer user_data)
{

}

char* g_udisks_volume_enumerate_identifiers (GVolume* base)
{
    return NULL;
}

GFile* g_udisks_volume_get_activation_root (GVolume* base)
{
    return NULL;
}

GDrive* g_udisks_volume_get_drive (GVolume* base)
{
    return NULL;
}

GIcon* g_udisks_volume_get_icon (GVolume* base)
{
    return NULL;
}

char* g_udisks_volume_get_identifier (GVolume* base, const char* kind)
{
    return NULL;
}

GMount* g_udisks_volume_get_mount (GVolume* base)
{
    return NULL;
}

char* g_udisks_volume_get_name (GVolume* base)
{
    return NULL;
}

char* g_udisks_volume_get_uuid (GVolume* base)
{
    return NULL;
}

void g_udisks_volume_mount_fn (GVolume* base, GMountMountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, void* callback_target)
{

}

gboolean g_udisks_volume_should_automount (GVolume* base)
{
    return FALSE;
}

gboolean g_udisks_volume_eject_finish (GVolume* base, GAsyncResult* res, GError** error)
{
    return FALSE;
}

gboolean g_udisks_volume_eject_with_operation_finish (GVolume* base, GAsyncResult* res, GError** error)
{
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
