/*
 *      fm-file-info-job.c
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

/**
 * SECTION:fm-file-info-job
 * @short_description: Job to gather information about files.
 * @title: FmFileInfoJob
 *
 * @include: libfm/fm-file-info-job.h
 *
 * The #FmFileInfoJob can be used to get filled #FmFileInfo for some files.
 */

#include "fm-file-info-job.h"

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "fm-file-info.h"

static void fm_file_info_job_dispose              (GObject *object);
static gboolean fm_file_info_job_run(FmJob* fmjob);

const char gfile_info_query_attribs[]="standard::*,unix::*,time::*,access::*,id::filesystem";

G_DEFINE_TYPE(FmFileInfoJob, fm_file_info_job, FM_TYPE_JOB);

static void fm_file_info_job_class_init(FmFileInfoJobClass *klass)
{
    GObjectClass *g_object_class;
    FmJobClass* job_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_file_info_job_dispose;
    /* use finalize from parent class */

    job_class = FM_JOB_CLASS(klass);
    job_class->run = fm_file_info_job_run;
}


static void fm_file_info_job_dispose(GObject *object)
{
    FmFileInfoJob *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_FILE_INFO_JOB(object));

    self = (FmFileInfoJob*)object;
    if(self->file_infos)
    {
        fm_file_info_list_unref(self->file_infos);
        self->file_infos = NULL;
    }
    if(self->current)
    {
        fm_path_unref(self->current);
        self->current = NULL;
    }

    G_OBJECT_CLASS(fm_file_info_job_parent_class)->dispose(object);
}


static void fm_file_info_job_init(FmFileInfoJob *self)
{
    self->file_infos = fm_file_info_list_new();
    fm_job_init_cancellable(FM_JOB(self));
}

/**
 * fm_file_info_job_new
 * @files_to_query: (allow-none): list of paths to query informatiom
 * @flags: modificators of query mode
 *
 * Creates a new #FmFileInfoJob which can be used by #FmJob API.
 *
 * Returns: (transfer full): a new #FmFileInfoJob object.
 *
 * Since: 0.1.0
 */
FmFileInfoJob* fm_file_info_job_new(FmPathList* files_to_query, FmFileInfoJobFlags flags)
{
    GList* l;
    FmFileInfoJob* job = (FmFileInfoJob*)g_object_new(FM_TYPE_FILE_INFO_JOB, NULL);
    FmFileInfoList* file_infos;

    job->flags = flags;
    if(files_to_query)
    {
        file_infos = job->file_infos;
        for(l = fm_path_list_peek_head_link(files_to_query);l;l=l->next)
        {
            FmPath* path = FM_PATH(l->data);
            FmFileInfo* fi = fm_file_info_new();
            fm_file_info_set_path(fi, path);
            fm_file_info_list_push_tail_noref(file_infos, fi);
        }
    }
    return job;
}

static gboolean fm_file_info_job_run(FmJob* fmjob)
{
    GList* l;
    FmFileInfoJob* job = (FmFileInfoJob*)fmjob;
    GError* err = NULL;

    if(job->file_infos == NULL)
        return FALSE;

    for(l = fm_file_info_list_peek_head_link(job->file_infos); !fm_job_is_cancelled(fmjob) && l;)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        GList* next = l->next;
        FmPath* path = fm_file_info_get_path(fi);

        if(job->current)
            fm_path_unref(job->current);
        job->current = fm_path_ref(path);

        if(fm_path_is_native(path))
        {
            char* path_str = fm_path_to_str(path);
            if(!_fm_file_info_job_get_info_for_native_file(fmjob, fi, path_str, &err))
            {
                FmJobErrorAction act = fm_job_emit_error(fmjob, err, FM_JOB_ERROR_MILD);
                g_error_free(err);
                err = NULL;
                if(act == FM_JOB_RETRY)
                {
                    g_free(path_str);
                    continue; /* retry */
                }

                fm_file_info_list_delete_link(job->file_infos, l); /* also calls unref */
            }
            g_free(path_str);
        }
        else
        {
            GFile* gf;

            gf = fm_path_to_gfile(path);
            if(!_fm_file_info_job_get_info_for_gfile(fmjob, fi, gf, &err))
            {
              if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_NOT_MOUNTED)
              {
                GFileInfo *inf;
                /* location by link isn't mounted; unfortunately we cannot
                   launch a target if we don't know what kind of target we
                   have; lets make a simplest directory-kind GFIleInfo */
                /* FIXME: this may be dirty a bit */
                g_error_free(err);
                err = NULL;
                inf = g_file_info_new();
                g_file_info_set_file_type(inf, G_FILE_TYPE_DIRECTORY);
                g_file_info_set_name(inf, fm_path_get_basename(path));
                g_file_info_set_display_name(inf, fm_path_get_basename(path));
                fm_file_info_set_from_gfileinfo(fi, inf);
                g_object_unref(inf);
              }
              else
              {
                FmJobErrorAction act = fm_job_emit_error(fmjob, err, FM_JOB_ERROR_MILD);
                g_error_free(err);
                err = NULL;
                if(act == FM_JOB_RETRY)
                {
                    g_object_unref(gf);
                    continue; /* retry */
                }

                fm_file_info_list_delete_link(job->file_infos, l); /* also calls unref */
              }
            }
            g_object_unref(gf);
        }
        l = next;
    }
    return TRUE;
}

/**
 * fm_file_info_job_add
 * @job: a job to add file
 * @path: a path to add to query list
 *
 * Adds a @path to query list for the @job.
 *
 * This API may only be called before starting the @job.
 *
 * Since: 0.1.0
 */
void fm_file_info_job_add(FmFileInfoJob* job, FmPath* path)
{
    FmFileInfo* fi = fm_file_info_new();
    fm_file_info_set_path(fi, path);
    fm_file_info_list_push_tail_noref(job->file_infos, fi);
}

/**
 * fm_file_info_job_add_gfile
 * @job: a job to add file
 * @gf: a file descriptor to add to query list
 *
 * Adds a path @gf to query list for the @job.
 *
 * This API may only be called before starting the @job.
 *
 * Since: 0.1.0
 */
void fm_file_info_job_add_gfile(FmFileInfoJob* job, GFile* gf)
{
    FmPath* path = fm_path_new_for_gfile(gf);
    FmFileInfo* fi = fm_file_info_new();
    fm_file_info_set_path(fi, path);
    fm_path_unref(path);
    fm_file_info_list_push_tail_noref(job->file_infos, fi);
}

/**
 * fm_file_info_job_get_current
 * @job: the job to inspect
 *
 * Retrieves current the #FmPath which caused the error.
 * Returned data are owned by @job and shouldn't be freed by caller.
 *
 * This API may only be called in error handler.
 *
 * Returns: (transfer none): the current processing file path.
 *
 * Since: 0.1.10
 */
FmPath* fm_file_info_job_get_current(FmFileInfoJob* job)
{
    return job->current;
}
