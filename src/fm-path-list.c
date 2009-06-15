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

static FmListFuncs funcs = 
{
	fm_path_ref,
	fm_path_unref
};

FmPathList* fm_path_list_new()
{
	return (FmPathList*)fm_list_new(&funcs);
}

FmPathList* fm_path_list_new_from_uri_list(const char* uri_list)
{
	FmPathList* pl = fm_path_list_new();	
	return pl;
}

char* fm_path_list_to_uri_list(FmPathList* pl)
{
	return NULL;
}

FmPathList* fm_path_list_new_from_file_info_list(GList* fis)
{
	FmPathList* list = fm_path_list_new();
	GList* l;
	for(l=fis;l;l=l->next)
	{
		FmFileInfo* fi = (FmFileInfo*)l->data;
		fm_list_push_tail(list, fi->path);
	}
	return list;
}

FmPathList* fm_path_list_new_from_file_info_slist(GSList* fis)
{
	FmPathList* list = fm_path_list_new();
	GSList* l;
	for(l=fis;l;l=l->next)
	{
		FmFileInfo* fi = (FmFileInfo*)l->data;
		fm_list_push_tail(list, fi->path);
	}
	return list;
}
