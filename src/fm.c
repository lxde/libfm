/*
 *      fm.c
 *      
 *      Copyright 2009 PCMan <pcman@thinkpad>
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

#include "fm.h"

gboolean fm_init(FmConfig* config)
{
    char* path;
	g_thread_init(NULL);
	g_thread_pool_set_max_idle_time(10000); /* is 10 sec enough? */

    if(config)
        fm_config = (FmConfig*)g_object_ref(config);
    else
    {
        /* create default config object */
        fm_config = fm_config_new();
        fm_config_load_from_file(fm_config, NULL);
    }

	fm_path_init();
    fm_icon_init();
    fm_monitor_init();
    fm_file_info_init();
    
    /* override gnome-terminal */
    path = g_strconcat(PACKAGE_LIB_DIR ":", g_getenv("PATH"), NULL);
    g_setenv("PATH", path, TRUE);
    g_free(path);
}

void fm_finalize()
{
	fm_icon_finalize();
    fm_monitor_finalize();

    fm_config_save(fm_config, NULL);
    g_object_unref(fm_config);
    fm_config = NULL;
}
