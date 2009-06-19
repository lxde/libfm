/*
 *      fm-list.h
 *      A generic list container supporting reference counting.
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


#ifndef __FM_LIST_H__
#define __FM_LIST_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _FmList			FmList;
typedef struct _FmListFuncs		FmListFuncs;

struct _FmList
{
	GQueue list;
	FmListFuncs* funcs;
	gint n_ref;
};

struct _FmListFuncs
{
	gpointer (*item_ref)(gpointer item);
	void (*item_unref)(gpointer item);
};

FmList* fm_list_new(FmListFuncs* funcs);

FmList* fm_list_ref(gpointer list);
void fm_list_unref(gpointer list);

#define FM_LIST(l)	((FmList*)l)

/* Since FmList is actually a GQueue with reference counting,
 * all APIs for GQueue should be usable */

void fm_list_clear(gpointer list);
#define fm_list_is_empty(l)				g_queue_is_empty((GQueue*)l)
#define fm_list_get_length(l)			g_queue_get_length((GQueue*)l)

#define fm_list_reverse(l)				g_queue_reverse((GQueue*)l)
#define fm_list_foreach(l,f,d)			g_queue_foreach((GQueue*)l,f,d)
#define fm_list_find(l,d)				g_queue_find((GQueue*)l,d)
#define fm_list_find_custom(l,d,f)		g_queue_find_custom((GQueue*)l,d,f)
#define fm_list_sort(l,f,d)				g_queue_sort((GQueue*)l,f,d)

#define fm_list_push_head(l,d)			g_queue_push_head((GQueue*)l,l->funcs->item_ref(d))
#define fm_list_push_tail(l,d)			g_queue_push_tail((GQueue*)l,l->funcs->item_ref(d))
#define fm_list_push_nth(l,d,n)			g_queue_push_nth((GQueue*)l,l->funcs->item_ref(d),n)

#define fm_list_push_head_noref(l,d)			g_queue_push_head((GQueue*)l,d)
#define fm_list_push_tail_noref(l,d)			g_queue_push_tail((GQueue*)l,d)
#define fm_list_push_nth_noref(l,d,n)			g_queue_push_nth((GQueue*)l,d,n)

#define fm_list_pop_head(l)				g_queue_pop_head((GQueue*)l)
#define fm_list_pop_tail(l)				g_queue_pop_tail((GQueue*)l)
#define fm_list_pop_nth(l,n)			g_queue_pop_nth((GQueue*)l,n)

#define fm_list_peek_head(l)			g_queue_peek_head((GQueue*)l)
#define fm_list_peek_tail(l)			g_queue_peek_tail((GQueue*)l)
#define fm_list_peek_nth(l,n)			g_queue_peek_nth((GQueue*)l,n)

#define fm_list_index(l,d)				g_queue_index((GQueue*)l,d)

void fm_list_remove(gpointer list, gpointer data);
void fm_list_remove_all(gpointer list, gpointer data);
#define fm_list_insert_before(l,s,d)	g_queue_insert_before((GQueue*)l,s,l->funcs->item_ref(d))
#define fm_list_insert_after(l,s,d)		g_queue_insert_after((GQueue*)l,s,l->funcs->item_ref(d))
#define fm_list_insert_sorted(l,d,f,u)	g_queue_insert_sorted((GQueue*)l,l->funcs->item_ref(d),f,u)

#define fm_list_insert_before_noref(l,s,d)	g_queue_insert_before((GQueue*)l,s,d)
#define fm_list_insert_after_noref(l,s,d)		g_queue_insert_after((GQueue*)l,s,d)
#define fm_list_insert_sorted_noref(l,d,f,u)	g_queue_insert_sorted((GQueue*)l,d,f,u)

#define fm_list_push_head_link(l,l_)	g_queue_push_head_link((GQueue*)l,l_)
#define fm_list_push_tail_link(l,l_)	g_queue_push_tail_link((GQueue*)l,l_)
#define fm_list_push_nth_link(l,n,l_)	g_queue_push_nth_link((GQueue*)l,n,l_)

#define fm_list_pop_head_link(l)		g_queue_pop((GQueue*)l)
#define fm_list_pop_tail_link(l)		g_queue_pop_tail_link((GQueue*)l)
#define fm_list_pop_nth_link(l,n)		g_queue_pop_nth_link((GQueue*)l,n)

#define fm_list_peek_head_link(l)		g_queue_peek_head_link((GQueue*)l)
#define fm_list_peek_tail_link(l)		g_queue_peek_tail_link((GQueue*)l)
#define fm_list_peek_nth_link(l,n)		g_queue_peek_nth_link((GQueue*)l,n)

#define fm_list_link_index(l,l_)		g_queue_index((GQueue*)l,l_)
#define fm_list_unlink(l,l_)			g_queue_unlink((GQueue*)l,l_)
void fm_list_delete_link(gpointer list, gpointer l_);

G_END_DECLS

#endif /* __FM_LIST_H__ */
