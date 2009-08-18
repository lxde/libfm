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

#define FM_LIST(list)	((FmList*)list)

/* Since FmList is actually a GQueue with reference counting,
 * all APIs for GQueue should be usable */

void fm_list_clear(gpointer list);
#define fm_list_is_empty(list)				g_queue_is_empty((GQueue*)list)
#define fm_list_get_length(list)			g_queue_get_length((GQueue*)list)

#define fm_list_reverse(list)				g_queue_reverse((GQueue*)list)
#define fm_list_foreach(list,f,d)			g_queue_foreach((GQueue*)list,f,d)
#define fm_list_find(list,d)				g_queue_find((GQueue*)list,d)
#define fm_list_find_custom(list,d,f)		g_queue_find_custom((GQueue*)list,d,f)
#define fm_list_sort(list,f,d)				g_queue_sort((GQueue*)list,f,d)

#define fm_list_push_head(list,d)			g_queue_push_head((GQueue*)list,list->funcs->item_ref(d))
#define fm_list_push_tail(list,d)			g_queue_push_tail((GQueue*)list,list->funcs->item_ref(d))
#define fm_list_push_nth(list,d,n)			g_queue_push_nth((GQueue*)list,list->funcs->item_ref(d),n)

#define fm_list_push_head_noref(list,d)			g_queue_push_head((GQueue*)list,d)
#define fm_list_push_tail_noref(list,d)			g_queue_push_tail((GQueue*)list,d)
#define fm_list_push_nth_noref(list,d,n)			g_queue_push_nth((GQueue*)list,d,n)

#define fm_list_pop_head(list)				g_queue_pop_head((GQueue*)list)
#define fm_list_pop_tail(list)				g_queue_pop_tail((GQueue*)list)
#define fm_list_pop_nth(list,n)			g_queue_pop_nth((GQueue*)list,n)

#define fm_list_peek_head(list)			g_queue_peek_head((GQueue*)list)
#define fm_list_peek_tail(list)			g_queue_peek_tail((GQueue*)list)
#define fm_list_peek_nth(list,n)			g_queue_peek_nth((GQueue*)list,n)

#define fm_list_index(list,d)				g_queue_index((GQueue*)list,d)

void fm_list_remove(gpointer list, gpointer data);
void fm_list_remove_all(gpointer list, gpointer data);
#define fm_list_insert_before(list,s,d)	g_queue_insert_before((GQueue*)list,s,list->funcs->item_ref(d))
#define fm_list_insert_after(list,s,d)		g_queue_insert_after((GQueue*)list,s,list->funcs->item_ref(d))
#define fm_list_insert_sorted(list,d,f,u)	g_queue_insert_sorted((GQueue*)list,list->funcs->item_ref(d),f,u)

#define fm_list_insert_before_noref(list,s,d)	g_queue_insert_before((GQueue*)list,s,d)
#define fm_list_insert_after_noref(list,s,d)		g_queue_insert_after((GQueue*)list,s,d)
#define fm_list_insert_sorted_noref(list,d,f,u)	g_queue_insert_sorted((GQueue*)list,d,f,u)

#define fm_list_push_head_link(list,l_)	g_queue_push_head_link((GQueue*)list,l_)
#define fm_list_push_tail_link(list,l_)	g_queue_push_tail_link((GQueue*)list,l_)
#define fm_list_push_nth_link(list,n,l_)	g_queue_push_nth_link((GQueue*)list,n,l_)

#define fm_list_pop_head_link(list)		g_queue_pop((GQueue*)list)
#define fm_list_pop_tail_link(list)		g_queue_pop_tail_link((GQueue*)list)
#define fm_list_pop_nth_link(list,n)		g_queue_pop_nth_link((GQueue*)list,n)

#define fm_list_peek_head_link(list)		g_queue_peek_head_link((GQueue*)list)
#define fm_list_peek_tail_link(list)		g_queue_peek_tail_link((GQueue*)list)
#define fm_list_peek_nth_link(list,n)		g_queue_peek_nth_link((GQueue*)list,n)

#define fm_list_link_index(list,l_)		g_queue_index((GQueue*)list,l_)
#define fm_list_unlink(list,l_)			g_queue_unlink((GQueue*)list,l_)
#define fm_list_delete_link_nounref(list, l_)    g_queue_delete_link((GQueue*)list,l_)
void fm_list_delete_link(gpointer list, gpointer l_);

G_END_DECLS

#endif /* __FM_LIST_H__ */
