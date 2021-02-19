//      g-udisks-drive.c
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

#include "g-udisks-drive.h"
#include "g-udisks-volume.h"
#include "dbus-utils.h"

#include <string.h>
#include <glib/gi18n-lib.h>

/* This array is taken from gnome-disk-utility: gdu-volume.c
 * Copyright (C) 2007 David Zeuthen, licensed under GNU LGPL
 */
static const struct
{
        const char *disc_type;
        const char *icon_name;
        const char *ui_name;
        const char *ui_name_blank;
} disc_data[] = {
  /* Translator: The word "blank" is used as an adjective, e.g. we are decsribing discs that are already blank */
  {"optical_cd",             "media-optical-cd-rom",        N_("CD-ROM Disc"),     N_("Blank CD-ROM Disc")},
  {"optical_cd_r",           "media-optical-cd-r",          N_("CD-R Disc"),       N_("Blank CD-R Disc")},
  {"optical_cd_rw",          "media-optical-cd-rw",         N_("CD-RW Disc"),      N_("Blank CD-RW Disc")},
  {"optical_dvd",            "media-optical-dvd-rom",       N_("DVD-ROM Disc"),    N_("Blank DVD-ROM Disc")},
  {"optical_dvd_r",          "media-optical-dvd-r",         N_("DVD-ROM Disc"),    N_("Blank DVD-ROM Disc")},
  {"optical_dvd_rw",         "media-optical-dvd-rw",        N_("DVD-RW Disc"),     N_("Blank DVD-RW Disc")},
  {"optical_dvd_ram",        "media-optical-dvd-ram",       N_("DVD-RAM Disc"),    N_("Blank DVD-RAM Disc")},
  {"optical_dvd_plus_r",     "media-optical-dvd-r-plus",    N_("DVD+R Disc"),      N_("Blank DVD+R Disc")},
  {"optical_dvd_plus_rw",    "media-optical-dvd-rw-plus",   N_("DVD+RW Disc"),     N_("Blank DVD+RW Disc")},
  {"optical_dvd_plus_r_dl",  "media-optical-dvd-dl-r-plus", N_("DVD+R DL Disc"),   N_("Blank DVD+R DL Disc")},
  {"optical_dvd_plus_rw_dl", "media-optical-dvd-dl-r-plus", N_("DVD+RW DL Disc"),  N_("Blank DVD+RW DL Disc")},
  {"optical_bd",             "media-optical-bd-rom",        N_("Blu-Ray Disc"),    N_("Blank Blu-Ray Disc")},
  {"optical_bd_r",           "media-optical-bd-r",          N_("Blu-Ray R Disc"),  N_("Blank Blu-Ray R Disc")},
  {"optical_bd_re",          "media-optical-bd-re",         N_("Blu-Ray RW Disc"), N_("Blank Blu-Ray RW Disc")},
  {"optical_hddvd",          "media-optical-hddvd-rom",     N_("HD DVD Disc"),     N_("Blank HD DVD Disc")},
  {"optical_hddvd_r",        "media-optical-hddvd-r",       N_("HD DVD-R Disc"),   N_("Blank HD DVD-R Disc")},
  {"optical_hddvd_rw",       "media-optical-hddvd-rw",      N_("HD DVD-RW Disc"),  N_("Blank HD DVD-RW Disc")},
  {"optical_mo",             "media-optical-mo",            N_("MO Disc"),         N_("Blank MO Disc")},
  {"optical_mrw",            "media-optical-mrw",           N_("MRW Disc"),        N_("Blank MRW Disc")},
  {"optical_mrw_w",          "media-optical-mrw-w",         N_("MRW/W Disc"),      N_("Blank MRW/W Disc")},
  {NULL, NULL, NULL, NULL}
};

struct _GUDisksDrive
{
    GObject parent;
    char* obj_path; /* dbus object path */
    GUDisksDevice* dev;
    GList* vols; /* weak_ref */
    GIcon* icon;
    gboolean is_removable : 1;
    gboolean is_optic_disc : 1;
    gboolean is_media_available : 1;
    gboolean is_media_change_notification_polling : 1;
    gboolean is_ejectable : 1;
    gboolean is_disc_blank : 1;
    guint num_audio_tracks;
    //char* vender;
    //char* model;
    char* conn_iface;
    char* media;
    GDBusProxy *proxy; /* dbus proxy */
};

