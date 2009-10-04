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
#include "fm-file-ops-job-delete.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

const char query[]=
	G_FILE_ATTRIBUTE_STANDARD_TYPE","
	G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
	G_FILE_ATTRIBUTE_STANDARD_NAME","
	G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL","
	G_FILE_ATTRIBUTE_STANDARD_SIZE","
	G_FILE_ATTRIBUTE_UNIX_BLOCKS","
	G_FILE_ATTRIBUTE_UNIX_BLOCK_SIZE","
    G_FILE_ATTRIBUTE_ID_FILESYSTEM;

static void progress_cb(goffset cur, goffset total, FmFileOpsJob* job);

gboolean fm_file_ops_job_copy_file(FmFileOpsJob* job, GFile* src, GFileInfo* inf, GFile* dest)
{
    gboolean ret = FALSE;
	GFileInfo* _inf;
	GError* err = NULL;
	gboolean is_virtual;
    GFileType type;
    goffset size;
    GFile* new_dest;
    GFileCopyFlags flags;
	FmJob* fmjob = FM_JOB(job);

	if( G_LIKELY(inf) )
		_inf = NULL;
	else
	{
		_inf = g_file_query_info(src, query, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, fmjob->cancellable, &err);
		if( !_inf )
		{
			/* FIXME: error handling */
			fm_job_emit_error(fmjob, err, FALSE);
			goto _out;
		}
		inf = _inf;
	}

	/* showing currently processed file. */
	fm_file_ops_job_emit_cur_file(job, g_file_info_get_display_name(inf));

	is_virtual = g_file_info_get_attribute_boolean(inf, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL);
    type = g_file_info_get_file_type(inf);
    size = g_file_info_get_size(inf);

	switch(type)
	{
	case G_FILE_TYPE_DIRECTORY:
		{
			GFileEnumerator* enu;
        _retry_mkdir:
            /* FIXME: handle permissions */
			if( !g_file_make_directory(dest, fmjob->cancellable, &err) )
			{
                FmFileOpOption opt = 0;
                if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
                {
                    g_error_free(err);
                    err = NULL;
                    opt = fm_file_ops_job_ask_rename(job, src, NULL, dest, &new_dest);
                    switch(opt)
                    {
                    case FM_FILE_OP_RENAME:
                        g_object_unref(dest);
                        dest = new_dest;
                        goto _retry_mkdir;
                        break;
                    case FM_FILE_OP_SKIP:
                        goto _out;
                        break;
                    case FM_FILE_OP_OVERWRITE:
                        break;
                    case FM_FILE_OP_CANCEL:
                        fm_job_cancel(FM_JOB(job));
                        ret = FALSE;
                        goto _out;
                        break;
                    }
                }
                if(!opt && opt != FM_FILE_OP_OVERWRITE )
                {
                    fm_job_emit_error(fmjob, err, FALSE);
                    goto _out;
                }
			}
			job->finished += size;

			enu = g_file_enumerate_children(src, query,
								0, fmjob->cancellable, &err);
			while( !fmjob->cancel )
			{
				inf = g_file_enumerator_next_file(enu, fmjob->cancellable, &err);
				if( inf )
				{
					GFile* sub = g_file_get_child(src, g_file_info_get_name(inf));
					GFile* sub_dest = g_file_get_child(dest, g_file_info_get_name(inf));
					gboolean ret = fm_file_ops_job_copy_file(job, sub, inf, sub_dest);
					g_object_unref(sub);
					g_object_unref(sub_dest);
					if( G_UNLIKELY(!ret) )
					{
						/* FIXME: error handling */
						goto _out;
					}
					g_object_unref(inf);
				}
				else
				{
					if(err)
					{
						/* FIXME: error handling */
						fm_job_emit_error(fmjob, err, FALSE);
						goto _out;
					}
					else /* EOF is reached */
                    {
                        ret = TRUE;
						break;
                    }
				}
			}
			g_file_enumerator_close(enu, NULL, &err);
			g_object_unref(enu);
		}
		break;

    case G_FILE_TYPE_SPECIAL:
        /* only handle FIFO for local files */
        if(g_file_is_native(src) && g_file_is_native(dest))
        {
            char* src_path = g_file_get_path(src);
            struct stat src_st;
            int r;
            r = lstat(src_path, &src_st);
            g_free(src_path);
            if(r == 0)
            {
                /* Handle FIFO on native file systems. */
                if(S_ISFIFO(src_st.st_mode))
                {
                    char* dest_path = g_file_get_path(dest);
                    int r = mkfifo(dest_path, src_st.st_mode);
                    g_free(dest_path);
                    if( r == 0)
                        break;
                }
                /* FIXME: how about blcok device, char device, and socket? */
            }
        }

	default:
        flags = G_FILE_COPY_ALL_METADATA|G_FILE_COPY_NOFOLLOW_SYMLINKS;
    _retry_copy:
		if( !g_file_copy(src, dest, flags, FM_JOB(job)->cancellable, 
                progress_cb, fmjob, &err) )
		{
            FmFileOpOption opt = 0;
            flags &= ~G_FILE_COPY_OVERWRITE;

            if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                g_error_free(err);
                err = NULL;

                opt = fm_file_ops_job_ask_rename(job, src, NULL, dest, &new_dest);
                switch(opt)
                {
                case FM_FILE_OP_RENAME:
                    g_object_unref(dest);
                    dest = new_dest;
                    goto _retry_copy;
                    break;
                case FM_FILE_OP_OVERWRITE:
                    flags |= G_FILE_COPY_OVERWRITE;
                    goto _retry_copy;
                    break;
                case FM_FILE_OP_CANCEL:
                    fm_job_cancel(FM_JOB(job));
                    ret = FALSE;
                    break;
                case FM_FILE_OP_SKIP:
                    break;
                }
                goto _out;
            }
            if(!opt)
            {
                fm_job_emit_error(fmjob, err, FALSE);
                g_error_free(err);
            }
			goto _out;
		}
		job->finished += size;
		job->current = 0;

        /* update progress */
        fm_file_ops_job_emit_percent(job);
        ret = TRUE;
		break;
	}
