/*
 *      fm-sortable.h
 *
 *      Copyright 2012 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

#ifndef _FM_SORTABLE_H_
#define _FM_SORTABLE_H_ 1
G_BEGIN_DECLS

/**
 * FmFolderModelSortMode:
 * @FM_FOLDER_MODEL_SORT_ASCENDING: sort ascending, mutually exclusive with FM_FOLDER_MODEL_SORT_DESCENDING
 * @FM_FOLDER_MODEL_SORT_DESCENDING: sort descending, mutually exclusive with OLDER_MODEL_SORT_ASCENDING
 * @FM_FOLDER_MODEL_SORT_CASE_SENSITIVE: case sensitive file names sort
 * @FM_FOLDER_MODEL_SORT_ORDER_MASK: (FM_FOLDER_MODEL_SORT_ASCENDING|FM_FOLDER_MODEL_SORT_DESCENDING)
 *
 * Sort mode flags supported by FmFolderModel
 */
/* FIXME:
 * @FM_FOLDER_MODEL_SORT_FOLDER_FIRST: sort folder before files
*/
typedef enum{
    FM_FOLDER_MODEL_SORT_ASCENDING = 0,
    FM_FOLDER_MODEL_SORT_DESCENDING = 1 << 0,
    FM_FOLDER_MODEL_SORT_CASE_SENSITIVE = 1 << 1,
//    FM_FOLDER_MODEL_SORT_FOLDER_FIRST = 1 << 2,
    FM_FOLDER_MODEL_SORT_ORDER_MASK = (FM_FOLDER_MODEL_SORT_ASCENDING|FM_FOLDER_MODEL_SORT_DESCENDING),
} FmFolderModelSortMode;

/**
 * FM_FOLDER_MODEL_SORT_DEFAULT:
 *
 * value which means do not change sorting mode flags.
 */
#define FM_FOLDER_MODEL_SORT_DEFAULT ((FmFolderModelSortMode)-1)

G_END_DECLS
#endif /* _FM_SORTABLE_H_ */