static void g_udisks_clear_props(GUDisksDrive* drv)
{
    //g_free(drv->vender);
    //g_free(drv->model);
    g_free(drv->conn_iface);
    g_free(drv->media);
    if(drv->icon)
        g_object_unref(drv->icon);
    drv->icon = NULL;
}

static void g_udisks_fill_props(GUDisksDrive* drv)
{
    drv->conn_iface = dbus_prop_dup_str(drv->proxy, "ConnectionBus");
    drv->is_removable = dbus_prop_bool(drv->proxy, "MediaRemovable");
    drv->is_optic_disc = dbus_prop_bool(drv->proxy, "Optical");
    drv->is_media_available = dbus_prop_bool(drv->proxy, "MediaAvailable");
    drv->is_media_change_notification_polling = dbus_prop_bool(drv->proxy, "MediaChangeDetected");
    drv->is_ejectable = dbus_prop_bool(drv->proxy, "Ejectable");
    drv->is_disc_blank = dbus_prop_bool(drv->proxy, "OpticalBlank");
    drv->num_audio_tracks = dbus_prop_uint(drv->proxy, "OpticalNumAudioTracks");
    //drv->vender = dbus_prop_dup_str(drv->proxy, "Vendor");
    //drv->model = dbus_prop_dup_str(drv->proxy, "Model");
    drv->media = dbus_prop_dup_str(drv->proxy, "Media");
}

typedef struct
{
    GUDisksDrive* drv;
    GAsyncReadyCallback callback;
    GMountUnmountFlags flags;
    GMountOperation* op;
    GCancellable* cancellable;
    gpointer user_data;
    GList* mounts;
}EjectData;

static guint sig_changed;
static guint sig_disconnected;
static guint sig_eject_button;
static guint sig_stop_button;

static void g_udisks_drive_drive_iface_init (GDriveIface * iface);
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


static gboolean g_udisks_drive_can_eject (GDrive* base)
{
    GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    return drv->is_ejectable;
}

static gboolean g_udisks_drive_can_poll_for_media (GDrive* base)
{
    GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    return drv->is_media_change_notification_polling;
}

static gboolean g_udisks_drive_can_start (GDrive* base)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
    return FALSE;
}

static gboolean g_udisks_drive_can_start_degraded (GDrive* base)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
    return FALSE;
}

static gboolean g_udisks_drive_can_stop (GDrive* base)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
    return FALSE;
}

static GList* g_udisks_drive_get_volumes (GDrive* base)
{
    GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    GList* vols = g_list_copy(drv->vols);
    g_list_foreach(vols, (GFunc)g_object_ref, NULL);
    return vols;
}

static void finish_eject(GSimpleAsyncResult* res, EjectData* data)
{
    g_simple_async_result_complete(res);
    g_object_unref(res);

    g_object_unref(data->drv);
    if (data->op)
        g_object_unref(data->op);
    if (data->cancellable)
        g_object_unref(data->cancellable);
    while (data->mounts)
    {
        g_object_unref(data->mounts->data);
        data->mounts = g_list_delete_link(data->mounts, data->mounts);
    }

    g_slice_free(EjectData, data);
}

static void on_ejected(GObject *source_object, GAsyncResult *result, gpointer user_data)
{
    EjectData* data = (EjectData*)user_data;
    GDBusProxy *proxy = (GDBusProxy *)source_object;
    GError *err = NULL;
    GSimpleAsyncResult *res;
    GVariant *val = g_dbus_proxy_call_finish(proxy, result, &err);

    if(err)
    {
        //TODO: use data->op to handle errors?
        GError *error = g_udisks_error_to_gio_error(err);
        g_error_free(err);
        res = g_simple_async_result_new_from_error(G_OBJECT(data->drv),
                                                   data->callback,
                                                   data->user_data,
                                                   error);
        g_error_free(error);
    }
    else
    {
        res = g_simple_async_result_new(G_OBJECT(data->drv),
                                        data->callback,
                                        data->user_data,
                                        NULL);
        g_simple_async_result_set_op_res_gboolean(res, TRUE); // FIXME
        if (val)
            g_variant_unref(val);
    }
    finish_eject(res, data);
}