_out:

    if( _inf )
        g_object_unref(_inf);

    return ret;
}

gboolean fm_file_ops_job_move_file(FmFileOpsJob* job, GFile* src, GFileInfo* inf, GFile* dest)
{
	GFileInfo* _inf;
	GError* err = NULL;
	gboolean is_virtual;
    GFileType type;
    goffset size;
	FmJob* fmjob = FM_JOB(job);
    const char* src_fs_id;
    gboolean ret = TRUE;

	if( G_LIKELY(inf) )
		_inf = NULL;
	else
	{
		_inf = g_file_query_info(src, query, 0, fmjob->cancellable, &err);
		if( !_inf )
		{
			/* FIXME: error handling */
			fm_job_emit_error(fmjob, err, FALSE);
			return FALSE;
		}
		inf = _inf;
	}

	/* showing currently processed file. */
	fm_file_ops_job_emit_cur_file(job, g_file_info_get_display_name(inf));

    src_fs_id = g_file_info_get_attribute_string(inf, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
    /* Check if source and destination are on the same device */
    if( strcmp(src_fs_id, job->dest_fs_id) == 0 ) /* same device */
    {
        FmFileOpOption opt = 0;
        GFileCopyFlags flags = G_FILE_COPY_ALL_METADATA|G_FILE_COPY_NOFOLLOW_SYMLINKS;
        _retry_move:
        if( !g_file_move(src, dest, flags, fmjob->cancellable, progress_cb, job, &err))
        {
            flags &= ~G_FILE_COPY_OVERWRITE;
            if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                GFile* new_dest = NULL;
                opt = fm_file_ops_job_ask_rename(job, src, NULL, dest, &new_dest);
                g_error_free(err);
                err = NULL;

                switch(opt)
                {
                case FM_FILE_OP_RENAME:
                    g_object_unref(dest);
                    dest = new_dest;
                    goto _retry_move;
                    break;
                case FM_FILE_OP_OVERWRITE:
                    flags |= G_FILE_COPY_OVERWRITE;
                    goto _retry_move;
                    break;
                case FM_FILE_OP_CANCEL:
                    fm_job_cancel(FM_JOB(job));
                    ret = FALSE;
                    break;
                case FM_FILE_OP_SKIP:
                    break;
                }
                goto _out;
            }
            if(!opt)
            {
                fm_job_emit_error(fmjob, err, FALSE);
                ret = FALSE;
            }
            goto _out;
        }
    }
    else /* use copy if they are on different devices */
    {
        /* use copy & delete */
        ret = fm_file_ops_job_copy_file(job, src, inf, dest);
        if(ret)
            ret = fm_file_ops_job_delete_file(job, src, inf); /* delete the source file. */
    }

_out:
    /* FIXME: error handling */
    if( _inf )
        g_object_unref(_inf);
	return ret;
}

