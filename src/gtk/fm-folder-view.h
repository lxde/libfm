/*
 *      folder-view.h
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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


#ifndef __FOLDER_VIEW_H__
#define __FOLDER_VIEW_H__

#include <gtk/gtk.h>
#include "fm-file-info.h"
#include "fm-path.h"
#include "fm-dnd-src.h"
#include "fm-dnd-dest.h"
#include "fm-folder.h"
#include "fm-folder-model.h"

G_BEGIN_DECLS

#define FM_FOLDER_VIEW_TYPE             (fm_folder_view_get_type())
#define FM_FOLDER_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_FOLDER_VIEW_TYPE, FmFolderView))
#define FM_FOLDER_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_FOLDER_VIEW_TYPE, FmFolderViewClass))
#define FM_IS_FOLDER_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_FOLDER_VIEW_TYPE))
#define FM_IS_FOLDER_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_FOLDER_VIEW_TYPE))

/**
 * FmFolderViewMode
 * @FM_FV_ICON_VIEW: standard icon view
 * @FM_FV_COMPACT_VIEW: view with small icons and text on right of them
 * @FM_FV_THUMBNAIL_VIEW: view with big icons/thumbnails
 * @FM_FV_LIST_VIEW: table-form view
 */
typedef enum
{
    FM_FV_ICON_VIEW,
    FM_FV_COMPACT_VIEW,
    FM_FV_THUMBNAIL_VIEW,
    FM_FV_LIST_VIEW
} FmFolderViewMode;

#define FM_FOLDER_VIEW_MODE_IS_VALID(mode)  ((guint)mode <= FM_FV_LIST_VIEW)

/**
 * FmFolderViewClickType
 * @FM_FV_CLICK_NONE: no click
 * @FM_FV_ACTIVATED: this can be triggered by both
                        left single or double click depending on
                        whether single-click activation is used or not.
 * @FM_FV_MIDDLE_CLICK: middle mouse button pressed
 * @FM_FV_CONTEXT_MENU: right mouse button pressed
 *
 * Click type for #FmFolderView::clicked signal handlers.
 */
typedef enum
{
    FM_FV_CLICK_NONE,
    FM_FV_ACTIVATED,
    FM_FV_MIDDLE_CLICK,
    FM_FV_CONTEXT_MENU
} FmFolderViewClickType;

#define FM_FOLDER_VIEW_CLICK_TYPE_IS_VALID(type)    (type > FM_FV_CLICK_NONE && type <= FM_FV_CONTEXT_MENU)

typedef struct _FmFolderView            FmFolderView;
typedef struct _FmFolderViewClass       FmFolderViewClass;

/**
 * FmFolderViewClass:
 * @parent_class: the parent class
 * @clicked: the class closure for the #FmFolderView::clicked signal
 * @sel_changed: the class closure for the #FmFolderView::sel-changed signal
 * @sort_changed: the class closure for the #FmFolderView::sort-changed signal
 */
struct _FmFolderViewClass
{
    GtkScrolledWindowClass parent_class;
    void (*clicked)(FmFolderView* fv, FmFolderViewClickType type, FmFileInfo* file);
    void (*sel_changed)(FmFolderView* fv, gint n_sel);
    void (*sort_changed)(FmFolderView* fv);
};

GType       fm_folder_view_get_type(void);
FmFolderView* fm_folder_view_new(FmFolderViewMode mode);

void fm_folder_view_set_mode(FmFolderView* fv, FmFolderViewMode mode);
FmFolderViewMode fm_folder_view_get_mode(FmFolderView* fv);

void fm_folder_view_set_selection_mode(FmFolderView* fv, GtkSelectionMode mode);
GtkSelectionMode fm_folder_view_get_selection_mode(FmFolderView* fv);

void fm_folder_view_sort(FmFolderView* fv, GtkSortType type, FmFolderModelViewCol by);
GtkSortType fm_folder_view_get_sort_type(FmFolderView* fv);
int fm_folder_view_get_sort_by(FmFolderView* fv);

void fm_folder_view_set_show_hidden(FmFolderView* fv, gboolean show);
gboolean fm_folder_view_get_show_hidden(FmFolderView* fv);

FmFolder* fm_folder_view_get_folder(FmFolderView* fv);
FmPath* fm_folder_view_get_cwd(FmFolderView* fv);
FmFileInfo* fm_folder_view_get_cwd_info(FmFolderView* fv);

FmFolderModel* fm_folder_view_get_model(FmFolderView* fv);
void fm_folder_view_set_model(FmFolderView* fv, FmFolderModel* model);

FmFileInfoList* fm_folder_view_dup_selected_files(FmFolderView* fv);
FmPathList* fm_folder_view_dup_selected_file_paths(FmFolderView* fv);

void fm_folder_view_select_all(FmFolderView* fv);
void fm_folder_view_select_invert(FmFolderView* fv);
void fm_folder_view_select_file_path(FmFolderView* fv, FmPath* path);
void fm_folder_view_select_file_paths(FmFolderView* fv, FmPathList* paths);

/* TODO: select files by custom func, not yet implemented */
void fm_folder_view_select_custom(FmFolderView* fv, GFunc filter, gpointer user_data);

G_END_DECLS

#endif /* __FOLDER_VIEW_H__ */
