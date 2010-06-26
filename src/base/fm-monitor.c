/*
 *      fm-file-monitor.c
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

#include "fm-monitor.h"
#include "fm-dummy-monitor.h"
#include <string.h>

#define MONITOR_RATE_LIMIT 5000

static GHashTable* hash = NULL;
static GHashTable* dummy_hash = NULL;
G_LOCK_DEFINE_STATIC(hash);

static void on_monitor_destroy(GFile* gf, GFileMonitor* mon)
{
    G_LOCK(hash);
    g_hash_table_remove(hash, gf);
    G_UNLOCK(hash);
}

static void on_dummy_monitor_destroy(GFile* gf, GFileMonitor* mon)
{
    G_LOCK(hash);
    g_hash_table_remove(dummy_hash, gf);
    G_UNLOCK(hash);
}

GFileMonitor* fm_monitor_directory(GFile* gf, GError** err)
{
    GFileMonitor* ret = NULL;
    G_LOCK(hash);
    ret = (GFileMonitor*)g_hash_table_lookup(hash, gf);
    if(!ret && !g_file_is_native(gf))
        ret = (GFileMonitor*)g_hash_table_lookup(dummy_hash, gf);
    if(ret)
        g_object_ref(ret);
    else
    {
        GError* e = NULL;
        ret = g_file_monitor_directory(gf, G_FILE_MONITOR_WATCH_MOUNTS, NULL, &e);
        if(ret)
        {
            g_object_weak_ref(G_OBJECT(ret), (GWeakNotify)on_monitor_destroy, gf);
            g_file_monitor_set_rate_limit(ret, MONITOR_RATE_LIMIT);
            g_hash_table_insert(hash, g_object_ref(gf), ret);
        }
        else
        {
            if(e)
            {
                if(e->domain == G_IO_ERROR && e->code == G_IO_ERROR_NOT_SUPPORTED)
                {
                    /* create a fake file monitor */
                    ret = fm_dummy_monitor_new();
                    g_error_free(e);
                    g_object_weak_ref(G_OBJECT(ret), (GWeakNotify)on_dummy_monitor_destroy, gf);
                    g_hash_table_insert(dummy_hash, g_object_ref(gf), ret);
                }
                else
                {
                    g_debug("error creating file monitor: %S", e->message);
                    G_UNLOCK(hash);
                    if(err)
                        *err = e;
                    else
                        g_error_free(e);
                    return NULL;
                }
            }
        }
    }
    G_UNLOCK(hash);
    return ret;
}

void _fm_monitor_init()
{
    hash = g_hash_table_new_full(g_file_hash, g_file_equal, g_object_unref, NULL);
    dummy_hash = g_hash_table_new_full(g_file_hash, g_file_equal, g_object_unref, NULL);
}

void _fm_monitor_finalize()
{
    g_hash_table_destroy(hash);
    g_hash_table_destroy(dummy_hash);
    hash = NULL;
    dummy_hash = NULL;
}

GFileMonitor* fm_monitor_lookup_monitor(GFile* gf)
{
    GFileMonitor* ret = NULL;
    if(G_UNLIKELY(!gf))
        return NULL;
    G_LOCK(hash);
    ret = (GFileMonitor*)g_hash_table_lookup(hash, gf);
    if(!ret && !g_file_is_native(gf))
        ret = (GFileMonitor*)g_hash_table_lookup(dummy_hash, gf);
    if(ret)
        g_object_ref(ret);
    G_UNLOCK(hash);
    return ret;
}

GFileMonitor* fm_monitor_lookup_dummy_monitor(GFile* gf)
{
    GFileMonitor* mon;
    char* scheme;
    if(G_LIKELY(!gf || g_file_is_native(gf)))
        return NULL;
    scheme = g_file_get_uri_scheme(gf);
    if(scheme)
    {
        /* those URI schemes don't need dummy monitor */
        if(strcmp(scheme, "trash") == 0
         || strcmp(scheme, "computer") == 0
         || strcmp(scheme, "network") == 0
         || strcmp(scheme, "applications") == 0)
        {
            g_free(scheme);
            return NULL;
        }
        g_free(scheme);
    }
    G_LOCK(hash);
    mon = (GFileMonitor*)g_hash_table_lookup(dummy_hash, gf);
    if(mon)
        g_object_ref(mon);
    else
    {
        /* create a fake file monitor */
        mon = fm_dummy_monitor_new();
        g_object_weak_ref(G_OBJECT(mon), (GWeakNotify)on_dummy_monitor_destroy, gf);
        g_hash_table_insert(dummy_hash, g_object_ref(gf), mon);
    }
    G_UNLOCK(hash);
    return mon;
}