static void do_eject(EjectData* data)
{
    GVariantBuilder b;
    g_debug("%s: sending DBus request", __func__);
    g_variant_builder_init(&b, G_VARIANT_TYPE("(a{sv})"));
    g_variant_builder_open(&b, G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_close(&b);
    g_dbus_proxy_call(data->drv->proxy, "Eject", g_variant_builder_end(&b),
                      G_DBUS_CALL_FLAGS_NONE, -1, data->cancellable, on_ejected, data);
}

static void unmount_before_eject(EjectData* data);

static void on_unmounted(GObject* mnt, GAsyncResult* res, gpointer input_data)
{
#define data ((EjectData*)input_data)
    GError* err = NULL;
    /* FIXME: with this approach, we could have racing condition.
     * Someone may mount other volumes before we finishing unmounting them all. */
    gboolean success = g_mount_unmount_with_operation_finish(G_MOUNT(mnt), res, &err);
    if(success)
    {
        /* we did picked first one from list */
        data->mounts = g_list_delete_link(data->mounts, data->mounts);
        g_object_unref(mnt);
        if(data->mounts) /* we still have some volumes on this drive mounted */
            unmount_before_eject(data);
        else /* all unmounted, do the eject. */
            do_eject(data);
    }
    else
    {
        GSimpleAsyncResult* res;
        res = g_simple_async_result_new_from_error(G_OBJECT(data->drv),
                                                   data->callback,
                                                   data->user_data,
                                                   err);
        finish_eject(res, data);
        g_error_free(err);
    }
#undef data
}

static void unmount_before_eject(EjectData* data)
{
    GMount* mnt = G_MOUNT(data->mounts->data);
    /* pop the first GMount in the list. */
    g_mount_unmount_with_operation(mnt, data->flags, data->op,
                                 data->cancellable,
                                 on_unmounted, data);
}

static void g_udisks_drive_eject_with_operation (GDrive* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    GList* vols = g_udisks_drive_get_volumes(base);
    GList* mounts = NULL;
    EjectData* data;

    /* umount all volumes/mounts first */
    if(vols)
    {
        GList* l;
        for(l = vols; l; l=l->next)
        {
            GUDisksVolume* vol = G_UDISKS_VOLUME(l->data);
            mounts = g_list_concat(g_udisks_volume_get_mounts(vol), mounts);
            g_object_unref(vol);
        }
        g_list_free(vols);
        g_list_foreach(mounts, (GFunc)g_object_ref, NULL);
    }

    data = g_slice_new0(EjectData);
    data->drv = g_object_ref(drv);
    data->callback = callback;
    data->cancellable = cancellable ? g_object_ref(cancellable) : NULL;
    data->flags = flags;
    data->op = mount_operation ? g_object_ref(mount_operation) : NULL;
    data->user_data = user_data;

    if(mounts) /* unmount all GMounts first, and do eject in ready callback */
    {
        /* NOTE: is this really needed?
         * I read the source code of UDisks and found it calling "eject"
         * command internally. According to manpage of "eject", it unmounts
         * partitions before ejecting the device. So I don't think that we
         * need to unmount ourselves. However, without this, we won't have
         * correct "mount-pre-unmount" signals. So, let's do it. */
        data->mounts = mounts;
        unmount_before_eject(data);
    }
    else /* no volume is mounted. it's safe to do eject directly. */
        do_eject(data);
}

static gboolean g_udisks_drive_eject_with_operation_finish (GDrive* base, GAsyncResult* res, GError** error)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    return !g_simple_async_result_propagate_error(G_SIMPLE_ASYNC_RESULT(res), error);
}

static void g_udisks_drive_eject (GDrive* base, GMountUnmountFlags flags, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    g_udisks_drive_eject_with_operation(base, flags, NULL, cancellable, callback, user_data);
}

static gboolean g_udisks_drive_eject_finish (GDrive* base, GAsyncResult* res, GError** error)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    return g_udisks_drive_eject_with_operation_finish(base, res, error);
}

