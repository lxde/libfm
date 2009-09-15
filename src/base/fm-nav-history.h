/*
 *      fm-nav-history.h
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


#ifndef __FM_NAV_HISTORY_H__
#define __FM_NAV_HISTORY_H__

#include <glib-object.h>
#include "fm-list.h"
#include "fm-path.h"

G_BEGIN_DECLS

#define FM_NAV_HISTORY_TYPE				(fm_nav_history_get_type())
#define FM_NAV_HISTORY(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_NAV_HISTORY_TYPE, FmNavHistory))
#define FM_NAV_HISTORY_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_NAV_HISTORY_TYPE, FmNavHistoryClass))
#define IS_FM_NAV_HISTORY(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_NAV_HISTORY_TYPE))
#define IS_FM_NAV_HISTORY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_NAV_HISTORY_TYPE))

typedef struct _FmNavHistory			FmNavHistory;
typedef struct _FmNavHistoryClass		FmNavHistoryClass;

struct _FmNavHistory
{
	GObject parent;
    FmPathList* paths;
    GList* cur;
    guint n_max;
};

struct _FmNavHistoryClass
{
	GObjectClass parent_class;
};

GType		fm_nav_history_get_type		(void);
FmNavHistory*	fm_nav_history_new			(void);

FmPathList* fm_nav_history_list(FmNavHistory* nh);
FmPath* fm_nav_history_get_cur(FmNavHistory* nh);
GList* fm_nav_history_get_cur_link(FmNavHistory* nh);
gboolean fm_nav_history_get_can_back(FmNavHistory* nh);
void fm_nav_history_back(FmNavHistory* nh);
gboolean fm_nav_history_get_can_forward(FmNavHistory* nh);
void fm_nav_history_forward(FmNavHistory* nh);
void fm_nav_history_chdir(FmNavHistory* nh, FmPath* path);
void fm_nav_history_jump(FmNavHistory* nh, GList* l);
void fm_nav_history_clear(FmNavHistory* nh);
void fm_nav_history_set_max(FmNavHistory* nh, guint num);

G_END_DECLS

#endif /* __FM_NAV_HISTORY_H__ */
