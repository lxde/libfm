//      g-udisks-drive.h
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


#ifndef __G_UDISKS_DRIVE_H__
#define __G_UDISKS_DRIVE_H__

#include <gio/gio.h>

G_BEGIN_DECLS


#define G_TYPE_UDISKS_DRIVE                (g_udisks_drive_get_type())
#define G_UDISKS_DRIVE(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            G_TYPE_UDISKS_DRIVE, GUDisksDrive))
#define G_UDISKS_DRIVE_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass),\
            G_TYPE_UDISKS_DRIVE, GUDisksDriveClass))
#define G_IS_UDISKS_DRIVE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            G_TYPE_UDISKS_DRIVE))
#define G_IS_UDISKS_DRIVE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),\
            G_TYPE_UDISKS_DRIVE))
#define G_UDISKS_DRIVE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),\
            G_TYPE_UDISKS_DRIVE, GUDisksDriveClass))

typedef struct _GUDisksDrive            GUDisksDrive;
typedef struct _GUDisksDriveClass        GUDisksDriveClass;

struct _GUDisksDriveClass
{
    GObjectClass parent_class;
};


GType        g_udisks_drive_get_type(void);
GUDisksDrive* g_udisks_drive_new(GDBusConnection* con, const char *obj_path,
                                 GCancellable* cancellable, GError** error);

void g_udisks_drive_set_device_path(GUDisksDrive* drv, const char* obj_path,
                                    GDBusConnection* con, GCancellable* cancellable,
                                    GError** error);

void g_udisks_drive_disconnected(GUDisksDrive* drv);

const char* g_udisks_drive_get_obj_path(GUDisksDrive* drv);

/* this is only valid if the device contains a optic disc */
const char* g_udisks_drive_get_disc_name(GUDisksDrive* drv);

gboolean g_udisks_drive_is_disc_blank(GUDisksDrive* drv);

G_END_DECLS

#endif /* __G_UDISKS_DRIVE_H__ */
