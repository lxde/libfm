/*
 *      g-udisks-volume-monitor.c
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
 *      Copyright 2021 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "g-udisks-volume-monitor.h"
#include "g-udisks-drive.h"
#include "g-udisks-mount.h"

#include <string.h>

/* FIXME: later we need to remove this when gio-udisks becomes an
 * independent gio module. */
#include "fm-config.h"

/*
 * Relations:
 * Monitor => Device => Volume => Mount[]
 *                             <= (weak)Mount
 *                   => Drive
 *                   <= (weak)Drive
 *                   <= (weak)Volume
 *         => Drive
 */

struct _GUDisksVolumeMonitor
{
    GNativeVolumeMonitor parent;
    GDBusConnection* con;
    GDBusProxy* udisks_proxy;
    GCancellable *cancellable;

    GList* devices;
    GList* drives;
};

static guint sig_drive_changed;
static guint sig_drive_connected;
static guint sig_drive_disconnected;
static guint sig_drive_eject_button;
static guint sig_drive_stop_button;
static guint sig_mount_added;
static guint sig_mount_changed;
static guint sig_mount_preunmount;
static guint sig_mount_removed;
static guint sig_volume_added;
static guint sig_volume_changed;
static guint sig_volume_removed;

static GUDisksDrive *add_drive(GUDisksVolumeMonitor* mon, const char *obj_path,
                               gboolean emit_signal);
static void remove_device(GUDisksVolumeMonitor* mon, const char* obj_path);
static void remove_drive(GUDisksVolumeMonitor* mon, const char* obj_path);

static void g_udisks_volume_monitor_finalize            (GObject *object);
static GMount* get_mount_for_mount_path(const char *mount_path, GCancellable *cancellable);

static gboolean is_supported(void);
static GList* get_connected_drives(GVolumeMonitor *mon);
static GList* get_volumes(GVolumeMonitor *mon);
static GList* get_mounts(GVolumeMonitor *mon);
static GVolume *get_volume_for_uuid(GVolumeMonitor *mon, const char *uuid);
static GMount *get_mount_for_uuid(GVolumeMonitor *mon, const char *uuid);
static void drive_eject_button(GVolumeMonitor *mon, GDrive *drive);

static GUDisksDevice* add_device(GUDisksVolumeMonitor* mon, const char* obj_path,
                                 gboolean do_introspection);

static void on_device_changed(GUDisksDevice* dev, GUDisksVolumeMonitor* mon);
static void on_mount_added(GUDisksDevice* dev, GUDisksMount* mnt, GUDisksVolumeMonitor* mon);
static void on_mount_preunmount(GUDisksDevice* dev, GUDisksMount* mnt, GUDisksVolumeMonitor* mon);
static void on_mount_removed(GUDisksDevice* dev, GUDisksMount* mnt, GUDisksVolumeMonitor* mon);
static void on_drive_changed(GUDisksDrive* dev, GUDisksVolumeMonitor* mon);

static GList* find_device_l(GUDisksVolumeMonitor* mon, const char* obj_path);
static GList* find_drive_l(GUDisksVolumeMonitor* mon, const char* obj_path);

static inline GUDisksDevice* find_device(GUDisksVolumeMonitor* mon, const char* obj_path)
{
    GList* l = find_device_l(mon, obj_path);
    return l ? G_UDISKS_DEVICE(l->data) : NULL;
}

static inline GUDisksDrive* find_drive(GUDisksVolumeMonitor* mon, const char* obj_path)
{
    GList* l = find_drive_l(mon, obj_path);
    return l ? G_UDISKS_DRIVE(l->data) : NULL;
}