static char** g_udisks_drive_enumerate_identifiers (GDrive* base)
{
    char** kinds = g_new0(char*, 4);
    kinds[0] = g_strdup(G_VOLUME_IDENTIFIER_KIND_LABEL);
    kinds[1] = g_strdup(G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    kinds[2] = g_strdup(G_VOLUME_IDENTIFIER_KIND_UUID);
    kinds[3] = NULL;
    return kinds;
}

const char* g_udisks_drive_get_obj_path(GUDisksDrive* drv)
{
    return drv->obj_path;
}

static const char* g_udisks_drive_get_icon_name(GUDisksDrive* drv)
{
    const char* icon_name = NULL;

    if(drv->media && *drv->media) /* by media type */
    {
        if(drv->is_optic_disc)
        {
            if(drv->num_audio_tracks > 0)
                icon_name = "media-optical-audio";
            else
            {
                guint i;
                icon_name = "media-optical";
                for( i = 0; i < G_N_ELEMENTS(disc_data); ++i)
                {
                    if(strcmp(drv->media, disc_data[i].disc_type) == 0)
                    {
                        if(drv->is_disc_blank)
                            icon_name = disc_data[i].icon_name;
                        break;
                    }
                }
            }
        }
        else
        {
            if(strcmp (drv->media, "flash_cf") == 0)
                icon_name = "media-flash-cf";
            else if(strcmp (drv->media, "flash_ms") == 0)
                icon_name = "media-flash-ms";
            else if(strcmp (drv->media, "flash_sm") == 0)
                icon_name = "media-flash-sm";
            else if(g_str_has_prefix (drv->media, "flash_sd"))
                icon_name = "media-flash-sd";
            else if(strcmp (drv->media, "flash_mmc") == 0)
                icon_name = "media-flash-sd";
            else if(strcmp (drv->media, "floppy") == 0)
                icon_name = "media-floppy";
            else if(strcmp (drv->media, "floppy_zip") == 0)
                icon_name = "media-floppy-zip";
            else if(strcmp (drv->media, "floppy_jaz") == 0)
                icon_name = "media-floppy-jaz";
            else if(g_str_has_prefix (drv->media, "flash"))
                icon_name = "media-flash";
            else if(strcmp (drv->media, "thumb") == 0)
                icon_name = "drive-harddisk-usb";
        }
    }
    else if(drv->conn_iface && *drv->conn_iface) /* by connection interface */
    {
/*        if(g_str_has_prefix(drv->conn_iface, "ata"))
            icon_name = drv->is_removable ? "drive-removable-media-ata" : "drive-harddisk-ata";
        else if(g_str_has_prefix (drv->conn_iface, "scsi"))
            icon_name = drv->is_removable ? "drive-removable-media-scsi" : "drive-harddisk-scsi";
        else */
        if(strcmp (drv->conn_iface, "usb") == 0)
            icon_name = drv->is_removable ? "drive-removable-media-usb" : "drive-harddisk-usb";
        else if (strcmp (drv->conn_iface, "ieee1394") == 0)
            icon_name = drv->is_removable ? "drive-removable-media-ieee1394" : "drive-harddisk-ieee1394";
    }

    if(!icon_name)
    {
        if(drv->is_removable)
            icon_name = "drive-removable-media";
        else
            icon_name = "drive-harddisk";
    }
    return icon_name;
}

const char* g_udisks_drive_get_disc_name(GUDisksDrive* drv)
{
    const char* name = NULL;
    if(!drv || !drv->is_optic_disc)
        return NULL;
    if(drv->media && *drv->media)
    {
        if(drv->num_audio_tracks > 0 && g_str_has_prefix(drv->media, "optical_cd"))
            name = _("Audio CD");
        else
        {
            guint i;
            for( i = 0; i < G_N_ELEMENTS(disc_data); ++i)
            {
                if(strcmp(drv->media, disc_data[i].disc_type) == 0)
                {
                    if(drv->is_disc_blank)
                        name = gettext(disc_data[i].ui_name_blank);
                    else
                        name = gettext(disc_data[i].ui_name);
                    break;
                }
            }
        }
    }

    if(!name)
    {
        if(drv->is_disc_blank)
            name = _("Blank Optical Disc");
        else
            name = _("Optical Disc");
    }
    return name;
}

static GIcon* g_udisks_drive_get_icon (GDrive* base)
{
    GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    if(!drv->icon)
    {
        char* drv_icon_name = drv->dev ? g_udisks_device_get_icon_name(drv->dev) : NULL;
        const char* icon_name = drv_icon_name ? drv_icon_name : g_udisks_drive_get_icon_name(drv);
        drv->icon = g_themed_icon_new_with_default_fallbacks(icon_name);
        g_free(drv_icon_name);
    }
    return (GIcon*)g_object_ref(drv->icon);
}

static char* g_udisks_drive_get_identifier (GDrive* base, const char* kind)
{
    GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    if(kind && drv->dev)
    {
        if(strcmp(kind, G_VOLUME_IDENTIFIER_KIND_LABEL) == 0)
            return g_udisks_device_get_label(drv->dev);
        else if(strcmp(kind, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE) == 0)
            return g_udisks_device_get_dev_file(drv->dev);
        else if(strcmp(kind, G_VOLUME_IDENTIFIER_KIND_UUID) == 0)
            return g_udisks_device_get_uuid(drv->dev);
    }
    return NULL;
}

static char* g_udisks_drive_get_name (GDrive* base)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
    return g_strdup("");
}

