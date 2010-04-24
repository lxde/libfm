/*
 *      fm-file-ops-job-change-attr.c
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

#include "fm-file-ops-job-change-attr.h"
#include "fm-monitor.h"

static const char query[] =  G_FILE_ATTRIBUTE_STANDARD_TYPE","
                               G_FILE_ATTRIBUTE_STANDARD_NAME","
                               G_FILE_ATTRIBUTE_UNIX_GID","
                               G_FILE_ATTRIBUTE_UNIX_UID","
                               G_FILE_ATTRIBUTE_UNIX_MODE","
                               G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME;

gboolean _fm_file_ops_job_change_attr_file(FmFileOpsJob* job, GFile* gf, GFileInfo* inf)
{
    GError* err = NULL;
    GCancellable* cancellable = fm_job_get_cancellable(FM_JOB(job));
    GFileType type;
    gboolean ret = TRUE;
    gboolean changed = FALSE;

	if( !inf)
	{
_retry_query_info:
		inf = g_file_query_info(gf, query,
							G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
							cancellable, &err);
        if(!inf)
        {
            FmJobErrorAction act = fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_MILD);
            g_error_free(err);
            err = NULL;
            if(act == FM_JOB_RETRY)
                goto _retry_query_info;
        }
	}
    else
        g_object_ref(inf);

    type = g_file_info_get_file_type(inf);

    /* change owner */
    if( !fm_job_is_cancelled(FM_JOB(job)) && job->uid != -1 )
    {
_retry_change_owner:
        if(!g_file_set_attribute_uint32(gf, G_FILE_ATTRIBUTE_UNIX_UID,
                                                  job->uid, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                  cancellable, &err) )
        {
            FmJobErrorAction act = fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_MILD);
            g_error_free(err);
            err = NULL;
            if(act == FM_JOB_RETRY)
                goto _retry_change_owner;
        }
        changed = TRUE;
    }

    /* change group */
    if( !fm_job_is_cancelled(FM_JOB(job)) && job->gid != -1 )
    {
_retry_change_group:
        if(!g_file_set_attribute_uint32(gf, G_FILE_ATTRIBUTE_UNIX_GID,
                                                  job->gid, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                  cancellable, &err) )
        {
            FmJobErrorAction act = fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_MILD);
            g_error_free(err);
            err = NULL;
            if(act == FM_JOB_RETRY)
                goto _retry_change_group;
        }
        changed = TRUE;
    }

    /* change mode */
    if( !fm_job_is_cancelled(FM_JOB(job)) && job->new_mode_mask )
    {
        guint32 mode = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_MODE);
        mode &= ~job->new_mode_mask;
        mode |= (job->new_mode & job->new_mode_mask);

        /* FIXME: this behavior should be optional. */
        /* treat dirs with 'r' as 'rx' */
        if(type == G_FILE_TYPE_DIRECTORY)
        {
            if((job->new_mode_mask & S_IRUSR) && (mode & S_IRUSR))
                mode |= S_IXUSR;
            if((job->new_mode_mask & S_IRGRP) && (mode & S_IRGRP))
                mode |= S_IXGRP;
            if((job->new_mode_mask & S_IROTH) && (mode & S_IROTH))
                mode |= S_IXOTH;
        }

        /* new mode */
