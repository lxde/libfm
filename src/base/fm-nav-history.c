/*
 *      fm-nav-history.c
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

#include "fm-nav-history.h"

static void fm_nav_history_finalize  			(GObject *object);

G_DEFINE_TYPE(FmNavHistory, fm_nav_history, G_TYPE_OBJECT);


static void fm_nav_history_class_init(FmNavHistoryClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_nav_history_finalize;
}


static void fm_nav_history_finalize(GObject *object)
{
	FmNavHistory *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_NAV_HISTORY(object));

	self = FM_NAV_HISTORY(object);
    fm_list_unref(self->paths);

	G_OBJECT_CLASS(fm_nav_history_parent_class)->finalize(object);
}


static void fm_nav_history_init(FmNavHistory *self)
{
	self->paths = fm_path_list_new();
}


FmNavHistory *fm_nav_history_new(void)
{
	return g_object_new(FM_NAV_HISTORY_TYPE, NULL);
}


FmPathList* fm_nav_history_list(FmNavHistory* nh)
{
    return fm_list_ref(nh->paths);
}

FmPath* fm_nav_history_get_cur(FmNavHistory* nh)
{
    return nh->cur ? (FmPath*)nh->cur->data : NULL;
}

GList* fm_nav_history_get_cur_link(FmNavHistory* nh)
{
    return nh->cur;
}

gboolean fm_nav_history_get_can_forward(FmNavHistory* nh)
{
    return nh->cur ? (nh->cur->prev != NULL) : FALSE;
}

void fm_nav_history_forward(FmNavHistory* nh)
{
    if(nh->cur && nh->cur->prev)
        nh->cur = nh->cur->prev;
}

gboolean fm_nav_history_get_can_back(FmNavHistory* nh)
{
    return nh->cur ? (nh->cur->next != NULL) : FALSE;
}

void fm_nav_history_back(FmNavHistory* nh)
{
    if(nh->cur && nh->cur->next)
        nh->cur = nh->cur->next;
}

void fm_nav_history_chdir(FmNavHistory* nh, FmPath* path)
{
    FmPath* tmp;
    while( fm_list_peek_head_link(nh->paths) != nh->cur )
    {
        tmp = fm_list_pop_head(nh->paths);
        fm_path_unref(tmp);
    }
    tmp = nh->cur ? (FmPath*)nh->cur->data : NULL;
    if( !tmp || !fm_path_equal(tmp, path) )
    {
        fm_list_push_head(nh->paths, path);
        nh->cur = fm_list_peek_head_link(nh->paths);
    }
}

void fm_nav_history_jump(FmNavHistory* nh, GList* l)
{
    nh->cur = l;
}

void fm_nav_history_clear(FmNavHistory* nh)
{
    fm_list_clear(nh->paths);
}

void fm_nav_history_set_max(FmNavHistory* nh, guint num)
{
    nh->n_max = num;
    if(num >=0)
    {
        while(fm_list_get_length(nh->paths) > num)
        {
            FmPath* path = (FmPath*)fm_list_pop_tail(nh->paths);
            fm_path_unref(path);
        }
    }
}

