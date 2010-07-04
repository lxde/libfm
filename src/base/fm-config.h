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
#define IS_FM_CONFIG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_CONFIG_TYPE))
#define IS_FM_CONFIG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_CONFIG_TYPE))

typedef struct _FmConfig            FmConfig;
typedef struct _FmConfigClass       FmConfigClass;

#define     FM_CONFIG_DEFAULT_SINGLE_CLICK      FALSE
#define     FM_CONFIG_DEFAULT_USE_TRASH         TRUE
#define     FM_CONFIG_DEFAULT_CONFIRM_DEL       TRUE

#define     FM_CONFIG_DEFAULT_BIG_ICON_SIZE     48
#define     FM_CONFIG_DEFAULT_SMALL_ICON_SIZE   16
#define     FM_CONFIG_DEFAULT_PANE_ICON_SIZE    16
#define     FM_CONFIG_DEFAULT_THUMBNAIL_SIZE    128

#define     FM_CONFIG_DEFAULT_SHOW_THUMBNAIL    TRUE
#define     FM_CONFIG_DEFAULT_THUMBNAIL_LOCAL   TRUE
#define     FM_CONFIG_DEFAULT_THUMBNAIL_MAX     2048

struct _FmConfig
{
    GObject parent;

    gboolean single_click; /* single click to open file */
    gboolean use_trash; /* delete file to trash can */
    gboolean confirm_del; /* ask before deleting files */

    guint big_icon_size;    /* size of big icons */
    guint small_icon_size;  /* size of small icons */
    guint pane_icon_size;   /* size of side pane icons */
    guint thumbnail_size;   /* size of thumbnail icons */

    gboolean show_thumbnail; /* show thumbnails */
    gboolean thumbnail_local; /* show thumbnails for local files only */
    guint thumbnail_max;    /* show thumbnails for files smaller than 'thumb_max' KB */

    gboolean show_internal_volumes; /* show system internal volumes in side pane. (udisks-only)*/

    char* terminal; /* command line to launch terminal emulator */
    gboolean si_unit;   /* use SI prefix for file sizes */

    char* archiver; /* desktop_id of the archiver used */
};

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