/* callback when new device or drive added or removed */
static void on_g_signal(GDBusProxy *proxy, char *sender_name, char *signal_name,
                        GVariant *parameters, gpointer user_data)
{
    GUDisksVolumeMonitor* mon = G_UDISKS_VOLUME_MONITOR(user_data);

    if (!g_variant_is_container(parameters) || g_variant_n_children(parameters) < 2)
    {
        // error, should be either 'oas' or 'oa{sa{sv}}'
        return;
    }

    GVariant *target = g_variant_get_child_value(parameters, 0);
    GVariant *ifaces = g_variant_get_child_value(parameters, 1);

    if (!g_variant_is_container(ifaces))
    {
        // should never happen
    }
    else if (strcmp(signal_name, "InterfacesAdded") == 0)
    {
        GVariant *test = g_variant_lookup_value(ifaces,
                                                "org.freedesktop.UDisks2.Drive", NULL);
        if (test)
        {
            /* appears to be a drive itself */
            add_drive(mon, g_variant_get_string(target, NULL), TRUE);
            g_variant_unref(test);
        }
        else if ((test = g_variant_lookup_value(ifaces,
                                                "org.freedesktop.UDisks2.Block", NULL)))
        {
            GUDisksDevice* dev = add_device(mon, g_variant_get_string(target, NULL),
                                            FALSE);
            g_variant_unref(test);
            if (dev)
            {
                char *drv_obj_path = g_udisks_device_get_drive_obj_path(dev);
                GUDisksDrive *drv = find_drive(mon, drv_obj_path);
                g_debug("added new device '%s' for drive '%s'", g_udisks_device_get_obj_path(dev), drv_obj_path);
                g_free(drv_obj_path);

                if (drv)
                {
                    g_udisks_device_set_drive(dev, G_DRIVE(drv));
                    test = g_variant_lookup_value(ifaces,
                                                  "org.freedesktop.UDisks2.PartitionTable", NULL);
                    g_udisks_drive_add_device(drv, dev, test != NULL);
                    if (test)
                    {
                        /* appears to be a drive device */
                        g_variant_unref(test);
                        /* FIXME: this is useless unless we support burn:/// */
                        if (g_udisks_drive_is_disc_blank(drv))
                        {
                            GUDisksVolume *vol = g_udisks_volume_new(dev, NULL);
                            g_debug("adding new volume for %s", g_udisks_device_get_obj_path(dev));
                            g_signal_emit(mon, sig_volume_added, 0, vol);
                        }
                    }
                }
                test = g_variant_lookup_value(ifaces,
                                              "org.freedesktop.UDisks2.Filesystem", NULL);
                if (test)
                {
                    /* appears to be mountable */
                    GFile *activation_root = NULL; //FIXME: use HintName or IdUUID into some point
                    GUDisksVolume *vol = g_udisks_volume_new(dev, activation_root);
                    g_debug("adding new volume for %s", g_udisks_device_get_obj_path(dev));
                    g_signal_emit(mon, sig_volume_added, 0, vol);
                    g_variant_unref(test);
                }
            }
        }
    }
    else if (strcmp(signal_name, "InterfacesRemoved") == 0)
    {
        GVariantIter iter;
        char *val;
        g_variant_iter_init(&iter, ifaces);
        while (g_variant_iter_next(&iter, "s", &val))
        {
            if (strcmp(val, "org.freedesktop.UDisks2.Drive") == 0)
                remove_drive(mon, g_variant_get_string(target, NULL));
            else if (strcmp(val, "org.freedesktop.UDisks2.Block") == 0)
                remove_device(mon, g_variant_get_string(target, NULL));
            g_free(val);
        }
    }
    else
    {
        // shoudl never happen!
    }
    g_variant_unref(target);
    g_variant_unref(ifaces);
}


G_DEFINE_TYPE(GUDisksVolumeMonitor, g_udisks_volume_monitor, G_TYPE_NATIVE_VOLUME_MONITOR);


static void g_udisks_volume_monitor_class_init(GUDisksVolumeMonitorClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GNativeVolumeMonitorClass *parent_class = G_NATIVE_VOLUME_MONITOR_CLASS(klass);
    GVolumeMonitorClass *monitor_class = G_VOLUME_MONITOR_CLASS (klass);

    g_object_class->finalize = g_udisks_volume_monitor_finalize;
    parent_class->get_mount_for_mount_path = get_mount_for_mount_path;

    monitor_class->get_mounts = get_mounts;
    monitor_class->get_volumes = get_volumes;
    monitor_class->get_connected_drives = get_connected_drives;
    monitor_class->get_volume_for_uuid = get_volume_for_uuid;
    monitor_class->get_mount_for_uuid = get_mount_for_uuid;
    monitor_class->is_supported = is_supported;
    monitor_class->drive_eject_button = drive_eject_button;

    sig_drive_changed = g_signal_lookup("drive-changed", G_TYPE_VOLUME_MONITOR);
    sig_drive_connected = g_signal_lookup("drive-connected", G_TYPE_VOLUME_MONITOR);
    sig_drive_disconnected = g_signal_lookup("drive-disconnected", G_TYPE_VOLUME_MONITOR);
    sig_drive_eject_button = g_signal_lookup("drive-eject-button", G_TYPE_VOLUME_MONITOR);
    sig_drive_stop_button = g_signal_lookup("drive-stop-button", G_TYPE_VOLUME_MONITOR);
    sig_mount_added = g_signal_lookup("mount-added", G_TYPE_VOLUME_MONITOR);
    sig_mount_changed = g_signal_lookup("mount-changed", G_TYPE_VOLUME_MONITOR);
    sig_mount_preunmount = g_signal_lookup("mount-pre-unmount", G_TYPE_VOLUME_MONITOR);
    sig_mount_removed = g_signal_lookup("mount-removed", G_TYPE_VOLUME_MONITOR);
    sig_volume_added = g_signal_lookup("volume-added", G_TYPE_VOLUME_MONITOR);
    sig_volume_changed = g_signal_lookup("volume-changed", G_TYPE_VOLUME_MONITOR);
    sig_volume_removed = g_signal_lookup("volume-removed", G_TYPE_VOLUME_MONITOR);
}

