//      g-udisks-volume.c
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

#include <glib/gi18n-lib.h>
#include "g-udisks-mount.h"
#include "g-udisks-drive.h"
#include "dbus-utils.h"

#include <string.h>

struct _GUDisksVolume
{
    GObject parent;
    GUDisksDevice* dev;
    GUDisksDrive* drv;
    char* name;
    GList* mounts; /* of GUDisksMount */
    GFile* activation_root;
};

struct _GUDisksVolumeClass
{
    GObjectClass parent_class;
    void (*mount_added)(GUDisksDevice* dev, GUDisksMount* mnt);
};

typedef struct
{
    GUDisksVolume* vol;
    GAsyncReadyCallback callback;
    gpointer user_data;
}AsyncData;

static guint sig_changed;
static guint sig_removed;
static guint sig_mount_added;

static void g_udisks_volume_volume_iface_init (GVolumeIface * iface);
static void g_udisks_volume_finalize            (GObject *object);

static void g_udisks_volume_eject_with_operation (GVolume* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data);
static gboolean g_udisks_volume_set_mounts(GUDisksVolume* vol, GUDisksDevice* dev);

static void on_device_changed(GUDisksDevice* dev, GUDisksVolume* vol)
{
    g_free(vol->name);
    vol->name = NULL;
    g_udisks_volume_set_mounts(vol, dev);
    g_signal_emit(vol, sig_changed, 0);
}

G_DEFINE_TYPE_EXTENDED (GUDisksVolume, g_udisks_volume, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_VOLUME,
                                               g_udisks_volume_volume_iface_init))


static void g_udisks_volume_class_init(GUDisksVolumeClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = g_udisks_volume_finalize;

    sig_mount_added = g_signal_new("mount-added", G_TYPE_FROM_CLASS(klass),
                                   G_SIGNAL_RUN_FIRST,
                                   G_STRUCT_OFFSET (GUDisksVolumeClass, mount_added),
                                   NULL, NULL,
                                   g_cclosure_marshal_VOID__OBJECT,
                                   G_TYPE_NONE, 1, G_TYPE_OBJECT);
}

static void g_udisks_volume_finalize(GObject *object)
{
    GUDisksVolume *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_VOLUME(object));

    self = G_UDISKS_VOLUME(object);

    g_signal_handlers_disconnect_by_func(self->dev, G_CALLBACK(on_device_changed), self);
    g_object_unref(self->dev);

    /* it should be already removed from drive at this point but let ensure that */
    g_udisks_drive_del_volume(self->drv, self);
    g_object_unref(self->drv);

    while(self->mounts)
    {
        g_udisks_mount_unmounted(self->mounts->data);
        g_object_unref(self->mounts->data);
        self->mounts = g_list_delete_link(self->mounts, self->mounts);
    }

    if(self->activation_root)
        g_object_unref(self->activation_root);

    g_free(self->name);

    G_OBJECT_CLASS(g_udisks_volume_parent_class)->finalize(object);
}

static void g_udisks_volume_init(GUDisksVolume *self)
{

}

static gboolean on_idle_start(gpointer data)
{
    GUDisksVolume* vol = (GUDisksVolume*)data;
    g_udisks_volume_set_mounts(vol, vol->dev);
    g_object_unref(data);
    return FALSE;
}

GUDisksVolume *g_udisks_volume_new(const char* obj_path, GDBusConnection* con,
                                   GFile* activation_root, GUDisksDrive* drv,
                                   GCancellable* cancellable, GError** error)
{
    GUDisksVolume* vol = (GUDisksVolume*)g_object_new(G_TYPE_UDISKS_VOLUME, NULL);

    vol->dev = g_udisks_device_get(obj_path, con, cancellable, error);
    g_signal_connect(vol->dev, "changed", G_CALLBACK(on_device_changed), vol);
    vol->drv = g_object_ref(drv);
    g_udisks_drive_add_volume(drv, vol);
    if (activation_root)
        vol->activation_root = g_object_ref_sink(activation_root);
    g_idle_add(on_idle_start, g_object_ref(vol));
    return vol;
}

static gboolean g_udisks_volume_can_eject (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return g_drive_can_eject(G_DRIVE(vol->drv));
}

static gboolean g_udisks_volume_can_mount (GVolume* base)
{
    /* FIXME, is this correct? */
    //GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return TRUE;
}

static void g_udisks_volume_eject (GVolume* base, GMountUnmountFlags flags, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    g_udisks_volume_eject_with_operation(base, flags, NULL, cancellable, callback, user_data);
}

typedef struct
{
    GUDisksVolume* vol;
    GAsyncReadyCallback callback;
    gpointer user_data;
}EjectData;

static void on_drive_ejected(GObject* drive, GAsyncResult* res, gpointer user_data)
{
    EjectData* data = (EjectData*)user_data;
    if(data->callback)
        data->callback(G_OBJECT(data->vol), res, data->user_data);
    g_object_unref(data->vol);
    g_slice_free(EjectData, data);
}

