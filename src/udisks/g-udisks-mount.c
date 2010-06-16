//      g-udisks-mount.c
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

#include "g-udisks-mount.h"

static void g_udisks_mount_mount_iface_init(GMountIface *iface);
static void g_udisks_mount_finalize            (GObject *object);

G_DEFINE_TYPE_EXTENDED (GUDisksMount, g_udisks_mount, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_MOUNT,
                                               g_udisks_mount_mount_iface_init))


static void g_udisks_mount_class_init(GUDisksMountClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = g_udisks_mount_finalize;
}

static void g_udisks_mount_finalize(GObject *object)
{
    GUDisksMount *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_MOUNT(object));

    self = G_UDISKS_MOUNT(object);

    G_OBJECT_CLASS(g_udisks_mount_parent_class)->finalize(object);
}


static void g_udisks_mount_init(GUDisksMount *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
        G_TYPE_UDISKS_MOUNT, GUDisksMountPrivate);

}


GMount *g_udisks_mount_new(void)
{
    return g_object_new(G_TYPE_UDISKS_MOUNT, NULL);
}

static gboolean g_udisks_mount_can_eject (GMount* base)
{

}

static gboolean g_udisks_mount_can_unmount (GMount* base)
{

}

static void g_udisks_mount_eject_data_free (gpointer _data)
{

}

static void g_udisks_mount_eject (GMount* base, GMountUnmountFlags flags, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer _user_data_)
{

}

static void udisks_mount_eject_ready (GObject* source_object, GAsyncResult* _res_, gpointer _user_data_)
{

}


static void g_udisks_mount_eject_with_operation_data_free (gpointer _data)
{

}

static void g_udisks_mount_eject_with_operation (GMount* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer _user_data_)
{

}

static void udisks_mount_eject_with_operation_ready (GObject* source_object, GAsyncResult* _res_, gpointer _user_data_)
{

}

static GDrive* g_udisks_mount_get_drive (GMount* base)
{

}

static GIcon* g_udisks_mount_get_icon (GMount* base)
{

}

static char* g_udisks_mount_get_name (GMount* base)
{

}

static GFile* g_udisks_mount_get_root (GMount* base)
{

}

static char* g_udisks_mount_get_uuid (GMount* base)
{

}

static GVolume* g_udisks_mount_get_volume (GMount* base)
{

}

static void g_udisks_mount_guess_content_type_data_free (gpointer _data)
{

}

static void g_udisks_mount_guess_content_type (GMount* base, gboolean force_rescan, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer _user_data_)
{

}

static void udisks_mount_guess_content_type_ready (GObject* source_object, GAsyncResult* _res_, gpointer _user_data_)
{

}


static char** g_udisks_mount_guess_content_type_sync (GMount* base, gboolean force_rescan, GCancellable* cancellable, int* result_length1, GError** error)
{

}

static void g_udisks_mount_remount_data_free (gpointer _data)
{

}

static void g_udisks_mount_remount (GMount* base, GMountMountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer _user_data_)
{

}

static void udisks_mount_remount_ready (GObject* source_object, GAsyncResult* _res_, gpointer _user_data_)
{

}

static void g_udisks_mount_unmount_data_free (gpointer _data)
{

}

static void g_udisks_mount_unmount (GMount* base, GMountUnmountFlags flags, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer _user_data_)
{

}

static void udisks_mount_unmount_ready (GObject* source_object, GAsyncResult* _res_, gpointer _user_data_)
{

}


static void g_udisks_mount_unmount_with_operation_data_free (gpointer _data)
{

}

static void g_udisks_mount_unmount_with_operation (GMount* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback _callback_, gpointer _user_data_)
{

}

static void udisks_mount_unmount_with_operation_ready (GObject* source_object, GAsyncResult* _res_, gpointer _user_data_)
{

}

void g_udisks_mount_mount_iface_init(GMountIface *iface)
{
    iface->get_root = g_udisks_mount_get_root;
    iface->get_name = g_udisks_mount_get_name;
    iface->get_icon = g_udisks_mount_get_icon;
    iface->get_uuid = g_udisks_mount_get_uuid;
    iface->get_drive = g_udisks_mount_get_drive;
    iface->get_volume = g_udisks_mount_get_volume;
    iface->can_unmount = g_udisks_mount_can_unmount;
    iface->can_eject = g_udisks_mount_can_eject;
    iface->unmount = g_udisks_mount_unmount;
    // iface->unmount_finish = g_udisks_mount_unmount_finish;
    iface->unmount_with_operation = g_udisks_mount_unmount_with_operation;
    // iface->unmount_with_operation_finish = g_udisks_mount_unmount_with_operation_finish;
    iface->eject = g_udisks_mount_eject;
    // iface->eject_finish = g_udisks_mount_eject_finish;
    iface->eject_with_operation = g_udisks_mount_eject_with_operation;
    // iface->eject_with_operation_finish = g_udisks_mount_eject_with_operation_finish;
    iface->guess_content_type = g_udisks_mount_guess_content_type;
    // iface->guess_content_type_finish = g_udisks_mount_guess_content_type_finish;
    iface->guess_content_type_sync = g_udisks_mount_guess_content_type_sync;
}
