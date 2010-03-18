/*
 *      fm-icon.c
 *
 *      Copyright 2009 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include "fm-icon.h"

static GHashTable* hash = NULL;
G_LOCK_DEFINE_STATIC(hash);

static GDestroyNotify destroy_func = NULL;

void _fm_icon_init()
{
    if(G_UNLIKELY(hash))
        return;
    hash = g_hash_table_new(g_icon_hash, g_icon_equal);
}

void _fm_icon_finalize()
{
    g_hash_table_destroy(hash);
    hash = NULL;
}

FmIcon* fm_icon_from_gicon(GIcon* gicon)
{
    FmIcon* icon;
    G_LOCK(hash);
    icon = (FmIcon*)g_hash_table_lookup(hash, gicon);
    if(G_UNLIKELY(!icon))
    {
        icon = g_slice_new0(FmIcon);
        icon->gicon = (GIcon*)g_object_ref(gicon);
        g_hash_table_insert(hash, icon->gicon, icon);
    }
    ++icon->n_ref;
    G_UNLOCK(hash);
    return icon;
}

FmIcon* fm_icon_from_name(const char* name)
{
    if(G_LIKELY(name))
    {
        FmIcon* icon;
        GIcon* gicon;
        if(g_path_is_absolute(name))
        {
            GFile* gicon_file = g_file_new_for_path(name);
            gicon = g_file_icon_new(gicon_file);
            g_object_unref(gicon_file);
        }
        else
            gicon = g_themed_icon_new(name);

        if(G_LIKELY(gicon))
        {
            icon = fm_icon_from_gicon(gicon);
            g_object_unref(gicon);
            return icon;
        }
    }
    return NULL;
}

/* FIXME: using mutex is a little bit expansive, but since we need
 * to handle hash table too, it might be necessary. */
FmIcon* fm_icon_ref(FmIcon* icon)
{
    G_LOCK(hash);
    ++icon->n_ref;
    G_UNLOCK(hash);
    return icon;
}

/* FIXME: what will happen if someone is ref this structure while we're
 * trying to free it? */
void fm_icon_unref(FmIcon* icon)
{
    G_LOCK(hash);
    --icon->n_ref;
    if(G_UNLIKELY(0 == icon->n_ref))
    {
        g_hash_table_remove(hash, icon->gicon);
        G_UNLOCK(hash);
        g_object_unref(icon->gicon);
        if(destroy_func && icon->user_data)
            destroy_func(icon->user_data);
        g_slice_free(FmIcon, icon);
    }
    else
        G_UNLOCK(hash);
}

static gboolean unload_cache(GIcon* key, FmIcon* icon, gpointer unused)
{
    --icon->n_ref;
    if(G_UNLIKELY(0 == icon->n_ref))
    {
        g_object_unref(icon->gicon);
        if(destroy_func && icon->user_data)
            destroy_func(icon->user_data);
        g_slice_free(FmIcon, icon);
    }
    return TRUE;
}

void fm_icon_unload_cache()
{
    G_LOCK(hash);
    g_hash_table_foreach_remove(hash, (GHRFunc)unload_cache, NULL);
    G_UNLOCK(hash);
}

void unload_user_data_cache(GIcon* key, FmIcon* icon, gpointer unused)
{
    if(destroy_func && icon->user_data)
    {
        destroy_func(icon->user_data);
        icon->user_data = NULL;
    }
}

void fm_icon_unload_user_data_cache()
{
    G_LOCK(hash);
    g_hash_table_foreach(hash, (GHFunc)unload_user_data_cache, NULL);
    G_UNLOCK(hash);
}

gpointer fm_icon_get_user_data(FmIcon* icon)
{
    return icon->user_data;
}

void fm_icon_set_user_data(FmIcon* icon, gpointer user_data)
{
    icon->user_data = user_data;
}

void fm_icon_set_user_data_destroy(GDestroyNotify func)
{
    destroy_func = func;
}
