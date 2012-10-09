/*
 *      fm-folder-view.h
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


#ifndef __FOLDER_VIEW_H__
#define __FOLDER_VIEW_H__

#include <gtk/gtk.h>
#include "fm-file-info.h"
#include "fm-folder-model.h"
#include "fm-file-launcher.h"

G_BEGIN_DECLS

#define FM_TYPE_FOLDER_VIEW             (fm_folder_view_get_type())
#define FM_FOLDER_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_TYPE_FOLDER_VIEW, FmFolderView))
#define FM_IS_FOLDER_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_TYPE_FOLDER_VIEW))
#define FM_FOLDER_VIEW_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE((obj),\
            FM_TYPE_FOLDER_VIEW, FmFolderViewInterface))

typedef struct _FmFolderView            FmFolderView; /* Dummy typedef */
typedef struct _FmFolderViewInterface   FmFolderViewInterface;

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

/* callbacks */
/**
 * FmFolderViewUpdatePopup
 * @fv: the folder view widget
 * @window: the window where @fv is present
 * @ui: the object to add interface
 * @act_grp: group of actions to add action
 * @files: list of files for current popup menu
 *
 * The callback to update popup menu. It can disable items of menu, add
 * some new, replace actions, etc. depending of the window and files.
 */
typedef void (*FmFolderViewUpdatePopup)(FmFolderView* fv, GtkWindow* window,
                                        GtkUIManager* ui, GtkActionGroup* act_grp,
                                        FmFileInfoList* files);

/**
 * FmFolderViewInterface:
 * @clicked: the class closure for #FmFolderView::clicked signal
 * @sel_changed: the class closure for #FmFolderView::sel-changed signal
 * @sort_changed: the class closure for #FmFolderView::sort-changed signal
 * @set_sel_mode: VTable func, see fm_folder_view_set_selection_mode()
 * @get_sel_mode: VTable func, see fm_folder_view_get_selection_mode()
 * @set_sort: unused since 1.0.2, will be removed from ABI 5
 * @get_sort: unused since 1.0.2, will be removed from ABI 5
 * @set_show_hidden: function to save show_hidden in the object structure
 * @get_show_hidden: function to retrieve show_hidden from the object structure
 * @get_folder: VTable func, see fm_folder_view_get_folder()
 * @set_model: VTable func, see fm_folder_view_set_model()
 * @get_model: VTable func, see fm_folder_view_get_model()
 * @count_selected_files: VTable func, see fm_folder_view_get_n_selected_files()
 * @dup_selected_files: VTable func, see fm_folder_view_dup_selected_files()
 * @dup_selected_file_paths: VTable func, see fm_folder_view_dup_selected_file_paths()
 * @select_all: VTable func, see fm_folder_view_select_all()
 * @unselect_all: VTable func, see fm_folder_view_unselect_all()
 * @select_invert: VTable func, see fm_folder_view_select_invert()
 * @select_file_path: VTable func, see fm_folder_view_select_file_path()
 * @get_custom_menu_callbacks: function to retrieve callbacks for popup menu setup
 */
/* FIXME: after ABI change:
 * @filter_changed: (since 1.0.2) the class closure for #FmFolderView::filter-changed signal
 */
struct _FmFolderViewInterface
{
    /*< private >*/
    GTypeInterface g_iface;

    /*< public >*/
    /* signals */
    void (*clicked)(FmFolderView* fv, FmFolderViewClickType type, FmFileInfo* file);
    void (*sel_changed)(FmFolderView* fv, FmFileInfoList* sels);
    void (*sort_changed)(FmFolderView* fv);
    /* void (*filter_changed)(FmFolderView* fv); */
    //void (*chdir)(FmFolderView* fv, FmPath* path);

    /* VTable */
    void (*set_sel_mode)(FmFolderView* fv, GtkSelectionMode mode);
    GtkSelectionMode (*get_sel_mode)(FmFolderView* fv);

    void (*set_sort)(FmFolderView* fv, GtkSortType type, FmFolderModelCol by);
    void (*get_sort)(FmFolderView* fv, GtkSortType* type, FmFolderModelCol* by);

    void (*set_show_hidden)(FmFolderView* fv, gboolean show);
    gboolean (*get_show_hidden)(FmFolderView* fv);

    FmFolder* (*get_folder)(FmFolderView* fv);

    void (*set_model)(FmFolderView* fv, FmFolderModel* model);
    FmFolderModel* (*get_model)(FmFolderView* fv);