static void g_udisks_volume_monitor_finalize(GObject *object)
{
    GUDisksVolumeMonitor *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_VOLUME_MONITOR(object));

    self = G_UDISKS_VOLUME_MONITOR(object);

    if(self->cancellable)
    {
        g_cancellable_cancel(self->cancellable);
        g_object_unref(self->cancellable);
    }

    if(self->udisks_proxy)
    {
        g_signal_handlers_disconnect_by_func(self->udisks_proxy,
                                             G_CALLBACK(on_g_signal), self);
        g_object_unref(self->udisks_proxy);
    }

    while(self->devices)
    {
        g_signal_handlers_disconnect_by_func(self->devices->data,
                                             G_CALLBACK(on_device_changed), self);
        g_signal_handlers_disconnect_by_func(self->devices->data,
                                             G_CALLBACK(on_mount_added), self);
        g_signal_handlers_disconnect_by_func(self->devices->data,
                                             G_CALLBACK(on_mount_preunmount), self);
        g_signal_handlers_disconnect_by_func(self->devices->data,
                                             G_CALLBACK(on_mount_removed), self);
        g_object_unref(self->devices->data);
        self->devices = g_list_delete_link(self->devices, self->devices);
    }

    while(self->drives)
    {
        g_signal_handlers_disconnect_by_func(self->drives->data,
                                             G_CALLBACK(on_drive_changed), self);
        g_udisks_drive_disconnected(self->drives->data);
        g_object_unref(self->drives->data);
        self->drives = g_list_delete_link(self->drives, self->drives);
    }

    G_OBJECT_CLASS(g_udisks_volume_monitor_parent_class)->finalize(object);
}

/* second phase of initialization: block devices scanned, add them and start monitoring */
static void do_introspection(GUDisksVolumeMonitor *mon, const char *obj_path,
                             void (*process)(GDBusNodeInfo *, const char *, GUDisksVolumeMonitor *, gpointer),
                             gpointer pdata)
{
    GVariant *res;
    GError *error = NULL;

    res = g_dbus_connection_call_sync(mon->con, "org.freedesktop.UDisks2",
                                      obj_path,
                                      "org.freedesktop.DBus.Introspectable",
                                      "Introspect", NULL, G_VARIANT_TYPE("(s)"),
                                      G_DBUS_CALL_FLAGS_NONE, -1,
                                      mon->cancellable, &error);

    /* parse result */
    if (!error)
    {
        GVariant *xml = g_variant_get_child_value(res, 0);
        GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(g_variant_get_string(xml, NULL),
                                                           &error);

        if (error)
        {
            g_warning("Udisks2 introspection bad result: %s", error->message);
            g_error_free(error);
        }
        else
        {
            process(info, obj_path, mon, pdata);
            g_dbus_node_info_unref(info);
        }
        g_variant_unref(xml);
        g_variant_unref(res);
    }
    else
    {
        g_warning("Udisks2 introspection failed: %s", error->message);
        g_error_free(error);
    }
}

static void process_drives_introspect(GDBusNodeInfo *info, const char *obj_path,
                                      GUDisksVolumeMonitor *mon, gpointer pdata)
{
    GDBusNodeInfo **sub;
    GString *str = g_string_new(obj_path);
    gsize len;

    g_string_append_c(str, '/');
    len = str->len;
    for (sub = info->nodes; sub && sub[0]; sub++)
    {
        g_string_append(str, sub[0]->path);
        add_drive(mon, str->str, FALSE);
        g_string_truncate(str, len);
    }
    g_string_free(str, TRUE);
}

