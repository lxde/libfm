/*
 * fm-search-job.h
 * 
 * Copyright 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#ifndef __FM_SEARCH_JOB_H__
#define __FM_SEARCH_JOB_H__

#include "fm-dir-list-job.h"

G_BEGIN_DECLS


#define FM_TYPE_SEARCH_JOB				(fm_search_job_get_type())
#define FM_SEARCH_JOB(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_TYPE_SEARCH_JOB, FmSearchJob))
#define FM_SEARCH_JOB_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_TYPE_SEARCH_JOB, FmSearchJobClass))
#define FM_IS_SEARCH_JOB(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_TYPE_SEARCH_JOB))
#define FM_IS_SEARCH_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_TYPE_SEARCH_JOB))
#define FM_SEARCH_JOB_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),\
			FM_TYPE_SEARCH_JOB, FmSearchJobClass))

typedef struct _FmSearchJob			FmSearchJob;
typedef struct _FmSearchJobClass		FmSearchJobClass;
typedef struct _FmSearchJobPrivate		FmSearchJobPrivate;

struct _FmSearchJob
{
	FmDirListJob parent;
	FmSearchJobPrivate *priv;
};

struct _FmSearchJobClass
{
	FmDirListJobClass parent_class;
};


GType fm_search_job_get_type(void);

FmSearchJob* fm_search_job_new(FmPath* search_uri);

G_END_DECLS

#endif /* __FM_SEARCH_JOB_H__ */
