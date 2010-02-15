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

static void fm_nav_history_item_free(FmNavHistoryItem* item)
{
    fm_path_unref(item->path);
    g_slice_free(FmNavHistoryItem, item);
}

static void fm_nav_history_finalize(GObject *object)
{
	FmNavHistory *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_NAV_HISTORY(object));

	self = FM_NAV_HISTORY(object);
    g_queue_foreach(&self->items, (GFunc)fm_nav_history_item_free, NULL);
    g_queue_clear(&self->items);

	G_OBJECT_CLASS(fm_nav_history_parent_class)->finalize(object);
}

static void fm_nav_history_init(FmNavHistory *self)
{
	g_queue_init(&self->items);
}


FmNavHistory *fm_nav_history_new(void)
{
	return g_object_new(FM_NAV_HISTORY_TYPE, NULL);
}

/* The returned GList belongs to FmNavHistory and shouldn't be freed. */
GList* fm_nav_history_list(FmNavHistory* nh)
{
    return nh->items.head;
}

const FmNavHistoryItem* fm_nav_history_get_cur(FmNavHistory* nh)
{
    return nh->cur ? (FmNavHistoryItem*)nh->cur->data : NULL;
}

GList* fm_nav_history_get_cur_link(FmNavHistory* nh)
{
    return nh->cur;
}

gboolean fm_nav_history_get_can_forward(FmNavHistory* nh)
{
    return nh->cur ? (nh->cur->prev != NULL) : FALSE;
}

void fm_nav_history_forward(FmNavHistory* nh, int old_scroll_pos)
{
    FmNavHistoryItem* tmp = nh->cur ? (FmNavHistoryItem*)nh->cur->data : NULL;
    if(tmp) /* remember current scroll pos */
        tmp->scroll_pos = old_scroll_pos;

    if(nh->cur && nh->cur->prev)
        nh->cur = nh->cur->prev;
}

gboolean fm_nav_history_get_can_back(FmNavHistory* nh)
{
    return nh->cur ? (nh->cur->next != NULL) : FALSE;
}

void fm_nav_history_back(FmNavHistory* nh, int old_scroll_pos)
{
    FmNavHistoryItem* tmp = nh->cur ? (FmNavHistoryItem*)nh->cur->data : NULL;
    if(tmp) /* remember current scroll pos */
        tmp->scroll_pos = old_scroll_pos;

    if(nh->cur && nh->cur->next)
        nh->cur = nh->cur->next;
}

void fm_nav_history_chdir(FmNavHistory* nh, FmPath* path, int old_scroll_pos)
{
    FmNavHistoryItem* tmp;

    /* if we're not at the top of the queue, remove all items beyond us. */
    while(nh->items.head != nh->cur)
    {
        tmp = g_queue_pop_head(&nh->items);
        fm_nav_history_item_free(tmp);
    }

    /* now we're at the top of the queue. */
    tmp = nh->cur ? (FmNavHistoryItem*)nh->cur->data : NULL;
    if(tmp) /* remember current scroll pos */
        tmp->scroll_pos = old_scroll_pos;

    if( !tmp || !fm_path_equal(tmp->path, path) ) /* we're not chdir to the same path */
    {
        tmp = g_queue_peek_head(&nh->items);

        /* add a new item */
        tmp = g_slice_new0(FmNavHistoryItem);
        tmp->path = fm_path_ref(path);
        g_queue_push_head(&nh->items, tmp);
        nh->cur = fm_list_peek_head_link(&nh->items);
    }
}

void fm_nav_history_jump(FmNavHistory* nh, GList* l, int old_scroll_pos)
{
    FmNavHistoryItem* tmp = nh->cur ? (FmNavHistoryItem*)nh->cur->data : NULL;
    if(tmp) /* remember current scroll pos */
        tmp->scroll_pos = old_scroll_pos;

    nh->cur = l;
}

void fm_nav_history_clear(FmNavHistory* nh)
{
    g_queue_foreach(&nh->items, (GFunc)fm_nav_history_item_free, NULL);
    g_queue_clear(&nh->items);
}

void fm_nav_history_set_max(FmNavHistory* nh, guint num)
{
    nh->n_max = num;
    if(num >=0)
    {
        while(g_queue_get_length(&nh->items) > num)
        {
            FmNavHistoryItem* item = (FmNavHistoryItem*)g_queue_pop_tail(&nh->items);
            fm_nav_history_item_free(item);
        }
    }
}

