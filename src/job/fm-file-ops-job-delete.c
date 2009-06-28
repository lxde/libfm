/*
 *      fm-file-ops-job-delete.c
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

#include "fm-file-ops-job-delete.h"

gboolean fm_file_ops_job_delete_file(FmJob* job, GFile* gf, GFileInfo* inf, GError** err)
{
    FmFileOpsJob* fjob = (FmFileOpsJob*)job;
	gboolean is_dir;
    GFileInfo* _inf = NULL;
    const char query[] =  G_FILE_ATTRIBUTE_STANDARD_TYPE","
                           G_FILE_ATTRIBUTE_STANDARD_NAME","
                           G_FILE_ATTRIBUTE_STANDARD_SIZE","
                           G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME;
	if( !inf)
	{
		_inf = inf = g_file_query_info(gf, query,
							G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
							&job->cancellable, &err);
	}
	if(!inf)
    {
        fm_job_emit_error(job, err, FALSE);
        g_error_free(err);
		return FALSE;
    }

    /* currently processed file. */
    fm_file_ops_job_emit_cur_file(fjob, g_file_info_get_display_name(inf));
    fjob->finished += g_file_info_get_size(inf);
    fm_file_ops_job_emit_percent(job);

	is_dir = (g_file_info_get_file_type(inf)==G_FILE_TYPE_DIRECTORY);

    if(_inf)
    	g_object_unref(_inf);

	if( job->cancel )
		return FALSE;

    if( is_dir )
	{
		GFileEnumerator* enu = g_file_enumerate_children(gf, query,
									G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
									job->cancellable, &err);
		while( ! job->cancel )
		{
			inf = g_file_enumerator_next_file(enu, job->cancellable, &err);
			if(inf)
			{
				GFile* sub = g_file_get_child(gf, g_file_info_get_name(inf));
				fm_file_ops_job_delete_file(job, sub, inf, &err); /* FIXME: error handling? */
				g_object_unref(sub);
				g_object_unref(inf);
			}
			else
			{
				break; /* FIXME: error handling */
			}
		}
		g_object_unref(enu);
	}
    return g_file_delete(gf, &job->cancellable, &err);
}

gboolean fm_file_ops_job_trash_file(FmJob* job, GFile* gf, GFileInfo* inf, GError** err)
{
    return TRUE;
}
