/*
 *      fm-file-ops-job-delete.c
 *
 *      Copyright 2009 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
#include "fm-monitor.h"

static const char query[] =  G_FILE_ATTRIBUTE_STANDARD_TYPE","
                               G_FILE_ATTRIBUTE_STANDARD_NAME","
                               G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME;


/* FIXME: cancel the job on errors */
gboolean fm_file_ops_job_delete_file(FmJob* job, GFile* gf, GFileInfo* inf)
{
    GError* err = NULL;
    FmFileOpsJob* fjob = (FmFileOpsJob*)job;
	gboolean is_dir, descend;
    GFileInfo* _inf = NULL;
    gboolean ret = FALSE;

	if( !inf)
	{
_retry_query_info:
		_inf = inf = g_file_query_info(gf, query,
							G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
							fm_job_get_cancellable(job), &err);
        if(!_inf)
        {
            FmJobErrorAction act = fm_job_emit_error(job, err, FM_JOB_ERROR_MODERATE);
            g_error_free(err);
            if(act == FM_JOB_RETRY)
                goto _retry_query_info;
            else if(act == FM_JOB_ABORT)
                return FALSE;
        }
	}
	if(!inf)
    {
        /* use basename of GFile as display name. */
        char* basename = g_file_get_basename(gf);
        char* disp = g_filename_display_name(basename);
        g_free(basename);
        fm_file_ops_job_emit_cur_file(fjob, disp);
        g_free(disp);
        ++fjob->finished;
		return FALSE;
    }

    /* currently processed file. */
    fm_file_ops_job_emit_cur_file(fjob, g_file_info_get_display_name(inf));

    /* show progress */
    ++fjob->finished;
    fm_file_ops_job_emit_percent(FM_FILE_OPS_JOB(job));

	is_dir = (g_file_info_get_file_type(inf)==G_FILE_TYPE_DIRECTORY);

    if(_inf)
    	g_object_unref(_inf);

	if( fm_job_is_cancelled(job) )
		return FALSE;

    if(is_dir)
    {
        descend = TRUE;
        /* special handling for trash:/// */
        if(!g_file_is_native(gf))
        {
            char* scheme = g_file_get_uri_scheme(gf);
            if(g_strcmp0(scheme, "trash") == 0)
            {
                /* little trick: basename of trash root is /. */
                char* basename = g_file_get_basename(gf);
                if(basename[0] != G_DIR_SEPARATOR)
                    descend = FALSE;
                g_free(basename);
            }
            g_free(scheme);
        }
    }
    else
        descend = FALSE;

    if( descend )
	{
        GFileMonitor* old_mon = fjob->src_folder_mon;
		GFileEnumerator* enu = g_file_enumerate_children(gf, query,
									G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
									fm_job_get_cancellable(job), &err);
        if(!enu)
        {
            FmJobErrorAction act = fm_job_emit_error(job, err, FM_JOB_ERROR_MODERATE);
            g_error_free(err);
            return FALSE;
        }

        if(! g_file_is_native(gf))
            fjob->src_folder_mon = fm_monitor_lookup_dummy_monitor(gf);

		while( ! fm_job_is_cancelled(job) )
		{
			inf = g_file_enumerator_next_file(enu, fm_job_get_cancellable(job), &err);
			if(inf)
			{
				GFile* sub = g_file_get_child(gf, g_file_info_get_name(inf));
				fm_file_ops_job_delete_file(job, sub, inf); /* FIXME: error handling? */
				g_object_unref(sub);
				g_object_unref(inf);
			}
			else
			{
                if(err)
                {
                    FmJobErrorAction act = fm_job_emit_error(job, err, FM_JOB_ERROR_MODERATE);
                    /* FM_JOB_RETRY is not supported here */
                    g_error_free(err);
                    g_object_unref(enu);
                    if(fjob->src_folder_mon)
                        g_object_unref(fjob->src_folder_mon);
                    fjob->src_folder_mon = old_mon;
                    return FALSE;
                }
                else /* EOF */
                    break;
			}
		}
		g_object_unref(enu);

        if(fjob->src_folder_mon)
        {
            /* FIXME: this is a little bit incorrect since we emit deleted signal before the
             * dir is really deleted. */
            g_file_monitor_emit_event(fjob->src_folder_mon, gf, NULL, G_FILE_MONITOR_EVENT_DELETED);
            g_object_unref(fjob->src_folder_mon);
        }
        fjob->src_folder_mon = old_mon;
	}
    if(!fm_job_is_cancelled(job))
    {
_retry_delete:
        ret = g_file_delete(gf, fm_job_get_cancellable(job), &err);
        if(ret)
        {
            if(fjob->src_folder_mon)
                g_file_monitor_emit_event(fjob->src_folder_mon, gf, NULL, G_FILE_MONITOR_EVENT_DELETED);
        }
        else
        {
            if(err)
            {
                FmJobErrorAction act;
                if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_PERMISSION_DENIED)
                {
                    /* special case for trash:/// */
                    /* FIXME: is there any better way to handle this? */
                    char* scheme = g_file_get_uri_scheme(gf);
                    if(g_strcmp0(scheme, "trash") == 0)
                    {
                        g_free(scheme);
                        g_error_free(err);
                        return TRUE;
                    }
                    g_free(scheme);
                }
                act = fm_job_emit_error(job, err, FM_JOB_ERROR_MODERATE);
                g_error_free(err);
                if(act == FM_JOB_RETRY)
                    goto _retry_delete;
                return FALSE;
            }
        }
    }
    else
        ret = FALSE;

    return ret;
}

