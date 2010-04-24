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

#ifndef __FM_FILE_OPS_JOB_XFER_H__
#define __FM_FILE_OPS_JOB_XFER_H__

#include <glib.h>
#include <gio/gio.h>
#include "fm-file-ops-job.h"

G_BEGIN_DECLS

gboolean _fm_file_ops_job_copy_file(FmFileOpsJob* job, GFile* src, GFileInfo* inf, GFile* dest);
gboolean _fm_file_ops_job_copy_run(FmFileOpsJob* job);

gboolean _fm_file_ops_job_move_file(FmFileOpsJob* job, GFile* src, GFileInfo* inf, GFile* dest);
gboolean _fm_file_ops_job_move_run(FmFileOpsJob* job);

G_END_DECLS

#endif
