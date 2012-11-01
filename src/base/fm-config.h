/*
 *      fm-config.h
 *
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
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


#ifndef __FM_CONFIG_H__
#define __FM_CONFIG_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define FM_CONFIG_TYPE              (fm_config_get_type())
#define FM_CONFIG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_CONFIG_TYPE, FmConfig))
#define FM_CONFIG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_CONFIG_TYPE, FmConfigClass))
#define FM_IS_CONFIG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_CONFIG_TYPE))
#define FM_IS_CONFIG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_CONFIG_TYPE))

typedef struct _FmConfig            FmConfig;
typedef struct _FmConfigClass       FmConfigClass;

#define     FM_CONFIG_DEFAULT_SINGLE_CLICK      FALSE
#define     FM_CONFIG_DEFAULT_USE_TRASH         TRUE
#define     FM_CONFIG_DEFAULT_CONFIRM_DEL       TRUE
#define     FM_CONFIG_DEFAULT_CONFIRM_TRASH     TRUE
#define     FM_CONFIG_DEFAULT_NO_USB_TRASH      FALSE

#define     FM_CONFIG_DEFAULT_BIG_ICON_SIZE     48
#define     FM_CONFIG_DEFAULT_SMALL_ICON_SIZE   16
#define     FM_CONFIG_DEFAULT_PANE_ICON_SIZE    16
#define     FM_CONFIG_DEFAULT_THUMBNAIL_SIZE    128

#define     FM_CONFIG_DEFAULT_SHOW_THUMBNAIL    TRUE
#define     FM_CONFIG_DEFAULT_THUMBNAIL_LOCAL   TRUE
#define     FM_CONFIG_DEFAULT_THUMBNAIL_MAX     2048

#define     FM_CONFIG_DEFAULT_FORCE_S_NOTIFY    TRUE
#define     FM_CONFIG_DEFAULT_BACKUP_HIDDEN     TRUE
#define     FM_CONFIG_DEFAULT_NO_EXPAND_EMPTY   FALSE
#define     FM_CONFIG_DEFAULT_SHOW_FULL_NAMES   FALSE

#define     FM_CONFIG_DEFAULT_AUTO_SELECTION_DELAY 600

/**
 * FmConfig:
 * @terminal: command line to launch terminal emulator
 * @archiver: desktop_id of the archiver used
 * @big_icon_size: size of big icons
 * @small_icon_size: size of small icons
 * @pane_icon_size: size of side pane icons
 * @thumbnail_size: size of thumbnail icons
 * @thumbnail_max: show thumbnails for files smaller than 'thumb_max' KB
 * @auto_selection_delay: delay for autoselection in single-click mode, in ms
 * @drop_default_action: default action on drop (see #FmDndDestDropAction)
 * @single_click: single click to open file
 * @use_trash: delete file to trash can
 * @confirm_del: ask before deleting files
 * @confirm_trash: ask before moving files to trash can
 * @show_thumbnail: show thumbnails
 * @thumbnail_local: show thumbnails for local files only
 * @show_internal_volumes: show system internal volumes in side pane. (udisks-only)
 * @si_unit: use SI prefix for file sizes
 * @advanced_mode: enable advanced features for experienced user
 * @force_startup_notify: use startup notify by default
 * @backup_as_hidden: treat backup files as hidden
 * @no_usb_trash: don't create trash folder on removable media
 * @no_child_non_expandable: hide expanders on empty folder
 * @show_full_names: always show full names in Icon View mode
 * @places_home: show 'Home' item in Places
 * @places_desktop: show 'Desktop' item in Places
 * @places_applications: show 'Applications' item in Places
 * @places_trash: show 'Trash' item in Places
 * @places_root: show '/' item in Places
 * @places_computer: chow 'My computer' item in Places
 * @places_network: show 'Network' item in Places
 * @places_unmounted: show unmounted internal volumes in Places
 * @select_child_on_up: select child where we were when go up to parent dir
 */
struct _FmConfig
{
    /*< private >*/
    GObject parent;

    /*< public >*/
    char* terminal;
    char* archiver;

    gint big_icon_size;
    gint small_icon_size;
    gint pane_icon_size;
    gint thumbnail_size;
    gint thumbnail_max;
    gint auto_selection_delay;
    gint drop_default_action;

    gboolean single_click;
    gboolean use_trash;
    gboolean confirm_del;
    gboolean confirm_trash;
    gboolean show_thumbnail;
    gboolean thumbnail_local;
    gboolean show_internal_volumes;
    gboolean si_unit;
    gboolean advanced_mode;
    gboolean force_startup_notify;
    gboolean backup_as_hidden;
    gboolean no_usb_trash;
    gboolean no_child_non_expandable;
    gboolean show_full_names;
    gboolean select_child_on_up;

    gboolean places_home;
    gboolean places_desktop;
    gboolean places_applications;
    gboolean places_trash;
    gboolean places_root;
    gboolean places_computer;
    gboolean places_network;
    gboolean places_unmounted;

    /*< private >*/
    gpointer _reserved1; /* reserved space for updates until next ABI */
    gpointer _reserved2;
    gpointer _reserved3;
    gpointer _reserved4;
    gpointer _reserved5;
    gpointer _reserved6;
    gpointer _reserved7;
    gpointer _reserved8;
};

/**
 * FmConfigClass
 * @parent_class: the parent class
 * @changed: the class closure for the #FmConfig::changed signal
 */
struct _FmConfigClass
{
    GObjectClass parent_class;
    void (*changed)(FmConfig* cfg);
};

/* global config object */
extern FmConfig* fm_config;

GType       fm_config_get_type      (void);
FmConfig*   fm_config_new           (void);

void fm_config_load_from_file(FmConfig* cfg, const char* name);

void fm_config_load_from_key_file(FmConfig* cfg, GKeyFile* kf);

void fm_config_save(FmConfig* cfg, const char* name);

void fm_config_emit_changed(FmConfig* cfg, const char* changed_key);

G_END_DECLS

#endif /* __FM_CONFIG_H__ */