void progress_cb(goffset cur, goffset total, FmFileOpsJob* job)
{
	job->current = cur;
    /* update progress */
    fm_file_ops_job_emit_percent(job);
}

gboolean fm_file_ops_job_copy_run(FmFileOpsJob* job)
{
	GList* l;
    FmJob* fmjob = FM_JOB(job);
	/* prepare the job, count total work needed with FmDeepCountJob */
	FmDeepCountJob* dc = fm_deep_count_job_new(job->srcs, FM_DC_JOB_DEFAULT);
    /* let the deep count job share the same cancellable object. */
    fm_job_set_cancellable(FM_JOB(dc), fmjob->cancellable);
    /* FIXME: there is no way to cancel the deep count job here. */
	fm_job_run_sync(dc);
	job->total = dc->total_size;
    if(fmjob->cancel)
    {
        g_object_unref(dc);
        return FALSE;
    }
	g_object_unref(dc);
	g_debug("total size to copy: %llu", job->total);

    /* FIXME: cancellation? */
	for(l = fm_list_peek_head_link(job->srcs); !fmjob->cancel && l; l=l->next)
	{
		FmPath* path = (FmPath*)l->data;
		FmPath* _dest = fm_path_new_child(job->dest, path->name);
		GFile* src = fm_path_to_gfile(path);
		GFile* dest = fm_path_to_gfile(_dest);
		fm_path_unref(_dest);
		if(!fm_file_ops_job_copy_file(job, src, NULL, dest))
        {
            fm_job_cancel(job);
			return FALSE;
        }
	}
	return TRUE;
}

gboolean fm_file_ops_job_move_run(FmFileOpsJob* job)
{
	GFile *dest;
	GFileInfo* inf;
	GList* l;
	GError* err = NULL;
	FmJob* fmjob = FM_JOB(job);
    dev_t dest_dev = 0;

    /* get information of destination folder */
	g_return_val_if_fail(job->dest, FALSE);
	dest = fm_path_to_gfile(job->dest);
    inf = g_file_query_info(dest, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL","
                                  G_FILE_ATTRIBUTE_UNIX_DEVICE","
                                  G_FILE_ATTRIBUTE_ID_FILESYSTEM","
                                  G_FILE_ATTRIBUTE_UNIX_DEVICE, 0, 
                                  fmjob->cancellable, &err);
    if(inf)
    {
        job->dest_fs_id = g_intern_string(g_file_info_get_attribute_string(inf, G_FILE_ATTRIBUTE_ID_FILESYSTEM));
        dest_dev = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_DEVICE); /* needed by deep count */
        g_object_unref(inf);
    }
    else
    {
        /* FIXME: error handling */
        g_object_unref(dest);
        return FALSE;
    }

	/* prepare the job, count total work needed with FmDeepCountJob */
	FmDeepCountJob* dc = fm_deep_count_job_new(job->srcs, FM_DC_JOB_DIFF_FS);
    fm_deep_count_job_set_dest(dc, dest_dev, job->dest_fs_id);
	fm_job_run_sync(dc);
	job->total = dc->total_size;

	if( FM_JOB(dc)->cancel )
		return FALSE;
	g_object_unref(dc);
	g_debug("total size to move: %llu, dest_fs: %s", job->total, job->dest_fs_id);

	for(l = fm_list_peek_head_link(job->srcs); !fmjob->cancel && l; l=l->next)
	{
		FmPath* path = (FmPath*)l->data;
		FmPath* _dest = fm_path_new_child(job->dest, path->name);
		GFile* src = fm_path_to_gfile(path);
		GFile* dest = fm_path_to_gfile(_dest);
		fm_path_unref(_dest);
		if(!fm_file_ops_job_move_file(job, src, NULL, dest))
        {
            fm_job_cancel(job);
			return FALSE;
        }
	}
    return TRUE;
}
