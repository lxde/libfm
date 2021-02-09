//      g-udisks-device.h
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


#ifndef __G_UDISKS_DEVICE_H__
#define __G_UDISKS_DEVICE_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS


#define G_TYPE_UDISKS_DEVICE                (g_udisks_device_get_type())
#define G_UDISKS_DEVICE(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            G_TYPE_UDISKS_DEVICE, GUDisksDevice))
#define G_UDISKS_DEVICE_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass),\
            G_TYPE_UDISKS_DEVICE, GUDisksDeviceClass))
#define G_IS_UDISKS_DEVICE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            G_TYPE_UDISKS_DEVICE))
#define G_IS_UDISKS_DEVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),\
            G_TYPE_UDISKS_DEVICE))
#define G_UDISKS_DEVICE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),\
            G_TYPE_UDISKS_DEVICE, GUDisksDeviceClass))

typedef struct _GUDisksDevice            GUDisksDevice;
typedef struct _GUDisksDeviceClass        GUDisksDeviceClass;


GType g_udisks_device_get_type (void);

GUDisksDevice* g_udisks_device_get(const char* obj_path, GDBusConnection* con,
                                   GCancellable* cancellable, GError** error);

GVariant *g_udisks_device_get_fstype(GUDisksDevice* dev);
char *g_udisks_device_get_uuid(GUDisksDevice* dev);
char *g_udisks_device_get_label(GUDisksDevice* dev);
char *g_udisks_device_get_dev_file(GUDisksDevice* dev);
char *g_udisks_device_get_dev_basename(GUDisksDevice* dev);
char *g_udisks_device_get_icon_name(GUDisksDevice* dev);
char **g_udisks_device_get_mount_paths(GUDisksDevice* dev);
gboolean g_udisks_device_can_auto_mount(GUDisksDevice* dev);
const char *g_udisks_device_get_obj_path(GUDisksDevice* dev);
GDBusProxy *g_udisks_device_get_fs_proxy(GUDisksDevice* dev);

G_END_DECLS

#endif /* __G_UDISKS_DEVICE_H__ */