gboolean fm_file_ops_job_trash_file(FmJob* job, GFile* gf, GFileInfo* inf)
{
    return TRUE;
}

gboolean fm_file_ops_job_delete_run(FmFileOpsJob* job)
{
	GList* l;
    gboolean ret = TRUE;
	/* prepare the job, count total work needed with FmDeepCountJob */
	FmDeepCountJob* dc = fm_deep_count_job_new(job->srcs, FM_DC_JOB_PREPARE_DELETE);
    /* let the deep count job share the same cancellable */
    fm_job_set_cancellable(dc, fm_job_get_cancellable(FM_JOB(job)));
	fm_job_run_sync(FM_JOB(dc));
	job->total = dc->count;
	g_object_unref(dc);

    if(fm_job_is_cancelled(FM_JOB(job)))
    {
        g_debug("delete job is cancelled");
        return FALSE;
    }

	g_debug("total number of files to delete: %llu", job->total);

	l = fm_list_peek_head_link(job->srcs);
	for(; ! fm_job_is_cancelled(FM_JOB(job)) && l;l=l->next)
	{
        GFileMonitor* mon;
		GFile* src = fm_path_to_gfile((FmPath*)l->data);
        if( g_file_is_native(src) )
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

        ret = fm_file_ops_job_delete_file(FM_JOB(job), src, NULL);
		g_object_unref(src);

        if(mon)
        {
            g_object_unref(mon);
            job->src_folder_mon = NULL;
        }
	}
	return ret;
}

gboolean fm_file_ops_job_trash_run(FmFileOpsJob* job)
{
    gboolean ret = TRUE;
	GList* l;
    GList* failed = NULL;
    GError* err = NULL;
    FmJob* fmjob = FM_JOB(job);
	g_debug("total number of files to delete: %u", fm_list_get_length(job->srcs));
    job->total = fm_list_get_length(job->srcs);

    /* FIXME: we shouldn't trash a file already in trash:/// */

	l = fm_list_peek_head_link(job->srcs);
	for(; !fm_job_is_cancelled(fmjob) && l;l=l->next)
	{
		GFile* gf = fm_path_to_gfile((FmPath*)l->data);
        ret = g_file_trash(gf, fm_job_get_cancellable(fmjob), &err);
		g_object_unref(gf);
        if(!ret)
        {
            if( err->domain == G_IO_ERROR && err->code == G_IO_ERROR_NOT_SUPPORTED)
            {
                /* if trashing is not supported by the file system */
                failed = g_list_prepend(failed, (FmPath*)l->data);
                /* will fallback to delete later. */
                continue;
            }
            /* otherwise, it's caused by another reason */
            /* FIXME: ask the user first before we returned? */
        }
        else
            ++job->finished;
        fm_file_ops_job_emit_percent(job);
	}

    /* these files cannot be trashed due to lack of support from
     * underlying file systems. */
    if(failed)
    {
/*
        char* msg = g_strdup_printf(
                        _("These files cannot be moved to trash bin because"
                        "the underlying file systems don't support this operation\n"
                        "%s"
                        "Are you want to delete them instead?"), files);
        fm_job_ask(job, msg, );
*/
        /* fallback to delete! */
        job->total = g_list_length(failed);
        job->finished = 0;
        fm_file_ops_job_emit_percent(job);
        g_list_foreach(failed, (GFunc)fm_path_unref, NULL);
        g_list_free(failed);
        /* replace srcs with failed files and run delete job instead */
        // fm_file_ops_job_delete_run(job);
    }
    return TRUE;
}
