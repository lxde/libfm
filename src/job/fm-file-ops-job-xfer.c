/*
 *      fm-file-ops-xfer.c
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


#include "fm-file-ops-job-xfer.h"

const char query[]=
	G_FILE_ATTRIBUTE_STANDARD_TYPE","
	G_FILE_ATTRIBUTE_STANDARD_NAME","
	G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL","
	G_FILE_ATTRIBUTE_STANDARD_SIZE","
	G_FILE_ATTRIBUTE_UNIX_BLOCKS","
	G_FILE_ATTRIBUTE_UNIX_BLOCK_SIZE;

static void progress_cb(goffset cur, goffset total, FmFileOpsJob* job);

/* FIXME: handle duplicated filenames and overwrite */
gboolean fm_file_ops_job_copy_file(FmFileOpsJob* job, GFile* src, GFileInfo* inf, GFile* dest)
{
	GFileInfo* _inf;
	GError* err = NULL;
	gboolean is_virtual;
	FmJob* fmjob = FM_JOB(job);

	if( G_LIKELY(inf) )
		_inf = NULL;
	else
	{
		_inf = g_file_query_info(src, query, 0, &fmjob->cancellable, &err);
		if( !_inf )
		{
			/* FIXME: error handling */
			return FALSE;
		}
		inf = _inf;
	}

	is_virtual = g_file_info_get_attribute_boolean(inf, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL);

	switch(g_file_info_get_file_type(inf))
	{
	case G_FILE_TYPE_DIRECTORY:
		{
			GFileEnumerator* enu;

			if( !g_file_make_directory(dest, &fmjob->cancellable, &err) )
			{
				/* FIXME: error handling */
				return FALSE;
			}
			job->finished += g_file_info_get_size(inf);

			enu = g_file_enumerate_children(src, query,
								0, fmjob->cancellable, &err);
			while( !fmjob->cancel )
			{
				inf = g_file_enumerator_next_file(enu, fmjob->cancellable, &err);
				if( inf )
				{
					GFile* sub = g_file_get_child(src, g_file_info_get_name(inf));
					GFile* sub_dest = g_file_get_child(dest, g_file_info_get_name(inf));
					g_debug("%s", g_file_info_get_name(inf));
					gboolean ret = fm_file_ops_job_copy_file(job, sub, inf, sub_dest);
					g_object_unref(sub);
					g_object_unref(sub_dest);
					if( G_UNLIKELY(!ret) )
					{
						/* FIXME: error handling */
						return FALSE;
					}
					g_object_unref(inf);
				}
				else
				{
					if(err)
					{
						/* FIXME: error handling */
						return FALSE;
					}
					else /* EOF is reached */
						break;
				}
			}
			g_file_enumerator_close(enu, NULL, &err);
			g_object_unref(enu);
		}
		break;

	default:
		if( !g_file_copy(src, dest, 
					G_FILE_COPY_ALL_METADATA,
					&FM_JOB(job)->cancellable, 
					progress_cb, fmjob, &err) )
			return FALSE;
		job->finished += job->current;
		job->current = 0;
		break;
	}
	if(_inf)
		g_object_unref(_inf);
	return TRUE;
}

gboolean fm_file_ops_job_move_file(FmFileOpsJob* job, GFile* src, GFileInfo* inf, GFile* dest)
{
	return FALSE;
}

void progress_cb(goffset cur, goffset total, FmFileOpsJob* job)
{
	job->current = cur;
}
