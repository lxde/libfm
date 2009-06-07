/*
 *      fm-deep-count-job.c
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

#include "fm-deep-count-job.h"

static void fm_deep_countjob_finalize  			(GObject *object);
G_DEFINE_TYPE(FmDeepCountjob, fm_deep_countjob, FM_JOB_TYPE);


static void fm_deep_countjob_class_init(FmDeepCountjobClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = fm_deep_countjob_finalize;
}


static void fm_deep_countjob_finalize(GObject *object)
{
	FmDeepCountjob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_DEEP_COUNTJOB(object));

	self = FM_DEEP_COUNTJOB(object);

	G_OBJECT_CLASS(fm_deep_countjob_parent_class)->finalize(object);
}


static void fm_deep_countjob_init(FmDeepCountjob *self)
{
	
}


FmJob *fm_deep_countjob_new(void)
{
	return g_object_new(FM_DEEP_COUNTJOB_TYPE, NULL);
}