static GDriveStartStopType g_udisks_drive_get_start_stop_type (GDrive* base)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
    return G_DRIVE_START_STOP_TYPE_UNKNOWN;
}

static gboolean g_udisks_drive_has_media (GDrive* base)
{
    GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    return drv->is_media_available;
}

static gboolean g_udisks_drive_has_volumes (GDrive* base)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    return FALSE;
}

static gboolean g_udisks_drive_is_media_check_automatic (GDrive* base)
{
    GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* FIXME: is this correct? */
    return drv->is_media_change_notification_polling;
}

static gboolean g_udisks_drive_is_media_removable (GDrive* base)
{
    GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    return drv->is_removable;
}

static void g_udisks_drive_poll_for_media (GDrive* base, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
}

static gboolean g_udisks_drive_poll_for_media_finish (GDrive* base, GAsyncResult* res, GError** error)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    return FALSE;
}

static void g_udisks_drive_start (GDrive* base, GDriveStartFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
}

static gboolean g_udisks_drive_start_finish (GDrive* base, GAsyncResult* res, GError** error)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
    return FALSE;
}

static void g_udisks_drive_stop (GDrive* base, GMountUnmountFlags flags, GMountOperation* mount_operation, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
}

static gboolean g_udisks_drive_stop_finish (GDrive* base, GAsyncResult* res, GError** error)
{
    //GUDisksDrive* drv = G_UDISKS_DRIVE(base);
    /* TODO */
    return FALSE;
}

static void g_udisks_drive_changed(GDBusProxy *proxy, GVariant *changed_properties,
                                   GStrv invalidated_properties, gpointer user_data)
{
    g_return_if_fail(G_IS_UDISKS_DRIVE(user_data));
    GUDisksDrive* drv = G_UDISKS_DRIVE(user_data);

    g_udisks_clear_props(drv);
    g_udisks_fill_props(drv);
    g_signal_emit(drv, sig_changed, 0);
}