    gint (*count_selected_files)(FmFolderView* fv);
    FmFileInfoList* (*dup_selected_files)(FmFolderView* fv);
    FmPathList* (*dup_selected_file_paths)(FmFolderView* fv);

    void (*select_all)(FmFolderView* fv);
    void (*unselect_all)(FmFolderView* fv);
    void (*select_invert)(FmFolderView* fv);
    void (*select_file_path)(FmFolderView* fv, FmPath* path);

    /* for implementation internal usage */
    void (*get_custom_menu_callbacks)(FmFolderView* fv, FmFolderViewUpdatePopup*,
                                      FmLaunchFolderFunc*);
    /* FIXME: add few reserved after ABI change */
};

GType           fm_folder_view_get_type(void);

/* VTable calls */
void            fm_folder_view_set_selection_mode(FmFolderView* fv, GtkSelectionMode mode);
GtkSelectionMode fm_folder_view_get_selection_mode(FmFolderView* fv);

void            fm_folder_view_set_show_hidden(FmFolderView* fv, gboolean show);
gboolean        fm_folder_view_get_show_hidden(FmFolderView* fv);

FmFolder*       fm_folder_view_get_folder(FmFolderView* fv);
FmPath*         fm_folder_view_get_cwd(FmFolderView* fv);
FmFileInfo*     fm_folder_view_get_cwd_info(FmFolderView* fv);

FmFolderModel*  fm_folder_view_get_model(FmFolderView* fv);
void            fm_folder_view_set_model(FmFolderView* fv, FmFolderModel* model);

gint            fm_folder_view_get_n_selected_files(FmFolderView* fv);
FmFileInfoList* fm_folder_view_dup_selected_files(FmFolderView* fv);
FmPathList*     fm_folder_view_dup_selected_file_paths(FmFolderView* fv);

void            fm_folder_view_select_all(FmFolderView* fv);
void            fm_folder_view_select_invert(FmFolderView* fv);
void            fm_folder_view_select_file_path(FmFolderView* fv, FmPath* path);
void            fm_folder_view_select_file_paths(FmFolderView* fv, FmPathList* paths);
void            fm_folder_view_unselect_all(FmFolderView* fv);

/* generate a popup menu for the window */
GtkMenu*        fm_folder_view_add_popup(FmFolderView* fv, GtkWindow* parent,
                                         FmFolderViewUpdatePopup update_popup);

void            fm_folder_view_set_active(FmFolderView* fv, gboolean set);

/* bounce action to FmFolderView stock */
void            fm_folder_view_bounce_action(GtkAction* act, FmFolderView* fv);

/* emit signals; for interface implementations only */
void            fm_folder_view_item_clicked(FmFolderView* fv, GtkTreePath* path,
                                            FmFolderViewClickType type);
void            fm_folder_view_sel_changed(GObject* obj, FmFolderView* fv);
//void            fm_folder_view_chdir(FmFolderView* fv, FmPath* path);

#ifndef FM_DISABLE_DEPRECATED
/* deprecated since 1.0.2; left until soname 5 for compatibility */
void            fm_folder_view_sort(FmFolderView* fv, GtkSortType type, FmFolderModelCol by);
GtkSortType     fm_folder_view_get_sort_type(FmFolderView* fv);
FmFolderModelCol fm_folder_view_get_sort_by(FmFolderView* fv);

/* functions defined in fm-standard-view.c
 * are obsoleted since 1.0.1 but left until soname 5 for compatibility */
FmFolderView* fm_folder_view_new(guint mode);

void fm_folder_view_set_mode(FmFolderView* fv, guint mode);
guint fm_folder_view_get_mode(FmFolderView* fv);

void fm_folder_view_select_custom(FmFolderView* fv, GFunc filter, gpointer user_data);
#endif

typedef struct _FmFolderViewColumnInfo FmFolderViewColumnInfo;

/**
 * FmFolderViewColumnInfo:
 * @col_id: column type
 *
 * FmFolderView column setup data.
 */
struct _FmFolderViewColumnInfo
{
    /*< public >*/
    FmFolderModelCol col_id;

    /*< private >*/
    gint reserved1; /* reserved members for usage by interface implementations */
    gint reserved2;
    gint reserved3;
    gint reserved4;
};

gboolean        fm_folder_view_set_columns(FmFolderView* fv, const GSList* cols);
GSList*         fm_folder_view_get_columns(FmFolderView* fv);

G_END_DECLS

#endif /* __FOLDER_VIEW_H__ */
