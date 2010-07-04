/*
 *      fm-dummy-monitor.c
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

#include "fm-dummy-monitor.h"

static void fm_dummy_monitor_finalize           (GObject *object);

G_DEFINE_TYPE(FmDummyMonitor, fm_dummy_monitor, G_TYPE_FILE_MONITOR);

static gboolean cancel()
{
    return TRUE;
}

static void fm_dummy_monitor_class_init(FmDummyMonitorClass *klass)
{
    GFileMonitorClass* fm_class = G_FILE_MONITOR_CLASS(klass);
    fm_class->cancel = cancel;
/*
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_dummy_monitor_finalize;
*/
}

/*
static void fm_dummy_monitor_finalize(GObject *object)
{
    FmDummyMonitor *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_FM_DUMMY_MONITOR(object));

    self = FM_DUMMY_MONITOR(object);

    G_OBJECT_CLASS(fm_dummy_monitor_parent_class)->finalize(object);
}
*/

static void fm_dummy_monitor_init(FmDummyMonitor *self)
{

}


GFileMonitor *fm_dummy_monitor_new(void)
{
    return (GFileMonitor*)g_object_new(FM_DUMMY_MONITOR_TYPE, NULL);
}

