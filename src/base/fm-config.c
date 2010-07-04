/*
 *      fm-config.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-config.h"
#include "fm-utils.h"
#include <stdio.h>

enum
{
    CHANGED,
    N_SIGNALS
};

/* global config object */
FmConfig* fm_config = NULL;

static guint signals[N_SIGNALS];

static void fm_config_finalize              (GObject *object);

G_DEFINE_TYPE(FmConfig, fm_config, G_TYPE_OBJECT);


static void fm_config_class_init(FmConfigClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_config_finalize;

    /* when a config key is changed, the signal can be emitted with detail. */
    signals[CHANGED]=
        g_signal_new("changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST|G_SIGNAL_DETAILED,
                     G_STRUCT_OFFSET(FmConfigClass, changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

}


static void fm_config_finalize(GObject *object)
{
    FmConfig *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_FM_CONFIG(object));

    self = FM_CONFIG(object);

    G_OBJECT_CLASS(fm_config_parent_class)->finalize(object);
}


static void fm_config_init(FmConfig *self)
{
    self->single_click = FM_CONFIG_DEFAULT_SINGLE_CLICK;
    self->use_trash = FM_CONFIG_DEFAULT_USE_TRASH;
    self->confirm_del = FM_CONFIG_DEFAULT_CONFIRM_DEL;
    self->big_icon_size = FM_CONFIG_DEFAULT_BIG_ICON_SIZE;
    self->small_icon_size = FM_CONFIG_DEFAULT_SMALL_ICON_SIZE;
    self->pane_icon_size = FM_CONFIG_DEFAULT_PANE_ICON_SIZE;
    self->thumbnail_size = FM_CONFIG_DEFAULT_THUMBNAIL_SIZE;
    self->show_thumbnail = FM_CONFIG_DEFAULT_SHOW_THUMBNAIL;
    self->thumbnail_local = FM_CONFIG_DEFAULT_THUMBNAIL_LOCAL;
    self->thumbnail_max = FM_CONFIG_DEFAULT_THUMBNAIL_MAX;
}


FmConfig *fm_config_new(void)
{
    return (FmConfig*)g_object_new(FM_CONFIG_TYPE, NULL);
}

void fm_config_emit_changed(FmConfig* cfg, const char* changed_key)
{
    GQuark detail = changed_key ? g_quark_from_string(changed_key) : 0;
    g_signal_emit(cfg, signals[CHANGED], detail);
}

void fm_config_load_from_key_file(FmConfig* cfg, GKeyFile* kf)
{
    fm_key_file_get_bool(kf, "config", "use_trash", &cfg->use_trash);
    fm_key_file_get_bool(kf, "config", "single_click", &cfg->single_click);
    fm_key_file_get_bool(kf, "config", "confirm_del", &cfg->confirm_del);
    cfg->terminal = g_key_file_get_string(kf, "config", "terminal", NULL);
    cfg->archiver = g_key_file_get_string(kf, "config", "archiver", NULL);
    fm_key_file_get_int(kf, "config", "thumbnail_local", &cfg->thumbnail_local);
    fm_key_file_get_int(kf, "config", "thumbnail_max", &cfg->thumbnail_max);

#ifdef USE_UDISKS
    fm_key_file_get_bool(kf, "config", "show_internal_volumes", &cfg->show_internal_volumes);
#endif

    fm_key_file_get_int(kf, "ui", "big_icon_size", &cfg->big_icon_size);
    fm_key_file_get_int(kf, "ui", "small_icon_size", &cfg->small_icon_size);
    fm_key_file_get_int(kf, "ui", "pane_icon_size", &cfg->pane_icon_size);
    fm_key_file_get_int(kf, "ui", "thumbnail_size", &cfg->thumbnail_size);
    fm_key_file_get_int(kf, "ui", "show_thumbnail", &cfg->show_thumbnail);
}

void fm_config_load_from_file(FmConfig* cfg, const char* name)
{
    char **dirs, **dir;
    char *path;
    GKeyFile* kf = g_key_file_new();

    if(G_LIKELY(!name))
        name = "libfm/libfm.conf";
    else
    {
        if(G_UNLIKELY(g_path_is_absolute(name)))
        {
            if(g_key_file_load_from_file(kf, name, 0, NULL))
                fm_config_load_from_key_file(cfg, kf);
            goto _out;
        }
    }

    dirs = g_get_system_config_dirs(), **dir;
    for(dir=dirs;*dir;++dir)
    {
        path = g_build_filename(*dir, name, NULL);
        if(g_key_file_load_from_file(kf, path, 0, NULL))
            fm_config_load_from_key_file(cfg, kf);
        g_free(path);
    }
    path = g_build_filename(g_get_user_config_dir(), name, NULL);
    if(g_key_file_load_from_file(kf, path, 0, NULL))
        fm_config_load_from_key_file(cfg, kf);
    g_free(path);

_out:
    g_key_file_free(kf);
    g_signal_emit(cfg, signals[CHANGED], 0);
}

void fm_config_save(FmConfig* cfg, const char* name)
{
    char* path = NULL;;
    char* dir_path;
    FILE* f;
    if(!name)
        name = path = g_build_filename(g_get_user_config_dir(), "libfm/libfm.conf", NULL);
    else if(!g_path_is_absolute(name))
        name = path = g_build_filename(g_get_user_config_dir(), name, NULL);

    dir_path = g_path_get_dirname(name);
    if(g_mkdir_with_parents(dir_path, 0700) != -1)
    {
        f = fopen(name, "w");
        if(f)
        {
            fputs("[config]\n", f);
            fprintf(f, "single_click=%d\n", cfg->single_click);
            fprintf(f, "use_trash=%d\n", cfg->use_trash);
            fprintf(f, "confirm_del=%d\n", cfg->confirm_del);
#ifdef USE_UDISKS
            fprintf(f, "show_internal_volumes=%d\n", cfg->show_internal_volumes);
#endif

            if(cfg->terminal)
                fprintf(f, "terminal=%s\n", cfg->terminal);
            if(cfg->archiver)
                fprintf(f, "archiver=%s\n", cfg->archiver);
            fprintf(f, "thumbnail_local=%d\n", cfg->thumbnail_local);
            fprintf(f, "thumbnail_max=%d\n", cfg->thumbnail_max);
            fputs("\n[ui]\n", f);
            fprintf(f, "big_icon_size=%d\n", cfg->big_icon_size);
            fprintf(f, "small_icon_size=%d\n", cfg->small_icon_size);
            fprintf(f, "pane_icon_size=%d\n", cfg->pane_icon_size);
            fprintf(f, "thumbnail_size=%d\n", cfg->thumbnail_size);
            fprintf(f, "show_thumbnail=%d\n", cfg->show_thumbnail);
            fclose(f);
        }
    }
    g_free(dir_path);
    g_free(path);
}