static void process_devices_introspect(GDBusNodeInfo *info, const char *obj_path,
                                       GUDisksVolumeMonitor *mon, gpointer pdata)
{
    GDBusNodeInfo **sub;
    GString *str = g_string_new(obj_path);
    gsize len;

    g_string_append_c(str, '/');
    len = str->len;
    for (sub = info->nodes; sub && sub[0]; sub++)
    {
        g_string_append(str, sub[0]->path);
        add_device(mon, str->str, TRUE);
        g_string_truncate(str, len);
    }
    g_string_free(str, TRUE);
}

/* first phase of initialization: DBus connected */
static void on_dbus_connected(GObject *source_object, GAsyncResult *result,
                              gpointer user_data)
{
    GUDisksVolumeMonitor *mon = G_UDISKS_VOLUME_MONITOR(user_data);
    GError *error = NULL;

    mon->con = g_bus_get_finish(result, &error);
    if (!error)
    {
        /* collect drives */
        const char *path = "/org/freedesktop/UDisks2/drives";
        do_introspection(mon, path, &process_drives_introspect, NULL);

        /* collect devices */
        path = "/org/freedesktop/UDisks2/block_devices";
        do_introspection(mon, path, &process_devices_introspect, NULL);

        /* add a monitor */
        mon->udisks_proxy = g_dbus_proxy_new_sync(mon->con,
                                                  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                  NULL, "org.freedesktop.UDisks2",
                                                  "/org/freedesktop/UDisks2",
                                                  "org.freedesktop.DBus.ObjectManager",
                                                  mon->cancellable, &error);
        if (!error)
        {
            g_object_ref_sink(mon->udisks_proxy);
            g_signal_connect(mon->udisks_proxy, "g-signal", G_CALLBACK(on_g_signal), mon);
        }
        else
        {
            g_warning("Udisks2 ObjectManager proxy failed: %s", error->message);
            g_error_free(error);
        }
    }
    else
    {
        g_warning("Cannot connect DBus: %s", error->message);
        g_error_free(error);
    }
}

static void g_udisks_volume_monitor_init(GUDisksVolumeMonitor *self)
{
    self->cancellable = g_object_ref_sink(g_cancellable_new());
    g_bus_get(G_BUS_TYPE_SYSTEM, self->cancellable, on_dbus_connected, self);
}


GNativeVolumeMonitor *g_udisks_volume_monitor_new(void)
{
    return g_object_new(G_UDISKS_VOLUME_MONITOR_TYPE, NULL);
}

GMount* get_mount_for_mount_path(const char *mount_path, GCancellable *cancellable)
{
    /* TODO */

    return NULL;
}


gboolean is_supported(void)
{
    return TRUE;
}

GList* get_connected_drives(GVolumeMonitor *mon)
{
    GUDisksVolumeMonitor* umon = G_UDISKS_VOLUME_MONITOR(mon);
    GList* drvs = g_list_copy(umon->drives);
    g_list_foreach(drvs, (GFunc)g_object_ref, NULL);
    return drvs;
}

GList* get_volumes(GVolumeMonitor *mon)
{
    GUDisksVolumeMonitor* umon = G_UDISKS_VOLUME_MONITOR(mon);
    GList* vols = NULL, *l;
    for (l = umon->devices; l; l = l->next)
    {
        GVolume *vol = g_udisks_device_get_volume(l->data);
        if (vol)
            vols = g_list_prepend(vols, vol);
    }
    return g_list_reverse(vols);
}

GList* get_mounts(GVolumeMonitor *mon)
{
    GList* vols = get_volumes(mon);
    GList* l;
    GList* mnts = NULL;

    for (l = vols; l; l = l->next)
    {
        mnts = g_list_concat(g_udisks_volume_get_mounts(l->data), mnts);
        g_object_unref(l->data);
    }
    g_list_free(vols);
    g_list_foreach(mnts, (GFunc)g_object_ref, NULL);
    return mnts;
}

