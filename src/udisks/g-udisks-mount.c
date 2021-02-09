//      g-udisks-mount.c
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

#include <glib/gi18n-lib.h>

struct _GUDisksMount
{
    GObject parent;
    GUDisksVolume* vol; /* weak */
    GFile* root;
};

typedef struct
{
    GUDisksMount* mnt;
    GAsyncReadyCallback callback;
    GMountOperation* mount_operation;
    gpointer user_data;
}AsyncData;

static guint sig_pre_unmount;
static guint sig_unmounted;

static void g_udisks_mount_mount_iface_init(GMountIface *iface);
static void g_udisks_mount_finalize            (GObject *object);

static void g_udisks_mount_unmount_with_operation (GMount* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data);

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
    if(self->root)
        g_object_unref(self->root);

    G_OBJECT_CLASS(g_udisks_mount_parent_class)->finalize(object);
}


static void g_udisks_mount_init(GUDisksMount *self)
{
}


GUDisksMount *g_udisks_mount_new(GUDisksVolume* vol, GFile* root)
{
    g_return_val_if_fail(vol, NULL);
    g_return_val_if_fail(root, NULL);
    GUDisksMount* mnt = g_object_new(G_TYPE_UDISKS_MOUNT, NULL);
    /* we don't do g_object_ref here to prevent circular reference. */
    mnt->vol = vol;
    mnt->root = g_object_ref_sink(root);
    return mnt;
}

static gboolean g_udisks_mount_can_eject (GMount* base)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return mnt->vol ? g_volume_can_eject(G_VOLUME(mnt->vol)) : FALSE;
}

static gboolean g_udisks_mount_can_unmount (GMount* base)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return mnt->root != NULL;
}

typedef struct
{
    GUDisksMount* mnt;
    GAsyncReadyCallback callback;
    gpointer user_data;
}EjectData;

static void on_drive_ejected(GObject* drive, GAsyncResult* res, gpointer user_data)
{
    EjectData* data = (EjectData*)user_data;
    if(data->callback)
        data->callback(G_OBJECT(data->mnt), res, data->user_data);
    g_object_unref(data->mnt);
    g_slice_free(EjectData, data);
}

static void g_udisks_mount_eject_with_operation (GMount* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    GDrive* drv = g_mount_get_drive(base);
    if(drv)
    {
        EjectData* data = g_slice_new(EjectData);
        data->mnt = g_object_ref(mnt);
        data->callback = callback;
        data->user_data = user_data;
        g_drive_eject_with_operation(drv, flags, mount_operation, cancellable,
                                     on_drive_ejected, data);
        g_object_unref(drv);
    }
}

static gboolean g_udisks_mount_eject_with_operation_finish(GMount* base, GAsyncResult* res, GError** error)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return g_volume_eject_with_operation_finish(G_VOLUME(mnt->vol), res, error);
}

static void g_udisks_mount_eject (GMount* base, GMountUnmountFlags flags, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    //GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    g_udisks_mount_eject_with_operation(base, flags, NULL, cancellable, callback, user_data);
}

static gboolean g_udisks_mount_eject_finish(GMount* base, GAsyncResult* res, GError** error)
{
    //GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return g_udisks_mount_eject_with_operation_finish(base, res, error);
}

static GDrive* g_udisks_mount_get_drive (GMount* base)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return mnt->vol ? g_volume_get_drive(G_VOLUME(mnt->vol)) : NULL;
}

static GIcon* g_udisks_mount_get_icon (GMount* base)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return mnt->vol ? g_volume_get_icon(G_VOLUME(mnt->vol)) : NULL;
}

static char* g_udisks_mount_get_name (GMount* base)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return mnt->vol ? g_volume_get_name(G_VOLUME(mnt->vol)) : NULL;
}

static GFile* g_udisks_mount_get_root (GMount* base)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return mnt->root ? (GFile*)g_object_ref(mnt->root) : NULL;
}