void g_udisks_drive_drive_iface_init (GDriveIface * iface)
{
    iface->get_name = g_udisks_drive_get_name;
    iface->get_icon = g_udisks_drive_get_icon;
    iface->has_volumes = g_udisks_drive_has_volumes;
    iface->get_volumes = g_udisks_drive_get_volumes;
    iface->is_media_removable = g_udisks_drive_is_media_removable;
    iface->has_media = g_udisks_drive_has_media;
    iface->is_media_check_automatic = g_udisks_drive_is_media_check_automatic;
    iface->can_eject = g_udisks_drive_can_eject;
    iface->can_poll_for_media = g_udisks_drive_can_poll_for_media;
    iface->eject = g_udisks_drive_eject;
    iface->eject_finish = g_udisks_drive_eject_finish;
    iface->eject_with_operation = g_udisks_drive_eject_with_operation;
    iface->eject_with_operation_finish = g_udisks_drive_eject_with_operation_finish;
    iface->poll_for_media = g_udisks_drive_poll_for_media;
    iface->poll_for_media_finish = g_udisks_drive_poll_for_media_finish;
    iface->get_identifier = g_udisks_drive_get_identifier;
    iface->enumerate_identifiers = g_udisks_drive_enumerate_identifiers;

    iface->get_start_stop_type = g_udisks_drive_get_start_stop_type;
    iface->can_start = g_udisks_drive_can_start;
    iface->can_start_degraded = g_udisks_drive_can_start_degraded;
    iface->can_stop = g_udisks_drive_can_stop;
    iface->start = g_udisks_drive_start;
    iface->start_finish = g_udisks_drive_start_finish;
    iface->stop = g_udisks_drive_stop;
    iface->stop_finish = g_udisks_drive_stop_finish;

    sig_changed = g_signal_lookup("changed", G_TYPE_DRIVE);
    sig_disconnected = g_signal_lookup("disconnected", G_TYPE_DRIVE);
    sig_eject_button = g_signal_lookup("eject-button", G_TYPE_DRIVE);
    sig_stop_button = g_signal_lookup("stop-button", G_TYPE_DRIVE);

}

static void g_udisks_drive_finalize(GObject *object)
{
    GUDisksDrive *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(G_IS_UDISKS_DRIVE(object));

    self = G_UDISKS_DRIVE(object);

    if(self->dev)
        g_object_unref(self->dev);

    if(self->proxy)
    {
        g_signal_handlers_disconnect_by_func(self->proxy, G_CALLBACK(g_udisks_drive_changed), self);
        g_object_unref(self->proxy);
    }

    if(self->vols)
    {
        g_warning("GUDisksDrive: volumes list isn't empty on exit!");
        g_list_free(self->vols);
    }

    g_udisks_clear_props(self);

    g_free(self->obj_path);

    G_OBJECT_CLASS(g_udisks_drive_parent_class)->finalize(object);
}


static void g_udisks_drive_init(GUDisksDrive *self)
{

}

GUDisksDrive *g_udisks_drive_new(GDBusConnection* con, const char *obj_path,
                                 GCancellable* cancellable, GError** error)
{
    GUDisksDrive* drv = (GUDisksDrive*)g_object_new(G_TYPE_UDISKS_DRIVE, NULL);
    drv->obj_path = g_strdup(obj_path);
    drv->proxy = g_dbus_proxy_new_sync(con, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                       "org.freedesktop.UDisks2", obj_path,
                                       "org.freedesktop.UDisks2.Drive",
                                       cancellable, error);
    if (drv->proxy)
    {
        g_udisks_fill_props(drv);
        g_signal_connect(drv->proxy, "g-properties-changed",
                         G_CALLBACK(g_udisks_drive_changed), drv);
    }
    return drv;
}

void g_udisks_drive_set_device_path(GUDisksDrive* drv, const char* obj_path,
                                    GDBusConnection* con, GCancellable* cancellable,
                                    GError** error)
{
    g_return_if_fail(G_IS_UDISKS_DRIVE(drv));
    if(drv->dev)
        g_object_unref(drv->dev);
    drv->dev = g_udisks_device_get(obj_path, con, cancellable, error);
}

void g_udisks_drive_add_volume(GUDisksDrive* drv, GUDisksVolume* vol)
{
    GList *l = g_list_find(drv->vols, vol);
    if(l)
        g_warning("Volume is already in list on drive!");
    else
        drv->vols = g_list_append(drv->vols, vol);
}

void g_udisks_drive_del_volume(GUDisksDrive* drv, GUDisksVolume* vol)
{
    drv->vols = g_list_remove(drv->vols, vol);
}

gboolean g_udisks_drive_is_disc_blank(GUDisksDrive* drv)
{
    return drv->is_disc_blank;
}

void g_udisks_drive_disconnected(GUDisksDrive* drv)
{
    g_signal_emit(drv, sig_disconnected, 0);
}
