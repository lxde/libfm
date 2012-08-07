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


#ifndef __FOLDER_EXO_VIEW_H__
#define __FOLDER_EXO_VIEW_H__

#include <gtk/gtk.h>
#include "fm-file-info.h"
#include "fm-path.h"
#include "fm-dnd-src.h"
#include "fm-dnd-dest.h"
#include "fm-folder.h"
#include "fm-folder-model.h"
#include "fm-folder-view.h"

G_BEGIN_DECLS

#define FM_FOLDER_EXO_VIEW_TYPE             (fm_folder_exo_view_get_type())
#define FM_FOLDER_EXO_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_FOLDER_EXO_VIEW_TYPE, FmFolderExoView))
#define FM_FOLDER_EXO_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_FOLDER_EXO_VIEW_TYPE, FmFolderExoViewClass))
#define FM_IS_FOLDER_EXO_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_FOLDER_EXO_VIEW_TYPE))
#define FM_IS_FOLDER_EXO_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_FOLDER_EXO_VIEW_TYPE))

/**
 * FmFolderExoViewMode
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
} FmFolderExoViewMode;

#ifndef FM_DISABLE_DEPRECATED
#define FM_FOLDER_VIEW_MODE_IS_VALID(mode) FM_FOLDER_EXO_VIEW_MODE_IS_VALID(mode)
#endif
#define FM_FOLDER_EXO_VIEW_MODE_IS_VALID(mode)  ((guint)mode <= FM_FV_LIST_VIEW)

typedef struct _FmFolderExoView            FmFolderExoView;
typedef struct _FmFolderExoViewClass       FmFolderExoViewClass;

struct _FmFolderExoViewClass
{
    /*< private >*/
    GtkScrolledWindowClass parent_class;
};

GType       fm_folder_exo_view_get_type(void);
FmFolderExoView* fm_folder_exo_view_new(FmFolderExoViewMode mode,
                                        FmFolderViewUpdatePopup update_popup,
                                        FmLaunchFolderFunc open_folders);

void fm_folder_exo_view_set_mode(FmFolderExoView* fv, FmFolderExoViewMode mode);
FmFolderExoViewMode fm_folder_exo_view_get_mode(FmFolderExoView* fv);

G_END_DECLS

#endif /* __FOLDER_EXO_VIEW_H__ */
