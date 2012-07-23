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

/**
 * SECTION:fm-nav-history
 * @short_description: Simple navigation history management.
 * @title: FmNavHistory
 *
 * @include: libfm/fm-nav-history.h
 *
 * The #FmNavHistory object implements history for paths that were
 * entered in some input bar and allows to add, remove or move items in it.
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
	g_return_if_fail(FM_IS_NAV_HISTORY(object));

	self = FM_NAV_HISTORY(object);
    g_queue_foreach(&self->items, (GFunc)fm_nav_history_item_free, NULL);
    g_queue_clear(&self->items);

	G_OBJECT_CLASS(fm_nav_history_parent_class)->finalize(object);
}

static void fm_nav_history_init(FmNavHistory *self)
{
	g_queue_init(&self->items);
}

/**
 * fm_nav_history_new
 *
 * Creates a new #FmNavHistory object with empty history.
 *
 * Returns: a new #FmNavHistory object.
 *
 * Since: 0.1.0
 */
FmNavHistory *fm_nav_history_new(void)
{
	return g_object_new(FM_NAV_HISTORY_TYPE, NULL);
}

/**
 * fm_nav_history_list
 * @nh: the history
 *
 * Retrieves full list of the history as #GList of #FmNavHistoryItem.
 * The returned #GList belongs to #FmNavHistory and shouldn't be freed.
 *
 * Returns: (transfer none): (element-type #FmNavHistoryItem): full history.
 *
 * Since: 0.1.0
 */
const GList* fm_nav_history_list(FmNavHistory* nh)
{
    return nh->items.head;
}

/**
 * fm_nav_history_get_cur
 * @nh: the history
 *
 * Retrieves current selected item of the history. The returned item
 * belongs to #FmNavHistory and shouldn't be freed by caller.
 *
 * Returns: (transfer none): current item.
 *
 * Since: 0.1.0
 */
const FmNavHistoryItem* fm_nav_history_get_cur(FmNavHistory* nh)
{
    return nh->cur ? (FmNavHistoryItem*)nh->cur->data : NULL;
}

/**
 * fm_nav_history_get_cur_link
 * @nh: the history
 *
 * Retrieves current selected item as #GList element containing
 * #FmNavHistoryItem. The returned item belongs to #FmNavHistory and
 * shouldn't be freed by caller.
 *
 * Returns: (transfer none): (element-type #FmNavHistoryItem): current item.
 *
 * Since: 0.1.0
 */
const GList* fm_nav_history_get_cur_link(FmNavHistory* nh)
{
    return nh->cur;
}

/**
 * fm_nav_history_can_forward
 * @nh: the history
 *
 * Checks if current selected item is the last item in the history.
 *
 * Before 1.0.0 this call had name fm_nav_history_get_can_forward.
 *
 * Returns: %TRUE if cursor can go forward in history.
 *
 * Since: 0.1.0
 */
gboolean fm_nav_history_can_forward(FmNavHistory* nh)
{
    return nh->cur ? (nh->cur->prev != NULL) : FALSE;
}

/**
 * fm_nav_history_forward
 * @nh: the history
 * @old_scroll_pos: the scroll position to associate with current item
 *
 * If there is a previous item in the history then sets @old_scroll_pos
 * into current item data and marks previous item current.
 *
 * Since: 0.1.0
 */
void fm_nav_history_forward(FmNavHistory* nh, int old_scroll_pos)
{
    if(nh->cur && nh->cur->prev)
    {
		FmNavHistoryItem* tmp = (FmNavHistoryItem*)nh->cur->data;
		if(tmp) /* remember current scroll pos */
			tmp->scroll_pos = old_scroll_pos;
        nh->cur = nh->cur->prev;
	}
}

/**
 * fm_nav_history_can_back
 * @nh: the history
 *
 * Checks if current selected item is the first item in the history.
 *
 * Before 1.0.0 this call had name fm_nav_history_get_can_back.
 *
 * Returns: %TRUE if cursor can go backward in history.
 *
 * Since: 0.1.0
 */
gboolean fm_nav_history_can_back(FmNavHistory* nh)
{
    return nh->cur ? (nh->cur->next != NULL) : FALSE;
}

/**
 * fm_nav_history_back
 * @nh: the history
 * @old_scroll_pos: the scroll position to associate with current item
 *
 * If there is a next item in the history then sets @old_scroll_pos into
 * current item data and marks next item current.
 *
 * Since: 0.1.0
 */
void fm_nav_history_back(FmNavHistory* nh, int old_scroll_pos)
{
	if(nh->cur && nh->cur->next)
	{
		FmNavHistoryItem* tmp = (FmNavHistoryItem*)nh->cur->data;
		if(tmp) /* remember current scroll pos */
			tmp->scroll_pos = old_scroll_pos;
        nh->cur = nh->cur->next;
	}
}

/**
 * fm_nav_history_chdir
 * @nh: the history
 * @path: new path to add
 * @old_scroll_pos: the scroll position to associate with current item
 *
 * Sets @old_scroll_pos into current item data and then adds new @path
 * to the beginning of the @nh.
 *
 * Since: 0.1.0
 */
/* FIXME: it's too dirty, need to redesign it later */
void fm_nav_history_chdir(FmNavHistory* nh, FmPath* path, int old_scroll_pos)
{
    FmNavHistoryItem* tmp;

    /* if we're not at the top of the queue, remove all items beyond us. */
    while(nh->items.head != nh->cur && !g_queue_is_empty(&nh->items))
    {
		/* FIXME: #3411314: pcmanfm crash on unmount.
		 * While tracing the bug, I noted that sometimes nh->items
		 * becomes empty, but nh->cur still points to somewhere.
		 * The cause is not yet known. */
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
        nh->cur = g_queue_peek_head_link(&nh->items);
    }
}

/**
 * fm_nav_history_jump
 * @nh: the history
 * @l: (element-type #FmNavHistoryItem): new current item
 * @old_scroll_pos: the scroll position to associate with current item
 *
 * Sets @old_scroll_pos into current item data and then
 * sets current item of @nh to one from @l.
 *
 * Since: 0.1.0
 */
/* FIXME: it's too dangerous, need to redesign it later */
void fm_nav_history_jump(FmNavHistory* nh, GList* l, int old_scroll_pos)
{
    FmNavHistoryItem* tmp = nh->cur ? (FmNavHistoryItem*)nh->cur->data : NULL;
    if(tmp) /* remember current scroll pos */
        tmp->scroll_pos = old_scroll_pos;

    nh->cur = l;
}

/**
 * fm_nav_history_clear
 * @nh: the history
 *
 * Removes all items from the history @nh.
 *
 * Since: 0.1.0
 */
void fm_nav_history_clear(FmNavHistory* nh)
{
    g_queue_foreach(&nh->items, (GFunc)fm_nav_history_item_free, NULL);
    g_queue_clear(&nh->items);
}

/**
 * fm_nav_history_set_max
 * @nh: the history
 * @num: new size of history
 *
 * Sets maximum length of the history @nh to be @num.
 *
 * Since: 0.1.0
 */
void fm_nav_history_set_max(FmNavHistory* nh, guint num)
{
    nh->n_max = num;
    if(num < 1)
		num = 1;
	while(g_queue_get_length(&nh->items) > num)
	{
		FmNavHistoryItem* item = (FmNavHistoryItem*)g_queue_pop_tail(&nh->items);
		fm_nav_history_item_free(item);
		/* FIXME: nh->cur may become invalid!!! */
	}
}

