//      g-udisks-volume.h
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


#ifndef __G_UDISKS_VOLUME_H__
#define __G_UDISKS_VOLUME_H__

#include <gio/gio.h>
#include "g-udisks-device.h"

G_BEGIN_DECLS


#define G_TYPE_UDISKS_VOLUME                (g_udisks_volume_get_type())
#define G_UDISKS_VOLUME(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            G_TYPE_UDISKS_VOLUME, GUDisksVolume))
#define G_UDISKS_VOLUME_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass),\
            G_TYPE_UDISKS_VOLUME, GUDisksVolumeClass))
#define G_IS_UDISKS_VOLUME(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            G_TYPE_UDISKS_VOLUME))
#define G_IS_UDISKS_VOLUME_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),\
            G_TYPE_UDISKS_VOLUME))
#define G_UDISKS_VOLUME_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),\
            G_TYPE_UDISKS_VOLUME, GUDisksVolumeClass))

typedef struct _GUDisksVolume            GUDisksVolume;
typedef struct _GUDisksVolumeClass        GUDisksVolumeClass;

struct _GUDisksVolumeClass
{
    GObjectClass parent_class;
};


GType        g_udisks_volume_get_type(void);
GUDisksVolume* g_udisks_volume_new(GUDisksDevice* dev, GFile* activation_root);

gboolean g_udisks_volume_set_mounts(GUDisksVolume* vol, char **mount_points);
GList *g_udisks_volume_get_mounts(GUDisksVolume* vol);

GUDisksDevice *g_udisks_volume_get_device(GUDisksVolume* vol);

void g_udisks_volume_changed(GUDisksVolume* vol);
void g_udisks_volume_removed(GUDisksVolume* vol);

G_END_DECLS

#endif /* __G_UDISKS_VOLUME_H__ */