static void g_udisks_volume_eject_with_operation (GVolume* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    if(g_drive_can_eject(G_DRIVE(vol->drv)))
    {
        EjectData* data = g_slice_new(EjectData);
        data->vol = g_object_ref(vol);
        data->callback = callback;
        data->user_data = user_data;
        g_drive_eject_with_operation(G_DRIVE(vol->drv), flags, mount_operation,
                                     cancellable, on_drive_ejected, data);
    }
}

static char** g_udisks_volume_enumerate_identifiers (GVolume* base)
{
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
    if(vol->mounts)
        return g_mount_get_root((GMount*)vol->mounts->data);
    return vol->activation_root ? g_object_ref(vol->activation_root) : NULL;
}

static GDrive* g_udisks_volume_get_drive (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return g_object_ref(vol->drv);
}

static GIcon* g_udisks_volume_get_icon (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return g_drive_get_icon(G_DRIVE(vol->drv));
}

static char* g_udisks_volume_get_identifier (GVolume* base, const char* kind)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    if(kind)
    {
        if(strcmp(kind, G_VOLUME_IDENTIFIER_KIND_LABEL) == 0)
            return g_udisks_device_get_label(vol->dev);
        else if(strcmp(kind, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE) == 0)
            return g_udisks_device_get_dev_file(vol->dev);
        else if(strcmp(kind, G_VOLUME_IDENTIFIER_KIND_UUID) == 0)
            return g_udisks_device_get_uuid(vol->dev);
    }
    return NULL;
}

static GMount* g_udisks_volume_get_mount (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return vol->mounts ? (GMount*)g_object_ref(vol->mounts->data) : NULL;
}

static char* g_udisks_volume_get_name (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    if(!vol->name)
    {
        /* build a human readable name */

        /* FIXME: find a better way to build a nicer volume name */
        vol->name = g_udisks_device_get_label(vol->dev);
        if(vol->name && !vol->name[0])
        {
            g_free(vol->name);
            vol->name = NULL;
        }
/*
        else if(vol->dev->partition_size > 0)
        {
            char* size_str = g_format_size_for_display(vol->dev->partition_size);
            vol->name = g_strdup_printf("%s Filesystem", size_str);
            g_free(size_str);
        }
*/
        if(!vol->name)
            vol->name = g_strdup(g_udisks_drive_get_disc_name(vol->drv));
        if(!vol->name)
            vol->name = g_udisks_device_get_dev_basename(vol->dev);
    }
    return g_strdup(vol->name);
}

static char* g_udisks_volume_get_uuid (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return g_udisks_device_get_uuid(vol->dev);
}

static void mount_callback(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GDBusProxy* proxy = (GDBusProxy *)source;
    AsyncData* data = (AsyncData*)user_data;
    GSimpleAsyncResult* res;
    GError *error = NULL;
    GVariant *val = g_dbus_proxy_call_finish(proxy, result, &error);

    g_debug("mount callback!!");
    if(error)
    {
        res = g_simple_async_result_new_from_error(G_OBJECT(data->vol),
                                                   data->callback,
                                                   data->user_data,
                                                   error);
        g_error_free(error);
    }
    else
    {
        res = g_simple_async_result_new(G_OBJECT(data->vol),
                                        data->callback,
                                        data->user_data,
                                        NULL);
        g_simple_async_result_set_op_res_gboolean(res, TRUE);
        g_variant_unref(val);
    }
    g_simple_async_result_complete(res);
    g_object_unref(res);

    g_object_unref(data->vol);
    g_slice_free(AsyncData, data);

}

static void g_udisks_volume_mount_fn(GVolume* base, GMountMountFlags flags,
                                     GMountOperation* mount_operation,
                                     GCancellable* cancellable,
                                     GAsyncReadyCallback callback, void* user_data)
{
    /* FIXME: need to make sure this works correctly */
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    GUDisksDevice* dev = vol->dev;
    GDBusProxy* proxy = g_udisks_device_get_fs_proxy(dev);

    if (proxy)
    {
        AsyncData* data = g_slice_new(AsyncData);
        GVariant* fstype = g_udisks_device_get_fstype(dev);
        GVariantBuilder b;

        g_debug("send DBus request to mount %s", g_udisks_device_get_obj_path(dev));
        data->vol = g_object_ref(vol);
        data->callback = callback;
        data->user_data = user_data;

        g_variant_builder_init(&b, G_VARIANT_TYPE("(a{sv})"));
        g_variant_builder_open(&b, G_VARIANT_TYPE ("a{sv}"));
        g_variant_builder_add(&b, "{sv}", "fstype", fstype);
        g_variant_builder_close(&b);
        g_dbus_proxy_call(proxy, "Mount", g_variant_builder_end(&b),
                          G_DBUS_CALL_FLAGS_NONE, -1, cancellable,
                          mount_callback, data);
        g_object_unref(proxy);
        g_variant_unref(fstype);
    }
    else
    {
        GSimpleAsyncResult* res;
        char *dev_file = g_udisks_device_get_dev_file(dev);

        res = g_simple_async_result_new_error(G_OBJECT(vol), callback, user_data,
                                              G_IO_ERROR, G_IO_ERROR_FAILED,
                                              _("No filesystem proxy for '%s'"),
                                              dev_file);
        g_simple_async_result_complete(res);
        g_object_unref(res);
        g_free(dev_file);
    }
}

