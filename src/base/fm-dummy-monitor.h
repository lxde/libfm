/*
 *      fm-dummy-monitor.h
 *      
 *      Copyright 2009 PCMan <pcman@debian>
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


#ifndef __FM_DUMMY_MONITOR_H__
#define __FM_DUMMY_MONITOR_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define FM_DUMMY_MONITOR_TYPE				(fm_dummy_monitor_get_type())
#define FM_DUMMY_MONITOR(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_DUMMY_MONITOR_TYPE, FmDummyMonitor))
#define FM_DUMMY_MONITOR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_DUMMY_MONITOR_TYPE, FmDummyMonitorClass))
#define IS_FM_DUMMY_MONITOR(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_DUMMY_MONITOR_TYPE))
#define IS_FM_DUMMY_MONITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_DUMMY_MONITOR_TYPE))

typedef struct _FmDummyMonitor			FmDummyMonitor;
typedef struct _FmDummyMonitorClass		FmDummyMonitorClass;

struct _FmDummyMonitor
{
	GFileMonitor parent;
};

struct _FmDummyMonitorClass
{
	GFileMonitorClass parent_class;
};

GType fm_dummy_monitor_get_type(void);
GFileMonitor* fm_dummy_monitor_new(void);

G_END_DECLS

#endif /* __FM_DUMMY_MONITOR_H__ */