GVolume *get_volume_for_uuid(GVolumeMonitor *mon, const char *uuid)
{
    GUDisksVolumeMonitor* umon = G_UDISKS_VOLUME_MONITOR(mon);
    GList* l;
    for(l = umon->devices; l; l=l->next)
    {
        char *dev_uuid = g_udisks_device_get_uuid(l->data);
        int res = g_strcmp0(dev_uuid, uuid);
        g_free(dev_uuid);
        if (res == 0)
            return g_udisks_device_get_volume(l->data);
    }
    return NULL;
}

GMount *get_mount_for_uuid(GVolumeMonitor *mon, const char *uuid)
{
    GVolume *vol = get_volume_for_uuid(mon, uuid);
    GMount *mnt = g_volume_get_mount(vol);
    g_object_unref(vol);
    return mnt;
}

/* signal added in 2.17 */
void drive_eject_button(GVolumeMonitor *mon, GDrive *drive)
{
    /* TODO */
    //GUDisksVolumeMonitor* umon = G_UDISKS_VOLUME_MONITOR(mon);

}

GList* find_device_l(GUDisksVolumeMonitor* mon, const char* obj_path)
{
    GList* l;
    for(l = mon->devices; l; l=l->next)
    {
        if(g_strcmp0(g_udisks_device_get_obj_path(l->data), obj_path) == 0)
            return l;
    }
    return NULL;
}

GList* find_drive_l(GUDisksVolumeMonitor* mon, const char* obj_path)
{
    GList* l;
    for(l = mon->drives; l; l=l->next)
    {
        if(g_strcmp0(g_udisks_drive_get_obj_path(l->data), obj_path) == 0)
            return l;
    }
    return NULL;
}

static GUDisksDrive *add_drive(GUDisksVolumeMonitor* mon, const char *obj_path,
                               gboolean emit_signal)
{
    GUDisksDrive *drv = find_drive(mon, obj_path);
    if (!drv)
    {
        drv = g_udisks_drive_new(mon->con, obj_path, mon->cancellable, NULL); // FIXME
        g_signal_connect(drv, "changed", G_CALLBACK(on_drive_changed), mon);
        mon->drives = g_list_prepend(mon->drives, g_object_ref_sink(drv));
        if(emit_signal)
            g_signal_emit(mon, sig_drive_connected, 0, drv);
    }
    return drv;
}

static void remove_drive(GUDisksVolumeMonitor* mon, const char *obj_path)
{
    GList* l = find_drive_l(mon, obj_path);
    g_debug("remove_drive: %s", obj_path);
    if(l)
    {
        GUDisksDrive* drv = G_UDISKS_DRIVE(l->data);
        mon->drives = g_list_delete_link(mon->drives, l);
        g_signal_handlers_disconnect_by_func(drv, G_CALLBACK(on_drive_changed), mon);
        g_signal_emit(mon, sig_drive_disconnected, 0, drv);
        g_udisks_drive_disconnected(drv);
        g_object_unref(drv);
    }
}

static void process_a_device_introspect(GDBusNodeInfo *info, const char *obj_path,
                                        GUDisksVolumeMonitor *mon, gpointer dev)
{
    GDBusInterfaceInfo **interfaces = info->interfaces;
    gboolean has_fs = FALSE, has_pt = FALSE;
    char *drv_obj_path;
    GUDisksDrive *drv;

    if (!interfaces) // Failure!
        return;

    drv_obj_path = g_udisks_device_get_drive_obj_path(dev);
    drv = find_drive(mon, drv_obj_path);
    if (drv)
        g_udisks_device_set_drive(dev, G_DRIVE(drv));
    while (*interfaces)
    {
        if (g_strcmp0((*interfaces)->name, "org.freedesktop.UDisks2.Filesystem") == 0)
            has_fs = TRUE;
        else if (drv && g_strcmp0((*interfaces)->name, "org.freedesktop.UDisks2.PartitionTable") == 0)
            has_pt = TRUE;
        interfaces++;
    }

    if (has_fs)
    {
        g_debug("adding new volume for %s", obj_path);
        g_udisks_volume_new(dev, NULL); //FIXME: set some activation_root?
    }
    if (drv)
    {
        g_debug("added device '%s' %s drive '%s'", obj_path, has_pt ? "as a" : "to", drv_obj_path);
        g_udisks_drive_add_device(drv, dev, has_pt);
        /* also treat blank optical discs as volumes here to be compatible with gvfs.
         * FIXME: this is useless unless we support burn:///
         * [PCMan] So, should we support this? Personally I think it's a bad idea. */
        if (g_udisks_drive_is_disc_blank(drv))
        {
            g_debug("adding new volume for %s", obj_path);
            g_udisks_volume_new(dev, NULL);
        }
    }
    g_free(drv_obj_path);
}