_retry_chmod:
        if( !g_file_set_attribute_uint32(gf, G_FILE_ATTRIBUTE_UNIX_MODE,
                                         mode, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                         cancellable, &err) )
        {
            FmJobErrorAction act = fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_MILD);
            g_error_free(err);
            err = NULL;
            if(act == FM_JOB_RETRY)
                goto _retry_chmod;
        }
        changed = TRUE;
    }

    /* currently processed file. */
    if(inf)
        fm_file_ops_job_emit_cur_file(job, g_file_info_get_display_name(inf));
    else
    {
        char* basename = g_file_get_basename(gf);
        char* disp = g_filename_display_name(basename);
        fm_file_ops_job_emit_cur_file(job, disp);
        g_free(disp);
        g_free(basename);
    }

    ++job->finished;
    fm_file_ops_job_emit_percent(job);

    if(changed && job->src_folder_mon)
        g_file_monitor_emit_event(job->src_folder_mon, gf, NULL, G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED);

    if( !fm_job_is_cancelled(FM_JOB(job)) && job->recursive && type == G_FILE_TYPE_DIRECTORY)
    {
        GFileMonitor* old_mon = job->src_folder_mon;
		GFileEnumerator* enu;
_retry_enum_children:
        enu = g_file_enumerate_children(gf, query,
									G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
									cancellable, &err);
        if(!enu)
        {
            FmJobErrorAction act = fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_MILD);
            g_error_free(err);
            err = NULL;
            if(act == FM_JOB_RETRY)
                goto _retry_enum_children;
		    return FALSE;
        }

        if(! g_file_is_native(gf))
            job->src_folder_mon = fm_monitor_lookup_dummy_monitor(gf);

		while( ! fm_job_is_cancelled(FM_JOB(job)) )
		{
			inf = g_file_enumerator_next_file(enu, cancellable, &err);
			if(inf)
			{
				GFile* sub = g_file_get_child(gf, g_file_info_get_name(inf));
				ret = _fm_file_ops_job_change_attr_file(job, sub, inf); /* FIXME: error handling? */
				g_object_unref(sub);
				g_object_unref(inf);
                if(!ret)
                    break;
			}
			else
			{
                if(err)
                {
                    FmJobErrorAction act = fm_job_emit_error(FM_JOB(job), err, FM_JOB_ERROR_MILD);
                    g_error_free(err);
                    err = NULL;
                    /* FM_JOB_RETRY is not supported here */
                }
                else /* EOF */
                    break;
			}
		}
		g_object_unref(enu);

        if(job->src_folder_mon)
        {
            /* FIXME: we also need to fire a changed event on the monitor of the dir itself. */
            g_file_monitor_emit_event(job->src_folder_mon, gf, NULL, G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED);
            g_object_unref(job->src_folder_mon);
        }
        job->src_folder_mon = old_mon;
    }
    if(inf)
        g_object_unref(inf);
    return ret;
}

gboolean _fm_file_ops_job_change_attr_run(FmFileOpsJob* job)
{
	GList* l;
	/* prepare the job, count total work needed with FmDeepCountJob */
    if(job->recursive)
    {
        FmDeepCountJob* dc = fm_deep_count_job_new(job->srcs, FM_DC_JOB_DEFAULT);
        fm_job_run_sync(FM_JOB(dc));
        job->total = dc->count;
        g_object_unref(dc);
    }
    else
        job->total = fm_list_get_length(job->srcs);

	g_debug("total number of files to change attribute: %llu", job->total);

    fm_file_ops_job_emit_prepared(job);

	l = fm_list_peek_head_link(job->srcs);
	for(; ! fm_job_is_cancelled(FM_JOB(job)) && l;l=l->next)
	{
        gboolean ret;
        GFileMonitor* mon;
		GFile* src = fm_path_to_gfile((FmPath*)l->data);
        if(g_file_is_native(src))
            mon = NULL;
        else
        {
            GFile* src_dir = g_file_get_parent(src);
            if(src_dir)
            {
                mon = fm_monitor_lookup_dummy_monitor(src_dir);
                job->src_folder_mon = mon;
        		g_object_unref(src_dir);
            }
            else
                job->src_folder_mon = mon = NULL;
        }

		ret = _fm_file_ops_job_change_attr_file(job, src, NULL);
		g_object_unref(src);

        if(mon)
        {
            g_object_unref(mon);
            job->src_folder_mon = NULL;
        }

		if(!ret) /* error! */
            return FALSE;
	}
	return TRUE;
}