static gboolean g_udisks_volume_mount_finish(GVolume* base, GAsyncResult* res, GError** error)
{
    return !g_simple_async_result_propagate_error(G_SIMPLE_ASYNC_RESULT(res), error);
}

static gboolean g_udisks_volume_should_automount (GVolume* base)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    return g_udisks_device_can_auto_mount(vol->dev);
}

static gboolean g_udisks_volume_eject_with_operation_finish (GVolume* base, GAsyncResult* res, GError** error)
{
    GUDisksVolume* vol = G_UDISKS_VOLUME(base);
    /* FIXME: is this correct? */
    return g_drive_eject_with_operation_finish(G_DRIVE(vol->drv), res, error);
}

static gboolean g_udisks_volume_eject_finish (GVolume* base, GAsyncResult* res, GError** error)
{
    return g_udisks_volume_eject_with_operation_finish(base, res, error);
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
    iface->mount_finish = g_udisks_volume_mount_finish;
    iface->should_automount = g_udisks_volume_should_automount;

    sig_changed = g_signal_lookup("changed", G_TYPE_VOLUME);
    sig_removed = g_signal_lookup("removed", G_TYPE_VOLUME);
}

/**
 * g_udisks_volume_set_mounts
 * @vol: volume instance
 * @dev: associated device
 *
 * Updates mounts list to actual one. Allocates and releases GUDisksMount
 * objects as needed.
 *
 * Returns: %TRUE if list was changed since last call.
 */
static gboolean g_udisks_volume_set_mounts(GUDisksVolume* vol, GUDisksDevice* dev)
{
    char **mount_points = g_udisks_device_get_mount_paths(dev);
    GList *new_list = NULL, *list;
    char **point;
    gboolean changed = FALSE;

    g_return_val_if_fail(G_IS_UDISKS_VOLUME(vol), FALSE);

    for (point = mount_points; point && *point; point++)
    {
        GFile *new_root = g_file_new_for_path(*point), *mount_root;

        for (list = vol->mounts; list; list = list->next)
        {
            mount_root = g_mount_get_root(list->data);
            if (g_file_equal(new_root, mount_root))
            {
                g_object_unref(mount_root);
                vol->mounts = g_list_remove_link(vol->mounts, list);
                new_list = g_list_concat(list, new_list);
                break;
            }
            g_object_unref(mount_root);
        }
        if (!list)
        {
            GUDisksMount *mnt = g_udisks_mount_new(vol, new_root);
            new_list = g_list_prepend(new_list, mnt);
            g_debug("adding new mount for '%s' at '%s'",
                    strrchr(g_udisks_device_get_obj_path(vol->dev), '/'),
                    *point);
            /* emit "mount-added" signal */
            g_signal_emit(vol, sig_mount_added, 0, mnt);
            changed = TRUE;
        }
        g_object_unref(new_root);
    }
    while(vol->mounts)
    {
        GUDisksMount *mnt = vol->mounts->data;
        GFile *f_root = g_mount_get_root(vol->mounts->data);
        char *c_root = g_file_get_path(f_root);
        g_debug("removing gone mount '%s'", c_root);
        g_free(c_root);
        g_object_unref(f_root);
        vol->mounts = g_list_delete_link(vol->mounts, vol->mounts);
        /* emit "mount-unmounted" signal */
        g_udisks_mount_unmounted(mnt);
        g_object_unref(mnt);
        changed = TRUE;
    }
    vol->mounts = g_list_reverse(new_list);

    g_strfreev(mount_points);
    return changed;
}

/**
 * g_udisks_volume_get_mounts
 * @vol: volume instance
 *
 * Retrieves currently mounted points.
 *
 * Returns: (element-type GUDisksMount)(transfer container): list of mounts.
 */
GList *g_udisks_volume_get_mounts(GUDisksVolume* vol)
{
    g_return_val_if_fail(G_IS_UDISKS_VOLUME(vol), NULL);
    return g_list_copy(vol->mounts);
}

/**
 * g_udisks_volume_get_device
 * @vol: volume instance
 *
 * Retrieves device which volume belongs to.
 *
 * Returns: (transfer none): GUDisksDevice instance.
 */
GUDisksDevice *g_udisks_volume_get_device(GUDisksVolume* vol)
{
    g_return_val_if_fail(G_IS_UDISKS_VOLUME(vol), NULL);
    return vol->dev;
}

const char *g_udisks_volume_get_obj_path(GUDisksVolume* vol)
{
    g_return_val_if_fail(G_IS_UDISKS_VOLUME(vol), NULL);
    return g_udisks_device_get_obj_path(vol->dev);
}

void g_udisks_volume_removed(GUDisksVolume* vol)
{
    g_return_if_fail(G_IS_UDISKS_VOLUME(vol));
    g_udisks_drive_del_volume(vol->drv, vol);
    g_signal_emit(vol, sig_removed, 0);
}