static void _g_udisks_device_added(GUDisksDevice* dev, GUDisksVolumeMonitor* mon,
                                   const char *obj_path)
{
    /* FIXME: how should we treat sys internal devices?
     * make this optional */
    g_debug("added new device '%s'", obj_path);
    if(!g_udisks_device_is_hidden(dev) &&
       (!g_udisks_device_is_sys_internal(dev) || fm_config->show_internal_volumes))
    {
        do_introspection(mon, obj_path, &process_a_device_introspect, dev);
    }
}

static void on_device_changed(GUDisksDevice* dev, GUDisksVolumeMonitor* mon)
{
    GVolume* vol = g_udisks_device_get_volume(dev);
    /*
    gboolean is_drive = dev->is_drive;
    char* usage = g_strdup(dev->usage);
    */
    g_debug("%s", __func__);

    if(vol)
    {
        g_signal_emit(mon, sig_volume_changed, 0, vol);
        g_udisks_volume_changed(G_UDISKS_VOLUME(vol));
        g_object_unref(vol);
    }
}

static void on_mount_added(GUDisksDevice* dev, GUDisksMount* mnt, GUDisksVolumeMonitor* mon)
{
    g_debug(__func__);
    g_signal_emit(mon, sig_mount_added, 0, mnt);
}

static void on_mount_preunmount(GUDisksDevice* dev, GUDisksMount* mnt, GUDisksVolumeMonitor* mon)
{
    g_debug(__func__);
    g_signal_emit(mon, sig_mount_preunmount, 0, mnt);
}

static void on_mount_removed(GUDisksDevice* dev, GUDisksMount* mnt, GUDisksVolumeMonitor* mon)
{
    g_debug(__func__);
    g_signal_emit(mon, sig_mount_removed, 0, mnt);
}

static void on_drive_changed(GUDisksDrive* drv, GUDisksVolumeMonitor* mon)
{
    g_debug("on_drive_changed");

    g_signal_emit(mon, sig_drive_changed, 0, drv);
}

static GUDisksDevice* add_device(GUDisksVolumeMonitor* mon, const char* obj_path,
                                 gboolean do_introspection)
{
    if(!find_device(mon, obj_path))
    {
        GError* err = NULL;
        GUDisksDevice* dev = g_udisks_device_new(obj_path, mon->con, mon->cancellable, &err);

        if (!err)
        {
            mon->devices = g_list_prepend(mon->devices, g_object_ref_sink(dev));
            g_signal_connect(dev, "changed", G_CALLBACK(on_device_changed), mon);
            g_signal_connect(dev, "mount-added", G_CALLBACK(on_mount_added), mon);
            g_signal_connect(dev, "mount-pre-unmount", G_CALLBACK(on_mount_preunmount), mon);
            g_signal_connect(dev, "mount-removed", G_CALLBACK(on_mount_removed), mon);
            if (do_introspection)
                _g_udisks_device_added(dev, mon, obj_path);
            return dev;
        }
        else
        {
            g_debug("%s", err->message);
            g_error_free(err);
        }
    }
    return NULL;
}

static void remove_device(GUDisksVolumeMonitor* mon, const char* obj_path)
{
    GList* l;
    l = find_device_l(mon, obj_path);
    if(l)
    {
        GUDisksDevice* dev = G_UDISKS_DEVICE(l->data);
        GVolume* vol = g_udisks_device_get_volume(dev);

        g_signal_handlers_disconnect_by_func(dev, G_CALLBACK(on_device_changed), mon);
        g_signal_handlers_disconnect_by_func(dev, G_CALLBACK(on_mount_added), mon);
        g_signal_handlers_disconnect_by_func(dev, G_CALLBACK(on_mount_preunmount), mon);
        g_signal_handlers_disconnect_by_func(dev, G_CALLBACK(on_mount_removed), mon);
        mon->devices = g_list_delete_link(mon->devices, l);
        if (vol)
        {
            g_signal_emit(mon, sig_volume_removed, 0, vol);
            g_udisks_volume_removed(G_UDISKS_VOLUME(vol));
            g_object_unref(vol);
        }
        g_object_unref(dev);
    }
    g_debug("device_removed: %s", obj_path);

}
