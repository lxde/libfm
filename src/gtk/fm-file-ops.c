/*
 *      fm-file-ops.c
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

#include "fm-file-ops.h"
#include "fm-progress-dlg.h"

/* FIXME: only show the progress dialog if the job isn't finished 
 * in 1 sec. */

void fm_copy_files(FmPathList* files, FmPath* dest_dir)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_COPY, files);
	fm_file_ops_job_set_dest(job, dest_dir);
	dlg = fm_progress_dlg_new(job);
	gtk_window_present(dlg);
	fm_job_run_async(job);
}

void fm_move_files(FmPathList* files, FmPath* dest_dir)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_MOVE, files);
	fm_file_ops_job_set_dest(job, dest_dir);
	dlg = fm_progress_dlg_new(job);
	gtk_window_present(dlg);
	fm_job_run_async(job);
}

void fm_trash_files(FmPathList* files)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_TRASH, files);
	dlg = fm_progress_dlg_new(job);
	gtk_window_present(dlg);
	fm_job_run_async(job);
}

void fm_delete_files(FmPathList* files)
{
	GtkWidget* dlg;
	FmJob* job = fm_file_ops_job_new(FM_FILE_OP_DELETE, files);
	dlg = fm_progress_dlg_new(job);
	gtk_window_present(dlg);
	fm_job_run_async(job);
}

