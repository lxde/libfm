/*
 *      fm-simple-job.h
 *
 *      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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


#ifndef __FM_SIMPLE_JOB_H__
#define __FM_SIMPLE_JOB_H__

#include <fm-job.h>

G_BEGIN_DECLS

#define FM_TYPE_SIMPLE_JOB              (fm_simple_job_get_type())
#define FM_SIMPLE_JOB(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_TYPE_SIMPLE_JOB, FmSimpleJob))
#define FM_SIMPLE_JOB_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_TYPE_SIMPLE_JOB, FmSimpleJobClass))
#define FM_IS_SIMPLE_JOB(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_TYPE_SIMPLE_JOB))
#define FM_IS_SIMPLE_JOB_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_TYPE_SIMPLE_JOB))

typedef struct _FmSimpleJob         FmSimpleJob;
typedef struct _FmSimpleJobClass        FmSimpleJobClass;

typedef gboolean (*FmSimpleJobFunc)(FmSimpleJob*, gpointer);

struct _FmSimpleJob
{
    FmJob parent;
    FmSimpleJobFunc func;
    gpointer user_data;
    GDestroyNotify destroy_data;
};

struct _FmSimpleJobClass
{
    FmJobClass parent_class;
};

GType fm_simple_job_get_type(void);
FmJob*  fm_simple_job_new(FmSimpleJobFunc func, gpointer user_data, GDestroyNotify destroy_data);

G_END_DECLS

#endif /* __FM_SIMPLE_JOB_H__ */
