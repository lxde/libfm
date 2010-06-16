//      g-udisks-drive.c
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

#include "g-udisks-drive.h"


static void g_udisks_drive_drive_iface_init (GVolumeIface * iface);
static void g_udisks_drive_finalize            (GObject *object);

G_DEFINE_TYPE_EXTENDED (GUDisksDrive, g_udisks_drive, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_DRIVE,
                                               g_udisks_drive_drive_iface_init))


static void g_udisks_drive_class_init(GUDisksDriveClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = g_udisks_drive_finalize;
}

void g_udisks_drive_drive_iface_init (GVolumeIface * iface)
{
/*
    iface->get_name = g_gdu_drive_get_name;
    iface->get_icon = g_gdu_drive_get_icon;
    iface->has_volumes = g_gdu_drive_has_volumes;
    iface->get_volumes = g_gdu_drive_get_volumes;
    iface->is_media_removable = g_gdu_drive_is_media_removable;
    iface->has_media = g_gdu_drive_has_media;
    iface->is_media_check_automatic = g_gdu_drive_is_media_check_automatic;
    iface->can_eject = g_gdu_drive_can_eject;
    iface->can_poll_for_media = g_gdu_drive_can_poll_for_media;
    iface->eject = g_gdu_drive_eject;
    iface->eject_finish = g_gdu_drive_eject_finish;
    iface->eject_with_operation = g_gdu_drive_eject_with_operation;
    iface->eject_with_operation_finish = g_gdu_drive_eject_with_operation_finish;
    iface->poll_for_media = g_gdu_drive_poll_for_media;
    iface->poll_for_media_finish = g_gdu_drive_poll_for_media_finish;
    iface->get_identifier = g_gdu_drive_get_identifier;
    iface->enumerate_identifiers = g_gdu_drive_enumerate_identifiers;

    iface->get_start_stop_type = g_gdu_drive_get_start_stop_type;
    iface->can_start = g_gdu_drive_can_start;
    iface->can_start_degraded = g_gdu_drive_can_start_degraded;
    iface->can_stop = g_gdu_drive_can_stop;
    iface->start = g_gdu_drive_start;
    iface->start_finish = g_gdu_drive_start_finish;
    iface->stop = g_gdu_drive_stop;
    iface->stop_finish = g_gdu_drive_stop_finish;
*/
}

static void g_udisks_drive_finalize(GObject *object)
{
    GUDisksDrive *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_DRIVE(object));

    self = G_UDISKS_DRIVE(object);
    if(self->dev)
        g_object_unref(self->dev);

    G_OBJECT_CLASS(g_udisks_drive_parent_class)->finalize(object);
}


static void g_udisks_drive_init(GUDisksDrive *self)
{

}


GDrive *g_udisks_drive_new(GUDisksDevice* dev)
{
    GUDisksDrive* drv = (GUDisksDrive*)g_object_new(G_TYPE_UDISKS_DRIVE, NULL);
    drv->dev = g_object_ref(dev);

    return (GDrive*)drv;
}

