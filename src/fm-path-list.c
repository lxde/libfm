/*
 *      fm-path-list.c
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

#include "fm-path-list.h"

FmPathList* fm_path_list_new()
{
	FmPathList* pl = g_slice_new(FmPathList);
	g_queue_init(&pl->list);
	pl->n_ref = 1;
	return pl;
}

FmPathList* fm_path_list_new_from_uri_list(const char* uri_list)
{
	FmPathList* pl = fm_path_list_new();
	
	return pl;
}

FmPathList* fm_path_list_ref(FmPathList* pl)
{
	g_atomic_int_inc(&pl->n_ref);
	return pl;
}

void fm_path_list_unref(FmPathList* pl)
{
	if(g_atomic_int_dec_and_test(&pl->n_ref))
	{
		g_queue_foreach(&pl->list, (GFunc)fm_path_unref, NULL);
		g_slice_free(FmPathList, pl);
	}
}

char* fm_path_list_to_uri_list(FmPathList* pl)
{
	return NULL;
}

guint fm_path_list_get_length(FmPathList* pl)
{
	return pl->list.length;
}

void fm_path_list_add(FmPathList* pl, FmPath* path)
{
	g_queue_push_tail(&pl->list, fm_path_ref(path));
}