static char* g_udisks_mount_get_uuid (GMount* base)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return mnt->vol ? g_volume_get_uuid(G_VOLUME(mnt->vol)) : NULL;
}

static GVolume* g_udisks_mount_get_volume (GMount* base)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return (GVolume*)g_object_ref(mnt->vol);
}

typedef struct
{
    GUDisksMount* mnt;
    GAsyncReadyCallback callback;
    gpointer user_data;
    GFile* root;
}GuessContentData;


static char** g_udisks_mount_guess_content_type_finish (GMount* base, GAsyncResult* res, GError** error)
{
    g_simple_async_result_propagate_error(G_SIMPLE_ASYNC_RESULT(res), error);
    return g_strdupv((char**)g_simple_async_result_get_op_res_gpointer(G_SIMPLE_ASYNC_RESULT(res)));
}

static void guess_content_data_free(gpointer user_data)
{
    GuessContentData* data = (GuessContentData*)user_data;
    g_object_unref(data->mnt);
    if(data->root)
        g_object_unref(data->root);
    g_slice_free(GuessContentData, data);
}

static gboolean guess_content_job(GIOSchedulerJob *job, GCancellable* cancellable, gpointer user_data)
{
    GuessContentData* data = (GuessContentData*)user_data;
    char** content_types;
    GSimpleAsyncResult* res;
    content_types = g_content_type_guess_for_tree(data->root);
    res = g_simple_async_result_new(G_OBJECT(data->mnt),
                                    data->callback,
                                    data->user_data,
                                    NULL);
    g_simple_async_result_set_op_res_gpointer(res, content_types, (GDestroyNotify)g_strfreev);
    g_simple_async_result_complete_in_idle(res);
    g_object_unref(res);
    return FALSE;
}

static void g_udisks_mount_guess_content_type (GMount* base, gboolean force_rescan, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    GFile* root = g_udisks_mount_get_root(base);
    g_debug("guess content type");
    if(root)
    {
        /* FIXME: this is not really cancellable. */
        GuessContentData* data = g_slice_new(GuessContentData);
        /* NOTE: this should be an asynchronous action, but
         * g_content_type_guess_for_tree provided by glib is not
         * cancellable. So, we got some problems here.
         * If we really want a perfect asynchronous implementation,
         * another option is using fork() to implement this. */
        data->mnt = g_object_ref(mnt);
        data->root = root;
        data->callback = callback;
        data->user_data = user_data;
        g_io_scheduler_push_job(guess_content_job, data, guess_content_data_free, G_PRIORITY_DEFAULT, cancellable);
    }
}

static gchar** g_udisks_mount_guess_content_type_sync (GMount* base, gboolean force_rescan, GCancellable* cancellable, GError** error)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    if(mnt->root)
    {
        char** ret;
        GFile* root = g_udisks_mount_get_root(G_MOUNT(mnt));
        ret = g_content_type_guess_for_tree(root);
        g_object_unref(root);
        return ret;
    }
    return NULL;
}

//static void g_udisks_mount_remount (GMount* base, GMountMountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
//{
//    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    /* TODO */
//}

static void unmount_callback(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GDBusProxy *proxy = G_DBUS_PROXY(source);
    AsyncData* data = (AsyncData*)user_data;
    GSimpleAsyncResult* res;
    GError *error = NULL;
    GVariant *val = g_dbus_proxy_call_finish(proxy, result, &error);
    if(error)
    {
        // FIXME: use data->mount_operation to decide what to do
        res = g_simple_async_result_new_from_error(G_OBJECT(data->mnt),
                                                   data->callback,
                                                   data->user_data,
                                                   error);
        g_error_free(error);
    }
    else
    {
        res = g_simple_async_result_new(G_OBJECT(data->mnt),
                                        data->callback,
                                        data->user_data,
                                        NULL);
        g_simple_async_result_set_op_res_gboolean(res, TRUE);
        if (val) g_variant_unref(val);
    }
    g_simple_async_result_complete(res);
    g_object_unref(res);

    g_object_unref(data->mnt);
    if(data->mount_operation)
        g_object_unref(data->mount_operation);
    g_slice_free(AsyncData, data);
}

