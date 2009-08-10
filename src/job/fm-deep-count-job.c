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
#include <glib/gstdio.h>

static void fm_deep_count_job_finalize  			(GObject *object);
G_DEFINE_TYPE(FmDeepCountJob, fm_deep_count_job, FM_TYPE_JOB);

static gboolean fm_deep_count_job_run(FmJob* job);

static void deep_count_posix(FmDeepCountJob* job, FmPath* fm_path, dev_t dir_dev);
static void deep_count_gio(FmDeepCountJob* job, FmPath* fm_path);

static void fm_deep_count_job_class_init(FmDeepCountJobClass *klass)
{
	GObjectClass *g_object_class;
	FmJobClass* job_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_deep_count_job_finalize;

	job_class = FM_JOB_CLASS(klass);
	job_class->run = fm_deep_count_job_run;
	job_class->finished = NULL;
}


static void fm_deep_count_job_finalize(GObject *object)
{
	FmDeepCountJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_DEEP_COUNT_JOB(object));

	self = FM_DEEP_COUNT_JOB(object);

	if(self->paths)
		fm_list_unref(self->paths);
	G_OBJECT_CLASS(fm_deep_count_job_parent_class)->finalize(object);
}


static void fm_deep_count_job_init(FmDeepCountJob *self)
{
}


FmJob *fm_deep_count_job_new(FmPathList* paths, FmDeepCountJobFlags flags)
{
	FmDeepCountJob* job = (FmDeepCountJob*)g_object_new(FM_DEEP_COUNT_JOB_TYPE, NULL);
	job->paths = fm_list_ref(paths);
	job->flags = flags;
	return (FmJob*)job;
}

gboolean fm_deep_count_job_run(FmJob* job)
{
	FmDeepCountJob* dc = (FmDeepCountJob*)job;
	GList* l;

	if(!fm_job_init_cancellable(job))
		return FALSE;

	l = fm_list_peek_head_link(dc->paths);
	for(; l; l=l->next)
	{
		FmPath* path = (FmPath*)l->data;
		if(!job->cancel)
		{
			if(fm_path_is_native(path)) /* if it's a native file, use posix APIs */
				deep_count_posix( dc, path, 0 );
			else
				deep_count_gio( dc, path );
		}
	}
	return TRUE;
}

void deep_count_posix(FmDeepCountJob* job, FmPath* fm_path, dev_t dir_dev)
{
	FmJob* fmjob = (FmJob*)job;
	GError* err = NULL;
	char* path = fm_path_to_str(fm_path);
	struct stat st;
	int ret;

	if( G_UNLIKELY(job->flags & FM_DC_JOB_FOLLOW_LINKS) )
		ret = stat(path, &st);
	else
		ret = lstat(path, &st);

	if( ret == 0 )
	{
		if( dir_dev )
		{
			if( job->flags & FM_DC_JOB_SAME_FS )
			{
				if( st.st_dev != dir_dev )
					return;
			}
			else if( job->flags & FM_DC_JOB_DIFF_FS )
			{
				if( st.st_dev == dir_dev )
					return;
			}
		}
		//g_debug("st_dev = %ld", st.st_dev);
		++job->count;
		job->total_size += (goffset)st.st_size;
		job->total_block_size += (st.st_blocks * st.st_blksize);
	}
	else
	{
		/* FIXME: error */
		return;
	}
	if(fmjob->cancel)
		return;

	if( S_ISDIR(st.st_mode) ) /* if it's a dir */
	{
		GDir* dir_ent = g_dir_open(path, 0, NULL);
		if(dir_ent)
		{
			char* basename;
			while( !fmjob->cancel
				&& (basename = g_dir_read_name(dir_ent)) )
			{
				FmPath* sub = fm_path_new_child(fm_path, basename);
				if(!fmjob->cancel)
					deep_count_posix(job, sub, st.st_dev);
				fm_path_unref(sub);
			}
			g_dir_close(dir_ent);
		}
	}
	g_free(path);
}

void deep_count_gio(FmDeepCountJob* job, FmPath* fm_path)
{
	FmJob* fmjob = (FmJob*)job;
	GError* err = NULL;
	const char query_str[] = 
					G_FILE_ATTRIBUTE_STANDARD_TYPE","
					G_FILE_ATTRIBUTE_STANDARD_NAME","
					G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL","
					G_FILE_ATTRIBUTE_STANDARD_SIZE","
					G_FILE_ATTRIBUTE_UNIX_BLOCKS","
					G_FILE_ATTRIBUTE_UNIX_BLOCK_SIZE;
	GFile* gf = fm_path_to_gfile(fm_path);
	GFileInfo* inf = g_file_query_info(gf, query_str, 
							G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
							fmjob->cancellable, &err);
	if( inf )
	{
		GFileType type = g_file_info_get_file_type(inf);
//		if( !g_file_info_get_attribute_boolean(inf, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL) )
		guint64 blk = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_UNIX_BLOCKS);
		guint32 blk_size= g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_BLOCK_SIZE);

		++job->count;
		job->total_size += g_file_info_get_size(inf);
		job->total_block_size += (blk * blk_size);

		g_object_unref(inf);

		if(fmjob->cancel)
			return;

		if( type == G_FILE_TYPE_DIRECTORY )
		{
			GFileEnumerator* enu = g_file_enumerate_children(gf, 
										query_str,
										G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
										fmjob->cancellable, &err);
			while( !fmjob->cancel )
			{
				inf = g_file_enumerator_next_file(enu, fmjob->cancellable, &err);
				if(!fmjob->cancel)
				{
					if(inf)
					{
						// if( !g_file_info_get_attribute_boolean(inf, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL) )
						{
							blk = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_UNIX_BLOCKS);
							blk_size= g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_BLOCK_SIZE);
							++job->count;
							job->total_size += g_file_info_get_size(inf);
							job->total_block_size += (blk * blk_size);
						}
						if( g_file_info_get_file_type(inf)==G_FILE_TYPE_DIRECTORY )
						{
							FmPath* sub = fm_path_new_child(fm_path, g_file_info_get_name(inf));
							deep_count_gio(job, sub);
							fm_path_unref(sub);
						}
					}
					else
					{
						break; /* FIXME: error handling */
					}
				}
				g_object_unref(inf);
			}
			g_file_enumerator_close(enu, NULL, &err);
			g_object_unref(enu);
		}
	}
	else
	{
		/* FIXME: error */
	}
	g_object_unref(gf);
}
