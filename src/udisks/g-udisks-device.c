//      g-udisks-device.c
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

#include "g-udisks-device.h"
#include "dbus-utils.h"
#include "udisks-device.h"

static void g_udisks_device_finalize            (GObject *object);

G_DEFINE_TYPE(GUDisksDevice, g_udisks_device, G_TYPE_OBJECT)


static void g_udisks_device_class_init(GUDisksDeviceClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = g_udisks_device_finalize;
}


static void g_udisks_device_finalize(GObject *object)
{
    GUDisksDevice *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_DEVICE(object));

    self = G_UDISKS_DEVICE(object);

    g_free(self->obj_path);
    g_free(self->dev_file);
    g_free(self->dev_file_presentation);
    g_free(self->name);
    g_free(self->icon_name);
    g_free(self->usage);
    g_free(self->type);
    g_free(self->uuid);
    g_free(self->label);
    g_free(self->vender);
    g_free(self->model);
    g_free(self->conn_iface);
    g_free(self->media);

    g_strfreev(self->mount_paths);

    G_OBJECT_CLASS(g_udisks_device_parent_class)->finalize(object);
}


static void g_udisks_device_init(GUDisksDevice *self)
{
}


GUDisksDevice *g_udisks_device_new(const char* obj_path, GHashTable* props)
{
    GUDisksDevice* dev = (GUDisksDevice*)g_object_new(G_TYPE_UDISKS_DEVICE, NULL);
    dev->obj_path = g_strdup(obj_path);
    dev->dev_file = dbus_prop_dup_str(props, "DeviceFile");
    dev->dev_file_presentation = dbus_prop_dup_str(props, "DeviceFilePresentation");
    dev->is_sys_internal = dbus_prop_bool(props, "DeviceIsSystemInternal");
    dev->is_removable = dbus_prop_bool(props, "DeviceIsRemovable");
    dev->is_read_only = dbus_prop_bool(props, "DeviceIsReadOnly");
    dev->is_drive = dbus_prop_bool(props, "DeviceIsDrive");
    dev->is_optic_disc = dbus_prop_bool(props, "DeviceIsOpticalDisc");
    dev->is_mounted = dbus_prop_bool(props, "DeviceIsMounted");
    dev->is_media_available = dbus_prop_bool(props, "DeviceIsMediaAvailable");
    dev->is_luks = dbus_prop_bool(props, "DeviceIsLuks");
    dev->is_luks_clear_text = dbus_prop_bool(props, "DeviceIsLuksCleartext");
    dev->is_linux_md_component = dbus_prop_bool(props, "DeviceIsLinuxMdComponent");
    dev->is_linux_md = dbus_prop_bool(props, "DeviceIsLinuxMd");
    dev->is_linux_lvm2lv = dbus_prop_bool(props, "DeviceIsLinuxLvm2LV");
    dev->is_linux_lvm2pv = dbus_prop_bool(props, "DeviceIsLinuxLvm2PV");
    dev->is_linux_dmmp_component = dbus_prop_bool(props, "DeviceIsLinuxDmmpComponent");
    dev->is_linux_dmmp = dbus_prop_bool(props, "DeviceIsLinuxDmmp");

    dev->is_ejectable = dbus_prop_bool(props, "DriveIsMediaEjectable");
    dev->is_disc_blank = dbus_prop_bool(props, "OpticalDiscIsBlank");

    dev->is_hidden = dbus_prop_bool(props, "DevicePresentationHide");
    dev->auto_mount = dbus_prop_bool(props, "DevicePresentationNopolicy");

    dev->mounted_by_uid = dbus_prop_uint(props, "DeviceMountedByUid");
//    dev->mount_paths = dbus_prop_strv(props, "DeviceMountPaths");
    dev->dev_size = dbus_prop_uint64(props, "DeviceSize");
    dev->partition_size = dbus_prop_uint64(props, "PartitionSize");

    dev->luks_unlocked_by_uid = dbus_prop_uint(props, "LuksCleartextUnlockedByUid");
//    dev->num_audio_tracks = dbus_prop_uint(props, "OpticalDiscNumAudioTracks");

    dev->name = dbus_prop_dup_str(props, "DevicePresentationName");
    dev->icon_name = dbus_prop_dup_str(props, "DevicePresentationIconName");

    dev->usage = dbus_prop_dup_str(props, "IdUsage");
    dev->type = dbus_prop_dup_str(props, "IdType");
    dev->uuid = dbus_prop_dup_str(props, "IdUuid");
    dev->label = dbus_prop_dup_str(props, "IdLabel");
    dev->vender = dbus_prop_dup_str(props, "DriveVendor");
    dev->model = dbus_prop_dup_str(props, "DriveModel");
    dev->conn_iface = dbus_prop_dup_str(props, "DriveConnectionInterface");
    dev->media = dbus_prop_dup_str(props, "DriveMedia");

    /* we need to handle partiton slaves */

    /* how to use LUKS? */
/*
    'LuksHolder'                              read      'o'
    'LuksCleartextSlave'                      read      'o'
    'PartitionSlave'                          read      'o'
*/
    return dev;
}