static void g_udisks_mount_unmount_with_operation (GMount* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    if(mnt->vol)
    {
        GUDisksDevice* dev = g_udisks_volume_get_device(mnt->vol);
        GDBusProxy* proxy = dev ? g_udisks_device_get_fs_proxy(dev) : NULL;
        if(proxy)
        {
            AsyncData* data = g_slice_new(AsyncData);
            GVariantBuilder b;
            data->mnt = g_object_ref(mnt);
            data->callback = callback;
            data->user_data = user_data;
            data->mount_operation = mount_operation ? g_object_ref(mount_operation) : NULL;

            g_debug("send DBus request to unmount %s", g_udisks_device_get_obj_path(dev));
            g_signal_emit(mnt, sig_pre_unmount, 0);

            g_variant_builder_init(&b, G_VARIANT_TYPE("(a{sv})"));
            g_variant_builder_open(&b, G_VARIANT_TYPE ("a{sv}"));
            g_variant_builder_close(&b);
            g_dbus_proxy_call(proxy, "Unmount", g_variant_builder_end(&b),
                              G_DBUS_CALL_FLAGS_NONE, -1, cancellable,
                              unmount_callback, data);
            g_object_unref(proxy);
        }
        else
        {
            GSimpleAsyncResult* res;
            char *dev_file = dev ? g_udisks_device_get_dev_file(dev) : NULL;

            res = g_simple_async_result_new_error(G_OBJECT(mnt), callback, user_data,
                                                  G_IO_ERROR, G_IO_ERROR_FAILED,
                                                  _("No filesystem proxy for '%s'"),
                                                  dev_file);
            g_simple_async_result_complete(res);
            g_object_unref(res);
            g_free(dev_file);
        }
    }
}

static gboolean g_udisks_mount_unmount_with_operation_finish(GMount* base, GAsyncResult* res, GError** error)
{
    //GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return !g_simple_async_result_propagate_error(G_SIMPLE_ASYNC_RESULT(res), error);
}

static void g_udisks_mount_unmount (GMount* base, GMountUnmountFlags flags, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    g_udisks_mount_unmount_with_operation(base, flags, NULL, cancellable, callback, user_data);
}

static gboolean g_udisks_mount_unmount_finish(GMount* base, GAsyncResult* res, GError** error)
{
    //GUDisksMount* mnt = G_UDISKS_MOUNT(base);
    return g_udisks_mount_unmount_with_operation_finish(base, res, error);
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
    iface->unmount_finish = g_udisks_mount_unmount_finish;
    iface->unmount_with_operation = g_udisks_mount_unmount_with_operation;
    iface->unmount_with_operation_finish = g_udisks_mount_unmount_with_operation_finish;
    iface->eject = g_udisks_mount_eject;
    iface->eject_finish = g_udisks_mount_eject_finish;
    iface->eject_with_operation = g_udisks_mount_eject_with_operation;
    iface->eject_with_operation_finish = g_udisks_mount_eject_with_operation_finish;
    iface->guess_content_type = g_udisks_mount_guess_content_type;
    iface->guess_content_type_finish = g_udisks_mount_guess_content_type_finish;
    iface->guess_content_type_sync = g_udisks_mount_guess_content_type_sync;

    sig_pre_unmount = g_signal_lookup("pre-unmount", G_TYPE_MOUNT);
    sig_unmounted = g_signal_lookup("unmounted", G_TYPE_MOUNT);
}

void g_udisks_mount_unmounted(GUDisksMount* mnt)
{
    g_signal_emit(mnt, sig_unmounted, 0);
    mnt->vol = NULL; /* disowned by the volume now */
}
