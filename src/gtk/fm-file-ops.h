/*
 *      fm-file-ops.h
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

#ifndef __FM_FILE_OPS_H__
#define __FM_FILE_OPS_H__

#include "fm-path.h"
#include "fm-file-ops-job.h"

void fm_copy_files(FmPathList* files, FmPath* dest_dir);

void fm_move_files(FmPathList* files, FmPath* dest_dir);

void fm_trash_files(FmPathList* files);

void fm_delete_files(FmPathList* files);

#endif

