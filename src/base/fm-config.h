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

#define FM_CONFIG_TYPE				(fm_config_get_type())
#define FM_CONFIG(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_CONFIG_TYPE, FmConfig))
#define FM_CONFIG_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_CONFIG_TYPE, FmConfigClass))
#define IS_FM_CONFIG(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_CONFIG_TYPE))
#define IS_FM_CONFIG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_CONFIG_TYPE))

typedef struct _FmConfig			FmConfig;
typedef struct _FmConfigClass		FmConfigClass;

struct _FmConfig
{
	GObject parent;

    gboolean single_click;
    gboolean use_trash;
    gboolean confirm_del;

    guint big_icon_size;
    guint small_icon_size;
};

struct _FmConfigClass
{
	GObjectClass parent_class;
    void (*changed)(FmConfig* cfg);
};

/* global config object */
extern FmConfig* fm_config;

GType		fm_config_get_type		(void);
FmConfig*	fm_config_new			(void);

void fm_config_load_from_file(FmConfig* cfg, const char* name);

void fm_config_load_from_key_file(FmConfig* cfg, GKeyFile* kf);

void fm_config_save(FmConfig* cfg, const char* name);

void fm_config_emit_changed(FmConfig* cfg);

G_END_DECLS

#endif /* __FM_CONFIG_H__ */
