/*
 *      fm-folder-view.c
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

/**
 * SECTION:fm-folder-view
 * @short_description: A folder view generic interface.
 * @title: FmFolderView
 *
 * @include: libfm/fm-folder-view.h
 *
 * The #FmFolderView generic interface is used to implement folder views
 * common code including handling sorting change, and keyboard and mouse
 * buttons events.
 *
 * The #FmFolderView interface methods can attach context menu to widget
 * which does folder operations and consists of items:
 * |[
 * CreateNew -> NewFolder
 *              NewBlank
 *              NewShortcut
 *              &lt;placeholder name='ph1'/&gt;
 * ------------------------
 * Paste
 * Cut
 * Copy
 * Del
 * Remove
 * FileProp
 * ------------------------
 * SelAll
 * InvSel
 * ------------------------
 * Sort -> Asc
 *         Desc
 *         ----------------
 *         ByName
 *         ByMTime
 *         BySize
 *         ByType
 *         ----------------
 *         &lt;placeholder name='CustomSortOps'/&gt;
 * ShowHidden
 * Rename
 * &lt;placeholder name='CustomFolderOps'/&gt;
 * ------------------------
 * &lt;placeholder name='CustomCommonOps'/&gt;
 * ------------------------
 * Prop
 * ]|
 * In created menu items 'Cut', 'Copy', 'Del', 'Remove', and 'FileProp'
 * are hidden.
 *
 * Widget can modity the menu replacing placeholders and hiding or
 * enabling existing items in it. Widget can do that in callback which
 * is supplied for call fm_folder_view_add_popup().
 *
 * If click was not on widget but on some item in it then not this
 * context menu but one with #FmFileMenu object will be opened instead.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "gtk-compat.h"

#include <glib/gi18n-lib.h>
#include <stdlib.h>

#include "fm-folder-view.h"
#include "fm-file-properties.h"
#include "fm-clipboard.h"
#include "fm-gtk-file-launcher.h"
#include "fm-file-menu.h"
#include "fm-gtk-utils.h"

static const char folder_popup_xml[] =
"<popup>"
  "<menu action='CreateNew'>"
    "<menuitem action='NewFolder'/>"
    "<menuitem action='NewBlank'/>"
    "<menuitem action='NewShortcut'/>"
    /* placeholder for ~/Templates support */
    "<placeholder name='ph1'/>"
  "</menu>"
  "<separator/>"
  "<menuitem action='Paste'/>"
  "<menuitem action='Cut'/>"
  "<menuitem action='Copy'/>"
  "<menuitem action='Del'/>"
  "<menuitem action='Remove'/>"
  "<menuitem action='FileProp'/>"
  "<separator/>"
  "<menuitem action='SelAll'/>"
  "<menuitem action='InvSel'/>"
  "<separator/>"
  "<menu action='Sort'>"
    "<menuitem action='Asc'/>"
    "<menuitem action='Desc'/>"
    "<separator/>"
    "<menuitem action='ByName'/>"
    "<menuitem action='ByMTime'/>"
    "<menuitem action='BySize'/>"
    "<menuitem action='ByType'/>"
    "<separator/>"
    /* "<menuitem action='MingleDirs'/>"
    "<menuitem action='SortIgnoreCase'/>" */
    /* placeholder for custom sort options */
    "<placeholder name='CustomSortOps'/>"
    /* "<separator/>"
    "<menuitem action='SortPerFolder'/>" */
  "</menu>"
  "<menuitem action='ShowHidden'/>"
  "<menuitem action='Rename'/>"
  /* placeholder for custom folder operations */
  "<placeholder name='CustomFolderOps'/>"
  "<separator/>"
  /* placeholder for custom application operations such as view mode changing */
  "<placeholder name='CustomCommonOps'/>"
  "<separator/>"
  "<menuitem action='Prop'/>"
"</popup>"
"<accelerator action='NewFolder2'/>"
"<accelerator action='NewFolder3'/>"
"<accelerator action='Copy2'/>"
"<accelerator action='Paste2'/>"
"<accelerator action='Del2'/>"
"<accelerator action='Remove2'/>"
"<accelerator action='FileProp2'/>"
"<accelerator action='FileProp3'/>"
"<accelerator action='Menu'/>"
"<accelerator action='Menu2'/>";

static void on_create_new(GtkAction* act, FmFolderView* fv);
static void on_cut(GtkAction* act, FmFolderView* fv);
static void on_copy(GtkAction* act, FmFolderView* fv);
static void on_paste(GtkAction* act, FmFolderView* fv);
static void on_trash(GtkAction* act, FmFolderView* fv);
static void on_rm(GtkAction* act, FmFolderView* fv);
static void on_select_all(GtkAction* act, FmFolderView* fv);
static void on_invert_select(GtkAction* act, FmFolderView* fv);
static void on_rename(GtkAction* act, FmFolderView* fv);
static void on_prop(GtkAction* act, FmFolderView* fv);
static void on_file_prop(GtkAction* act, FmFolderView* fv);
static void on_menu(GtkAction* act, FmFolderView* fv);
static void on_show_hidden(GtkToggleAction* act, FmFolderView* fv);

static const GtkActionEntry folder_popup_actions[]=
{
    {"CreateNew", NULL, N_("Create _New..."), NULL, NULL, NULL},
    {"NewFolder", "folder", N_("Folder"), "<Ctrl><Shift>N", NULL, G_CALLBACK(on_create_new)},
    {"NewFolder2", NULL, NULL, "Insert", NULL, G_CALLBACK(on_create_new)},
    {"NewFolder3", NULL, NULL, "KP_Insert", NULL, G_CALLBACK(on_create_new)},
    {"NewBlank", "text-x-generic", N_("Blank File"), NULL, NULL, G_CALLBACK(on_create_new)},
    {"NewShortcut", "system-run", N_("Shortcut"), NULL, NULL, G_CALLBACK(on_create_new)},
    {"Cut", GTK_STOCK_CUT, NULL, "<Ctrl>X", NULL, G_CALLBACK(on_cut)},
    {"Copy", GTK_STOCK_COPY, NULL, "<Ctrl>C", NULL, G_CALLBACK(on_copy)},
    {"Copy2", NULL, NULL, "<Ctrl>Insert", NULL, G_CALLBACK(on_copy)},
    {"Paste", GTK_STOCK_PASTE, NULL, "<Ctrl>V", NULL, G_CALLBACK(on_paste)},
    {"Paste2", NULL, NULL, "<Shift>Insert", NULL, G_CALLBACK(on_paste)},
    {"Del", GTK_STOCK_DELETE, NULL, "Delete", NULL, G_CALLBACK(on_trash)},
    {"Del2", NULL, NULL, "KP_Delete", NULL, G_CALLBACK(on_trash)},
    {"Remove", GTK_STOCK_REMOVE, NULL, "<Shift>Delete", NULL, G_CALLBACK(on_rm)},
    {"Remove2", NULL, NULL, "<Shift>KP_Delete", NULL, G_CALLBACK(on_rm)},
    {"SelAll", GTK_STOCK_SELECT_ALL, NULL, "<Ctrl>A", NULL, G_CALLBACK(on_select_all)},
    {"InvSel", NULL, N_("_Invert Selection"), "<Ctrl>I", NULL, G_CALLBACK(on_invert_select)},
    {"Sort", NULL, N_("_Sort Files"), NULL, NULL, NULL},
    {"Rename", NULL, N_("_Rename"), NULL, NULL, G_CALLBACK(on_rename)},
    {"Prop", GTK_STOCK_PROPERTIES, N_("Prop_erties"), "", NULL, G_CALLBACK(on_prop)},
    {"FileProp", GTK_STOCK_PROPERTIES, N_("Prop_erties"), "<Alt>Return", NULL, G_CALLBACK(on_file_prop)},
    {"FileProp2", NULL, NULL, "<Alt>KP_Enter", NULL, G_CALLBACK(on_file_prop)},
    {"FileProp3", NULL, NULL, "<Alt>ISO_Enter", NULL, G_CALLBACK(on_file_prop)},
    {"Menu", NULL, NULL, "Menu", NULL, G_CALLBACK(on_menu)},
    {"Menu2", NULL, NULL, "<Shift>F10", NULL, G_CALLBACK(on_menu)}
};

static GtkToggleActionEntry folder_toggle_actions[]=
{
    {"ShowHidden", NULL, N_("Show _Hidden"), "<Ctrl>H", NULL, G_CALLBACK(on_show_hidden), FALSE},
    {"SortPerFolder", NULL, N_("_Only for this folder"), NULL,
                N_("Check to remember sort as folder setting rather than global one"), NULL, FALSE},
    {"MingleDirs", NULL, N_("Mingle _files and folders"), NULL, NULL, NULL, FALSE},
    {"SortIgnoreCase", NULL, N_("_Ignore name case"), NULL, NULL, NULL, TRUE}
};

static const GtkRadioActionEntry folder_sort_type_actions[]=
{
    {"Asc", GTK_STOCK_SORT_ASCENDING, NULL, NULL, NULL, GTK_SORT_ASCENDING},
    {"Desc", GTK_STOCK_SORT_DESCENDING, NULL, NULL, NULL, GTK_SORT_DESCENDING},
};

static const GtkRadioActionEntry folder_sort_by_actions[]=
{
    {"ByName", NULL, N_("By _Name"), NULL, NULL, FM_FOLDER_MODEL_COL_NAME},
    {"ByMTime", NULL, N_("By _Modification Time"), NULL, NULL, FM_FOLDER_MODEL_COL_MTIME},
    {"BySize", NULL, N_("By _Size"), NULL, NULL, FM_FOLDER_MODEL_COL_SIZE},
    {"ByType", NULL, N_("By File _Type"), NULL, NULL, FM_FOLDER_MODEL_COL_DESC}
};

G_DEFINE_INTERFACE(FmFolderView, fm_folder_view, GTK_TYPE_WIDGET);

enum
{
    CLICKED,
    SEL_CHANGED,
    SORT_CHANGED,
    FILTER_CHANGED,
    //CHDIR,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static GQuark ui_quark;
static GQuark popup_quark;

static void fm_folder_view_default_init(FmFolderViewInterface *iface)
{
    ui_quark = g_quark_from_static_string("popup-ui");
    popup_quark = g_quark_from_static_string("popup-menu");

    /* properties and signals */
    /**
     * FmFolderView::clicked:
     * @view: the widget that emitted the signal
     * @type: (#FmFolderViewClickType) type of click
     * @file: (#FmFileInfo *) file on which cursor is
     *
     * The #FmFolderView::clicked signal is emitted when user clicked
     * somewhere in the folder area. If click was on free folder area
     * then @file is %NULL.
     *
     * Since: 0.1.0
     */
    signals[CLICKED]=
        g_signal_new("clicked",
                     FM_TYPE_FOLDER_VIEW,
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewInterface, clicked),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__UINT_POINTER,
                     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);

    /**
     * FmFolderView::sel-changed:
     * @view: the widget that emitted the signal
     * @n_sel: number of files currently selected in the folder
     *
     * The #FmFolderView::sel-changed signal is emitted when
     * selection of the view got changed.
     *
     * Before 1.0.0 parameter was list of currently selected files.
     *
     * Since: 0.1.0
     */
    signals[SEL_CHANGED]=
        g_signal_new("sel-changed",
                     FM_TYPE_FOLDER_VIEW,
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewInterface, sel_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * FmFolderView::sort-changed:
     * @view: the widget that emitted the signal
     *
     * The #FmFolderView::sort-changed signal is emitted when sorting
     * of the view got changed.
     *
     * Since: 0.1.10
     */
    signals[SORT_CHANGED]=
        g_signal_new("sort-changed",
                     FM_TYPE_FOLDER_VIEW,
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmFolderViewInterface, sort_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * FmFolderView::filter-changed:
     * @view: the widget that emitted the signal
     *
     * The #FmFolderView::filter-changed signal is emitted when filter
     * of the view model got changed. It's just bouncer for the same
     * signal of #FmFolderModel.
     *
     * Since: 1.0.2
     */
    signals[FILTER_CHANGED]=
        g_signal_new("filter-changed",
                     FM_TYPE_FOLDER_VIEW,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     /* FIXME: replace 0 after ABI change
                     G_STRUCT_OFFSET(FmFolderViewInterface, filter_changed), */
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /* FIXME: add "chdir" so main-win can connect to it and sync side-pane */
}

static void on_sort_col_changed(GtkTreeSortable* sortable, FmFolderView* fv)
{
    if(fm_folder_model_get_sort(FM_FOLDER_MODEL(sortable), NULL, NULL))
    {
        g_signal_emit(fv, signals[SORT_CHANGED], 0);
    }
}

static void on_filter_changed(FmFolderModel* model, FmFolderView* fv)
{
    g_signal_emit(fv, signals[FILTER_CHANGED], 0);
}

/**
 * fm_folder_view_set_selection_mode
 * @fv: a widget to apply
 * @mode: new mode of selection in @fv.
 *
 * Changes selection mode in @fv.
 *
 * Since: 0.1.0
 */
void fm_folder_view_set_selection_mode(FmFolderView* fv, GtkSelectionMode mode)
{
    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    FM_FOLDER_VIEW_GET_IFACE(fv)->set_sel_mode(fv, mode);
}

/**
 * fm_folder_view_get_selection_mode
 * @fv: a widget to inspect
 *
 * Retrieves current selection mode in @fv.
 *
 * Returns: current selection mode.
 *
 * Since: 0.1.0
 */
GtkSelectionMode fm_folder_view_get_selection_mode(FmFolderView* fv)
{
    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), GTK_SELECTION_NONE);

    return (*FM_FOLDER_VIEW_GET_IFACE(fv)->get_sel_mode)(fv);
}

/**
 * fm_folder_view_sort
 * @fv: a widget to apply
 * @type: new mode of sorting (ascending or descending)
 * @by: criteria of sorting
 *
 * Changes sorting in the view. Invalid values for @type or @by are
 * ignored (will not change sorting).
 *
 * Since 1.0.2 values passed to this API aren't remembered in the @fv
 * object. If @fv has no model then this API has no effect.
 * After the model is removed from @fv (calling fm_folder_view_set_model()
 * with NULL) there is no possibility to recover last settings and any
 * model added to @fv later will get defaults: FM_FOLDER_MODEL_COL_DEFAULT
 * and FM_SORT_DEFAULT.
 *
 * Since: 0.1.0
 *
 * Deprecated: 1.0.2: Use fm_folder_model_set_sort() instead.
 */
void fm_folder_view_sort(FmFolderView* fv, GtkSortType type, FmFolderModelCol by)
{
    FmFolderViewInterface* iface;
    FmFolderModel* model;
    FmSortMode mode;

    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    iface = FM_FOLDER_VIEW_GET_IFACE(fv);
    model = iface->get_model(fv);
    if(model)
    {
        if(type == GTK_SORT_ASCENDING || type == GTK_SORT_DESCENDING)
        {
            fm_folder_model_get_sort(model, NULL, &mode);
            mode &= ~FM_SORT_ORDER_MASK;
            mode |= (type == GTK_SORT_ASCENDING) ? FM_SORT_ASCENDING : FM_SORT_DESCENDING;
        }
        else
            mode = FM_SORT_DEFAULT;
        fm_folder_model_set_sort(model, by, mode);
    }
    /* model will generate signal to update config if changed */
}

/**
 * fm_folder_view_get_sort_type
 * @fv: a widget to inspect
 *
 * Retrieves current sorting type in @fv.
 *
 * Returns: mode of sorting (ascending or descending)
 *
 * Since: 0.1.0
 *
 * Deprecated: 1.0.2: use fm_folder_model_get_sort() instead.
 */
GtkSortType fm_folder_view_get_sort_type(FmFolderView* fv)
{
    FmFolderViewInterface* iface;
    FmFolderModel* model;
    FmSortMode mode;

    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), GTK_SORT_ASCENDING);

    iface = FM_FOLDER_VIEW_GET_IFACE(fv);
    model = iface->get_model(fv);
    if(model == NULL || !fm_folder_model_get_sort(model, NULL, &mode))
        return GTK_SORT_ASCENDING;
    return FM_SORT_IS_ASCENDING(mode) ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING;
}

/**
 * fm_folder_view_get_sort_by
 * @fv: a widget to inspect
 *
 * Retrieves current criteria of sorting in @fv (e.g. by name).
 *
 * Returns: criteria of sorting.
 *
 * Since: 0.1.0
 *
 * Deprecated: 1.0.2: use fm_folder_model_get_sort() instead.
 */
FmFolderModelCol fm_folder_view_get_sort_by(FmFolderView* fv)
{
    FmFolderViewInterface* iface;
    FmFolderModel* model;
    FmFolderModelCol by;

    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), FM_FOLDER_MODEL_COL_DEFAULT);

    iface = FM_FOLDER_VIEW_GET_IFACE(fv);
    model = iface->get_model(fv);
    if(model == NULL || !fm_folder_model_get_sort(model, &by, NULL))
        return FM_FOLDER_MODEL_COL_DEFAULT;
    return by;
}

/**
 * fm_folder_view_set_show_hidden
 * @fv: a widget to apply
 * @show: new setting
 *
 * Changes whether hidden files in folder shown in @fv should be visible
 * or not.
 *
 * See also: fm_folder_view_get_show_hidden().
 *
 * Since: 0.1.0
 */
void fm_folder_view_set_show_hidden(FmFolderView* fv, gboolean show)
{
    FmFolderViewInterface* iface;

    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    iface = FM_FOLDER_VIEW_GET_IFACE(fv);
    if(iface->get_show_hidden(fv) != show)
    {
        FmFolderModel* model;
        iface->set_show_hidden(fv, show);
        model = iface->get_model(fv);
        if(G_LIKELY(model))
            fm_folder_model_set_show_hidden(model, show);
        /* FIXME: signal to update config */
    }
}

/**
 * fm_folder_view_get_show_hidden
 * @fv: a widget to inspect
 *
 * Retrieves setting whether hidden files in folder shown in @fv should
 * be visible or not.
 *
 * See also: fm_folder_view_set_show_hidden().
 *
 * Returns: %TRUE if hidden files are visible.
 *
 * Since: 0.1.0
 */
gboolean fm_folder_view_get_show_hidden(FmFolderView* fv)
{
    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), FALSE);

    return (*FM_FOLDER_VIEW_GET_IFACE(fv)->get_show_hidden)(fv);
}

/**
 * fm_folder_view_get_folder
 * @fv: a widget to inspect
 *
 * Retrieves the folder shown by @fv. Returned data are owned by @fv and
 * should not be freed by caller.
 *
 * Returns: (transfer none): the folder of view.
 *
 * Since: 1.0.0
 */
FmFolder* fm_folder_view_get_folder(FmFolderView* fv)
{
    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), NULL);

    return (*FM_FOLDER_VIEW_GET_IFACE(fv)->get_folder)(fv);
}

/**
 * fm_folder_view_get_cwd
 * @fv: a widget to inspect
 *
 * Retrieves file path of the folder shown by @fv. Returned data are
 * owned by @fv and should not be freed by caller.
 *
 * Returns: (transfer none): file path of the folder.
 *
 * Since: 0.1.0
 */
FmPath* fm_folder_view_get_cwd(FmFolderView* fv)
{
    FmFolder* folder;

    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), NULL);

    folder = FM_FOLDER_VIEW_GET_IFACE(fv)->get_folder(fv);
    return folder ? fm_folder_get_path(folder) : NULL;
}

/**
 * fm_folder_view_get_cwd_info
 * @fv: a widget to inspect
 *
 * Retrieves file info of the folder shown by @fv. Returned data are
 * owned by @fv and should not be freed by caller.
 *
 * Returns: (transfer none): file info descriptor of the folder.
 *
 * Since: 0.1.0
 */
FmFileInfo* fm_folder_view_get_cwd_info(FmFolderView* fv)
{
    FmFolder* folder;

    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), NULL);

    folder = FM_FOLDER_VIEW_GET_IFACE(fv)->get_folder(fv);
    return folder ? fm_folder_get_info(folder) : NULL;
}

/**
 * fm_folder_view_get_model
 * @fv: a widget to inspect
 *
 * Retrieves the model used by @fv. Returned data are owned by @fv and
 * should not be freed by caller.
 *
 * Returns: (transfer none): the model of view.
 *
 * Since: 0.1.16
 */
FmFolderModel* fm_folder_view_get_model(FmFolderView* fv)
{
    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), NULL);

    return (*FM_FOLDER_VIEW_GET_IFACE(fv)->get_model)(fv);
}

static void unset_model(FmFolderView* fv, FmFolderModel* model)
{
    g_signal_handlers_disconnect_by_func(model, on_sort_col_changed, fv);
    g_signal_handlers_disconnect_by_func(model, on_filter_changed, fv);
}

/**
 * fm_folder_view_set_model
 * @fv: a widget to apply
 * @model: (allow-none): new view model
 *
 * Changes model for the @fv.
 *
 * Since: 1.0.0
 */
void fm_folder_view_set_model(FmFolderView* fv, FmFolderModel* model)
{
    FmFolderViewInterface* iface;
    FmFolderModel* old_model;
    FmFolderModelCol by = FM_FOLDER_MODEL_COL_DEFAULT;
    FmSortMode mode = FM_SORT_ASCENDING;

    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    iface = FM_FOLDER_VIEW_GET_IFACE(fv);
    old_model = iface->get_model(fv);
    if(old_model)
    {
        fm_folder_model_get_sort(old_model, &by, &mode);
        unset_model(fv, old_model);
        /* https://bugs.launchpad.net/ubuntu/+source/pcmanfm/+bug/1071231:
           after changing the folder selection isn't reset */
        iface->unselect_all(fv);
    }
    /* FIXME: which setting to apply if this is first model? */
    iface->set_model(fv, model);
    if(model)
    {
        fm_folder_model_set_sort(model, by, mode);
        g_signal_connect(model, "sort-column-changed", G_CALLBACK(on_sort_col_changed), fv);
        g_signal_connect(model, "filter-changed", G_CALLBACK(on_filter_changed), fv);
    }
}

/**
 * fm_folder_view_get_n_selected_files
 * @fv: a widget to inspect
 *
 * Retrieves number of the currently selected files.
 *
 * Returns: number of files selected.
 *
 * Since: 1.0.1
 */
gint fm_folder_view_get_n_selected_files(FmFolderView* fv)
{
    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), 0);

    return (*FM_FOLDER_VIEW_GET_IFACE(fv)->count_selected_files)(fv);
}

/**
 * fm_folder_view_dup_selected_files
 * @fv: a FmFolderView object
 *
 * Retrieves a list of
 * the currently selected files. The list should be freed after usage
 * with fm_file_info_list_unref&lpar;). If there are no files selected then
 * returns %NULL.
 *
 * Before 1.0.0 this API had name fm_folder_view_get_selected_files.
 *
 * Returns: (transfer full) (element-type FmFileInfo): list of selected file infos.
 *
 * Since: 0.1.0
 */
FmFileInfoList* fm_folder_view_dup_selected_files(FmFolderView* fv)
{
    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), NULL);

    return (*FM_FOLDER_VIEW_GET_IFACE(fv)->dup_selected_files)(fv);
}

/**
 * fm_folder_view_dup_selected_file_paths
 * @fv: a FmFolderView object
 *
 * Retrieves a list of
 * the currently selected files. The list should be freed after usage
 * with fm_path_list_unref&lpar;). If there are no files selected then returns
 * %NULL.
 *
 * Before 1.0.0 this API had name fm_folder_view_get_selected_file_paths.
 *
 * Returns: (transfer full) (element-type FmPath): list of selected file paths.
 *
 * Since: 0.1.0
 */
FmPathList* fm_folder_view_dup_selected_file_paths(FmFolderView* fv)
{
    g_return_val_if_fail(FM_IS_FOLDER_VIEW(fv), NULL);

    return (*FM_FOLDER_VIEW_GET_IFACE(fv)->dup_selected_file_paths)(fv);
}

/**
 * fm_folder_view_select_all
 * @fv: a widget to apply
 *
 * Selects all files in folder.
 *
 * Since: 0.1.0
 */
void fm_folder_view_select_all(FmFolderView* fv)
{
    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    FM_FOLDER_VIEW_GET_IFACE(fv)->select_all(fv);
}

/**
 * fm_folder_view_unselect_all
 * @fv: a widget to apply
 *
 * Unselects all files in folder.
 *
 * Since: 1.0.1
 */
void fm_folder_view_unselect_all(FmFolderView* fv)
{
    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    FM_FOLDER_VIEW_GET_IFACE(fv)->unselect_all(fv);
}

/**
 * fm_folder_view_select_invert
 * @fv: a widget to apply
 *
 * Selects all unselected files in @fv but unselects all selected.
 *
 * Since: 0.1.0
 */
void fm_folder_view_select_invert(FmFolderView* fv)
{
    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    FM_FOLDER_VIEW_GET_IFACE(fv)->select_invert(fv);
}

/**
 * fm_folder_view_select_file_path
 * @fv: a widget to apply
 * @path: a file path to select
 *
 * Selects a file in the folder.
 *
 * Since: 0.1.0
 */
void fm_folder_view_select_file_path(FmFolderView* fv, FmPath* path)
{
    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    FM_FOLDER_VIEW_GET_IFACE(fv)->select_file_path(fv, path);
}

/**
 * fm_folder_view_select_file_paths
 * @fv: a widget to apply
 * @paths: list of files to select
 *
 * Selects few files in the folder.
 *
 * Since: 0.1.0
 */
void fm_folder_view_select_file_paths(FmFolderView* fv, FmPathList* paths)
{
    GList* l;
    FmFolderViewInterface* iface;

    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    iface = FM_FOLDER_VIEW_GET_IFACE(fv);
    for(l = fm_path_list_peek_head_link(paths);l; l=l->next)
    {
        FmPath* path = FM_PATH(l->data);
        iface->select_file_path(fv, path);
    }
}

#define TEMPL_NAME_FOLDER    NULL
#define TEMPL_NAME_BLANK     (const char*)-1
#define TEMPL_NAME_SHORTCUT  (const char*)-2

/* FIXME: Need to load content of ~/Templates and list available templates in popup menus. */
static void create_new(GtkWindow* parent, FmPath* cwd, const char* templ)
{
    GError* err = NULL;
    FmPath* dest;
    char* basename;
    const char* msg;
    //FmMainWin* win = FM_MAIN_WIN(parent);
_retry:
    if(templ == TEMPL_NAME_FOLDER)
        msg = N_("Enter a name for the newly created folder:");
    else
        msg = N_("Enter a name for the newly created file:");
    basename = fm_get_user_input(parent, _("Create New..."), _(msg), _("New"));
    if(!basename)
        return;

    dest = fm_path_new_child(cwd, basename);
    g_free(basename);

    if( templ == TEMPL_NAME_FOLDER )
    {
        GFile* gf = fm_path_to_gfile(dest);
        if(!g_file_make_directory(gf, NULL, &err))
        {
            if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                fm_path_unref(dest);
                g_error_free(err);
                g_object_unref(gf);
                err = NULL;
                goto _retry;
            }
            fm_show_error(parent, NULL, err->message);
            g_error_free(err);
        }

        if(!err) /* select the newly created file */
        {
            /*FIXME: this doesn't work since the newly created file will
             * only be shown after file-created event was fired on its
             * folder's monitor and after FmFolder handles it in idle
             * handler. So, we cannot select it since it's not yet in
             * the folder model now. */
            /* fm_folder_view_select_file_path(fv, dest); */
        }
        g_object_unref(gf);
    }
    else if( templ == TEMPL_NAME_BLANK )
    {
        /* FIXME: should be implemented by templates support */
        GFile* gf = fm_path_to_gfile(dest);
        GFileOutputStream* f = g_file_create(gf, G_FILE_CREATE_NONE, NULL, &err);
        if(f)
        {
            g_output_stream_close(G_OUTPUT_STREAM(f), NULL, NULL);
            g_object_unref(f);
        }
        else
        {
            if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                fm_path_unref(dest);
                g_error_free(err);
                g_object_unref(gf);
                err = NULL;
                goto _retry;
            }
            fm_show_error(parent, NULL, err->message);
            g_error_free(err);
        }

        if(!err) /* select the newly created file */
        {
            /*FIXME: this doesn't work since the newly created file will
             * only be shown after file-created event was fired on its
             * folder's monitor and after FmFolder handles it in idle
             * handler. So, we cannot select it since it's not yet in
             * the folder model now. */
            /* fm_folder_view_select_file_path(fv, dest); */
        }
        g_object_unref(gf);
    }
    else if ( templ == TEMPL_NAME_SHORTCUT )
    {
        /* FIXME: a temp. workaround until ~/Templates support is implemented */
         char buf[256];
         GFile* gf = fm_path_to_gfile(dest);

         if (g_find_program_in_path("lxshortcut"))
         {
            char* path = g_file_get_path(gf);
            int s = snprintf(buf, sizeof(buf), "lxshortcut -i %s", path);
            g_free(path);
            if(s >= (int)sizeof(buf))
                buf[0] = '\0';
         }
         else
         {
             GtkWidget* msg;

             msg = gtk_message_dialog_new( NULL,
                                           0,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_OK,
                                           _("Error, lxshortcut not installed") );
             gtk_dialog_run( GTK_DIALOG(msg) );
             gtk_widget_destroy( msg );
         }
         if(buf[0] && !g_spawn_command_line_async(buf, NULL))
            fm_show_error(parent, NULL, _("Failed to start lxshortcut"));
         g_object_unref(gf);
    }
    else /* templates in ~/Templates */
    {
        /* FIXME: need an extended processing with desktop entries support */
        FmPath* dir = fm_path_new_for_str(g_get_user_special_dir(G_USER_DIRECTORY_TEMPLATES));
        FmPath* template = fm_path_new_child(dir, templ);
        fm_copy_file(parent, template, cwd);
        fm_path_unref(template);
        fm_path_unref(dir);
    }
    fm_path_unref(dest);
}

static void on_create_new(GtkAction* act, FmFolderView* fv)
{
    const char* name = gtk_action_get_name(act);
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    GtkWidget *win = gtk_menu_get_attach_widget(popup);

    if(strncmp(name, "NewFolder", 9) == 0)
        name = TEMPL_NAME_FOLDER;
    /* FIXME: add XDG_TEMPLATE support for anything but folder */
    else if(strcmp(name, "NewBlank") == 0)
        name = TEMPL_NAME_BLANK;
    else if(strcmp(name, "NewShortcut") == 0)
        name = TEMPL_NAME_SHORTCUT;
    create_new(GTK_WINDOW(win), fm_folder_view_get_cwd(fv), name);
}

static void on_cut(GtkAction* act, FmFolderView* fv)
{
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    GtkWidget *win = gtk_menu_get_attach_widget(popup);
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(win));

    /* check if we cut inside the view; for desktop focus will be NULL */
    if(focus == NULL || gtk_widget_is_ancestor(focus, GTK_WIDGET(fv)))
    {
        FmPathList* files = fm_folder_view_dup_selected_file_paths(fv);
        if(files)
        {
            fm_clipboard_cut_files(win, files);
            fm_path_list_unref(files);
        }
    }
    else if(GTK_IS_EDITABLE(focus) && /* fallback for editables */
            gtk_editable_get_selection_bounds((GtkEditable*)focus, NULL, NULL))
        gtk_editable_cut_clipboard((GtkEditable*)focus);
}

static void on_copy(GtkAction* act, FmFolderView* fv)
{
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    GtkWidget *win = gtk_menu_get_attach_widget(popup);
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(win));

    /* check if we copy inside the view; for desktop focus will be NULL */
    if(focus == NULL || gtk_widget_is_ancestor(focus, GTK_WIDGET(fv)))
    {
        FmPathList* files = fm_folder_view_dup_selected_file_paths(fv);
        if(files)
        {
            fm_clipboard_copy_files(win, files);
            fm_path_list_unref(files);
        }
    }
    else if(GTK_IS_EDITABLE(focus) && /* fallback for editables */
            gtk_editable_get_selection_bounds((GtkEditable*)focus, NULL, NULL))
        gtk_editable_copy_clipboard((GtkEditable*)focus);
}

static void on_paste(GtkAction* act, FmFolderView* fv)
{
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    GtkWidget *win = gtk_menu_get_attach_widget(popup);
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(win));

    /* check if we paste inside the view; for desktop focus will be NULL */
    if(focus == NULL || gtk_widget_is_ancestor(focus, GTK_WIDGET(fv)))
    {
        FmPath* path = fm_folder_view_get_cwd(fv);
        fm_clipboard_paste_files(GTK_WIDGET(fv), path);
    }
    else if(GTK_IS_EDITABLE(focus)) /* fallback for editables */
        gtk_editable_paste_clipboard((GtkEditable*)focus);
}

static void on_trash(GtkAction* act, FmFolderView* fv)
{
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    GtkWidget *win = gtk_menu_get_attach_widget(popup);
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(win));

    /* check if user pressed 'Del' inside the view; for desktop focus will be NULL */
    if(focus == NULL || gtk_widget_is_ancestor(focus, GTK_WIDGET(fv)))
    {
        FmPathList* files = fm_folder_view_dup_selected_file_paths(fv);
        if(files)
        {
            fm_trash_or_delete_files(GTK_WINDOW(win), files);
            fm_path_list_unref(files);
        }
    }
    else if(GTK_IS_EDITABLE(focus)) /* fallback for editables */
    {
        if(!gtk_editable_get_selection_bounds((GtkEditable*)focus, NULL, NULL))
        {
            gint pos = gtk_editable_get_position((GtkEditable*)focus);
            /* if no text selected then delete character next to cursor */
            gtk_editable_select_region((GtkEditable*)focus, pos, pos + 1);
        }
        gtk_editable_delete_selection((GtkEditable*)focus);
    }
}

static void on_rm(GtkAction* act, FmFolderView* fv)
{
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    GtkWidget *win = gtk_menu_get_attach_widget(popup);
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(win));

    /* check if user pressed 'Shift+Del' inside the view */
    if(focus == NULL || gtk_widget_is_ancestor(focus, GTK_WIDGET(fv)))
    {
        FmPathList* files = fm_folder_view_dup_selected_file_paths(fv);
        if(files)
        {
            fm_delete_files(GTK_WINDOW(win), files);
            fm_path_list_unref(files);
        }
    }
    /* for editables 'Shift+Del' means 'Cut' */
    else if(GTK_IS_EDITABLE(focus))
        gtk_editable_cut_clipboard((GtkEditable*)focus);
}

static void on_select_all(GtkAction* act, FmFolderView* fv)
{
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    GtkWidget *win = gtk_menu_get_attach_widget(popup);
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(win));

    /* check if we are inside the view; for desktop focus will be NULL */
    if(focus == NULL || gtk_widget_is_ancestor(focus, GTK_WIDGET(fv)))
        fm_folder_view_select_all(fv);
    else if(GTK_IS_EDITABLE(focus)) /* fallback for editables */
        gtk_editable_select_region((GtkEditable*)focus, 0, -1);
}

static void on_invert_select(GtkAction* act, FmFolderView* fv)
{
    fm_folder_view_select_invert(fv);
}

static void on_rename(GtkAction* act, FmFolderView* fv)
{
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    GtkWidget *win = gtk_menu_get_attach_widget(popup);

    /* FIXME: is it OK to rename folder itself? */
    fm_rename_file(GTK_WINDOW(win), fm_folder_view_get_cwd(fv));
}

static void on_prop(GtkAction* act, FmFolderView* fv)
{
    FmFolder* folder = fm_folder_view_get_folder(fv);

    if(folder && fm_folder_is_valid(folder))
    {
        GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
        GtkWidget *win = gtk_menu_get_attach_widget(popup);
        FmFileInfo* fi = fm_folder_get_info(folder);
        FmFileInfoList* files = fm_file_info_list_new();
        fm_file_info_list_push_tail(files, fi);
        fm_show_file_properties(GTK_WINDOW(win), files);
        fm_file_info_list_unref(files);
    }
}

static void on_file_prop(GtkAction* act, FmFolderView* fv)
{
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    GtkWidget *win = gtk_menu_get_attach_widget(popup);
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(win));

    /* check if it is inside the view; for desktop focus will be NULL */
    if(focus == NULL || gtk_widget_is_ancestor(focus, GTK_WIDGET(fv)))
    {
        FmFileInfoList* files = fm_folder_view_dup_selected_files(fv);
        if(files)
        {
            fm_show_file_properties(GTK_WINDOW(win), files);
            fm_file_info_list_unref(files);
        }
    }
}

static inline gboolean pointer_is_over_widget(GtkWidget* fv)
{
#if GTK_CHECK_VERSION(3, 0, 0)
    GtkStateFlags fl = gtk_widget_get_state_flags(fv);
    return (fl & GTK_STATE_FLAG_PRELIGHT) != 0;
#else
    GtkAllocation a;
    gint x, y;
    gtk_widget_get_allocation(fv, &a);
    gtk_widget_get_pointer(fv, &x, &y);
    /* g_debug("pointer is %d,%d out of %d,%d", x, y, a.width, a.height); */
    if(x < 0 || y < 0 || x > a.width || y > a.height)
        return FALSE;
    return TRUE;
#endif
}

/* if it's mouse event then act is NULL, see fm_folder_view_item_clicked() */
static void on_menu(GtkAction* act, FmFolderView* fv)
{
    GtkUIManager *ui = g_object_get_qdata(G_OBJECT(fv), ui_quark);
    GtkMenu *popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    FmFolderViewInterface *iface = FM_FOLDER_VIEW_GET_IFACE(fv);
    FmFolderModel *model;
    gboolean show_hidden;
    FmSortMode mode;
    GtkSortType type = GTK_SORT_ASCENDING;
    FmFolderModelCol by;

    /* FIXME: realize popup window and put it in the fv (honoring monitor) */
    /* don't show context menu outside of the folder view */
    if(act != NULL && !pointer_is_over_widget(GTK_WIDGET(fv)))
        return;
    /* FIXME: if act != NULL and there is selection in fv then open file menu instead */
    /* update actions */
    model = iface->get_model(fv);
    if(fm_folder_model_get_sort(model, &by, &mode))
        type = FM_SORT_IS_ASCENDING(mode) ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING;
    act = gtk_ui_manager_get_action(ui, "/popup/Sort/Asc");
    /* g_debug("set /popup/Sort/Asc: %u", type); */
    gtk_radio_action_set_current_value(GTK_RADIO_ACTION(act), type);
    act = gtk_ui_manager_get_action(ui, "/popup/Sort/ByName");
    if(by == FM_FOLDER_MODEL_COL_DEFAULT)
        by = FM_FOLDER_MODEL_COL_NAME;
    /* g_debug("set /popup/Sort/ByName: %u", by); */
    gtk_radio_action_set_current_value(GTK_RADIO_ACTION(act), by);
    show_hidden = iface->get_show_hidden(fv);
    act = gtk_ui_manager_get_action(ui, "/popup/ShowHidden");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), show_hidden);
    /* shadow 'Paste' if clipboard is empty and unshadow if not */
    act = gtk_ui_manager_get_action(ui, "/popup/Paste");
    gtk_action_set_sensitive(act, fm_clipboard_have_files(GTK_WIDGET(fv)));
    /* open popup */
    gtk_menu_popup(popup, NULL, NULL, NULL, NULL, 3, gtk_get_current_event_time());
}

static void on_show_hidden(GtkToggleAction* act, FmFolderView* fv)
{
    gboolean active = gtk_toggle_action_get_active(act);
    fm_folder_view_set_show_hidden(fv, active);
}

static void on_change_by(GtkRadioAction* act, GtkRadioAction* cur, FmFolderView* fv)
{
    guint val = gtk_radio_action_get_current_value(cur);
    FmFolderModel *model = fm_folder_view_get_model(fv);

    /* g_debug("on_change_by"); */
    if(model)
        fm_folder_model_set_sort(model, val, FM_SORT_DEFAULT);
}

static void on_change_type(GtkRadioAction* act, GtkRadioAction* cur, FmFolderView* fv)
{
    guint val = gtk_radio_action_get_current_value(cur);
    FmFolderModel *model = fm_folder_view_get_model(fv);
    FmSortMode mode;

    /* g_debug("on_change_type"); */
    if(model)
    {
        fm_folder_model_get_sort(model, NULL, &mode);
        mode &= ~FM_SORT_ORDER_MASK;
        mode |= (val == GTK_SORT_ASCENDING) ? FM_SORT_ASCENDING : FM_SORT_DESCENDING;
        fm_folder_model_set_sort(model, FM_FOLDER_MODEL_COL_DEFAULT, mode);
    }
}

static void on_ui_destroy(gpointer ui_ptr)
{
    GtkUIManager* ui = (GtkUIManager*)ui_ptr;
    GtkMenu* popup = GTK_MENU(gtk_ui_manager_get_widget(ui, "/popup"));
    GtkWindow* win = GTK_WINDOW(gtk_menu_get_attach_widget(popup));
    GtkAccelGroup* accel_grp = gtk_ui_manager_get_accel_group(ui);
    GSList *groups;

    groups = gtk_accel_groups_from_object(G_OBJECT(win));
    if(g_slist_find(groups, accel_grp) != NULL)
        gtk_window_remove_accel_group(win, accel_grp);
    gtk_widget_destroy(GTK_WIDGET(popup));
    g_object_unref(ui);
}

/**
 * fm_folder_view_add_popup
 * @fv: a widget to apply
 * @parent: parent window of @fv
 * @update_popup: function to extend popup menu for folder
 *
 * Adds popup menu to window @parent associated with widget @fv. This
 * includes hotkeys for popup menu items. Popup will be destroyed and
 * hotkeys will be removed from @parent when @fv is finalized.
 *
 * Returns: (transfer none): a new created widget.
 *
 * Since: 1.0.1
 */
GtkMenu* fm_folder_view_add_popup(FmFolderView* fv, GtkWindow* parent,
                                  FmFolderViewUpdatePopup update_popup)
{
    FmFolderViewInterface* iface;
    FmFolderModel* model;
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkMenu* popup;
    GtkAction* act;
    GtkAccelGroup* accel_grp;
    gboolean show_hidden;
    FmSortMode mode;
    GtkSortType type = (GtkSortType)-1;
    FmFolderModelCol by = (FmFolderModelCol)-1;

    iface = FM_FOLDER_VIEW_GET_IFACE(fv);
    show_hidden = iface->get_show_hidden(fv);
    model = iface->get_model(fv);
    if(fm_folder_model_get_sort(model, &by, &mode))
        type = FM_SORT_IS_ASCENDING(mode) ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING;

    /* init popup from XML string */
    ui = gtk_ui_manager_new();
    act_grp = gtk_action_group_new("Folder");
    gtk_action_group_set_translation_domain(act_grp, GETTEXT_PACKAGE);
    gtk_action_group_add_actions(act_grp, folder_popup_actions,
                                 G_N_ELEMENTS(folder_popup_actions), fv);
    gtk_action_group_add_toggle_actions(act_grp, folder_toggle_actions,
                                        G_N_ELEMENTS(folder_toggle_actions), fv);
    gtk_action_group_add_radio_actions(act_grp, folder_sort_type_actions,
                                       G_N_ELEMENTS(folder_sort_type_actions),
                                       type, G_CALLBACK(on_change_type), fv);
    gtk_action_group_add_radio_actions(act_grp, folder_sort_by_actions,
                                       G_N_ELEMENTS(folder_sort_by_actions),
                                       by, G_CALLBACK(on_change_by), fv);
    gtk_ui_manager_insert_action_group(ui, act_grp, 0);
    gtk_ui_manager_add_ui_from_string(ui, folder_popup_xml, -1, NULL);
    act = gtk_ui_manager_get_action(ui, "/popup/ShowHidden");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), show_hidden);
    act = gtk_ui_manager_get_action(ui, "/popup/Cut");
    gtk_action_set_visible(act, FALSE);
    act = gtk_ui_manager_get_action(ui, "/popup/Copy");
    gtk_action_set_visible(act, FALSE);
    act = gtk_ui_manager_get_action(ui, "/popup/Del");
    gtk_action_set_visible(act, FALSE);
    act = gtk_ui_manager_get_action(ui, "/popup/Remove");
    gtk_action_set_visible(act, FALSE);
    act = gtk_ui_manager_get_action(ui, "/popup/FileProp");
    gtk_action_set_visible(act, FALSE);
    if(update_popup)
        update_popup(fv, parent, ui, act_grp, NULL);
    popup = GTK_MENU(gtk_ui_manager_get_widget(ui, "/popup"));
    accel_grp = gtk_ui_manager_get_accel_group(ui);
    gtk_window_add_accel_group(parent, accel_grp);
    gtk_menu_attach_to_widget(popup, GTK_WIDGET(parent), NULL);
    g_object_unref(act_grp);
    g_object_set_qdata_full(G_OBJECT(fv), ui_quark, ui, on_ui_destroy);
    g_object_set_qdata(G_OBJECT(fv), popup_quark, popup);
    return popup;
}

/**
 * fm_folder_view_bounce_action
 * @act: an action to execute
 * @fv: a widget to apply
 *
 * Executes the action with the same name as @act in popup menu of @fv.
 * The popup menu should be created with fm_folder_view_add_popup()
 * before this call.
 *
 * Implemented actions are:
 * - Cut       : cut files into clipboard
 * - Copy      : copy files into clipboard
 * - Paste     : paste files from clipboard
 * - Del       : move files into trash bin
 * - Remove    : delete files from filesystem
 * - SelAll    : select all
 * - InvSel    : invert selection
 * - Rename    : rename the folder
 * - Prop      : folder properties dialog
 * - FileProp  : file properties dialog
 * - NewFolder : create new folder here
 *
 * See also: fm_folder_view_add_popup().
 *
 * Since: 1.0.1
 */
void fm_folder_view_bounce_action(GtkAction* act, FmFolderView* fv)
{
    const gchar *name;
    GtkUIManager *ui;
    GList *groups;

    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));
    g_return_if_fail(act != NULL);

    ui = g_object_get_qdata(G_OBJECT(fv), ui_quark);
    g_return_if_fail(ui != NULL && GTK_IS_UI_MANAGER(ui));

    groups = gtk_ui_manager_get_action_groups(ui);
    g_return_if_fail(groups != NULL);

    name = gtk_action_get_name(act);
    act = gtk_action_group_get_action((GtkActionGroup*)groups->data, name);
    if(act)
        gtk_action_activate(act);
    else
        g_debug("requested action %s wasn't found in popup", name);
}

/**
 * fm_folder_view_set_active
 * @fv: the folder view widget to apply
 * @set: state of accelerators to be set
 *
 * If @set is %TRUE then activates accelerators on the @fv that were
 * created with fm_folder_view_add_popup() before. If @set is %FALSE
 * then deactivates accelerators on the @fv. This API is useful if the
 * application window contains more than one folder view so gestures
 * will be used only on active view. This API has no effect in no popup
 * menu was created with fm_folder_view_add_popup() before this call.
 *
 * See also: fm_folder_view_add_popup().
 *
 * Since: 1.0.1
 */
void fm_folder_view_set_active(FmFolderView* fv, gboolean set)
{
    GtkUIManager *ui;
    GtkMenu *popup;
    GtkWindow* win;
    GtkAccelGroup* accel_grp;
    GSList *groups;
    gboolean active;

    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    ui = g_object_get_qdata(G_OBJECT(fv), ui_quark);
    popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);

    g_return_if_fail(ui != NULL && GTK_IS_UI_MANAGER(ui));
    g_return_if_fail(popup != NULL && GTK_IS_MENU(popup));

    win = GTK_WINDOW(gtk_menu_get_attach_widget(popup));
    accel_grp = gtk_ui_manager_get_accel_group(ui);
    groups = gtk_accel_groups_from_object(G_OBJECT(win));
    active = (g_slist_find(groups, accel_grp) != NULL);

    if(set && !active)
        gtk_window_add_accel_group(win, accel_grp);
    else if(!set && active)
        gtk_window_remove_accel_group(win, accel_grp);
}

/**
 * fm_folder_view_item_clicked
 * @fv: the folder view widget
 * @path: (allow-none): path to current pointed item
 * @type: what click was received
 *
 * Handles left click and right click in folder area. If some item was
 * left-clicked then fm_folder_view_item_clicked() tries to launch it.
 * If some item was right-clicked then opens file menu (applying the
 * update_popup returned by get_custom_menu_callbacks interface function
 * before opening it if it's not %NULL). If it was right-click on empty
 * space of folder view (so @path is %NULL) then opens folder popup
 * menu that was created by fm_folder_view_add_popup(). After that
 * emits the #FmFolderView::clicked signal.
 *
 * If open_folders callback from interface function get_custom_menu_callbacks
 * is %NULL then assume it was old API call so click will be not handled
 * by this function and signal handler will handle it instead.
 *
 * This API is internal for #FmFolderView and should be used only in
 * class implementations.
 *
 * Since: 1.0.1
 */
void fm_folder_view_item_clicked(FmFolderView* fv, GtkTreePath* path,
                                 FmFolderViewClickType type)
{
    FmFolderViewInterface* iface;
    GtkTreeModel* model;
    FmFileInfo* fi;
    const char* target;
    GtkMenu *popup;
    GtkWindow *win;
    FmFolderViewUpdatePopup update_popup;
    FmLaunchFolderFunc open_folders;
    GtkTreeIter it;

    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    iface = FM_FOLDER_VIEW_GET_IFACE(fv);
    if(path)
    {
        model = GTK_TREE_MODEL(iface->get_model(fv));
        if(gtk_tree_model_get_iter(model, &it, path))
            gtk_tree_model_get(model, &it, FM_FOLDER_MODEL_COL_INFO, &fi, -1);
    }
    else
        fi = NULL;
    popup = g_object_get_qdata(G_OBJECT(fv), popup_quark);
    if(popup == NULL) /* no fm_folder_view_add_popup() was called before */
        goto send_signal;
    win = GTK_WINDOW(gtk_menu_get_attach_widget(popup));
    /* handle left and rigth clicks */
    iface->get_custom_menu_callbacks(fv, &update_popup, &open_folders);
    /* if open_folders is NULL then it's old API call so don't handle */
    if(open_folders) switch(type)
    {
    case FM_FV_ACTIVATED: /* file activated */
        target = fm_file_info_get_target(fi);
        if(target && !fm_file_info_is_symlink(fi))
        {
            /* symlinks also has fi->target, but we only handle shortcuts here. */
            FmPath* real_path = fm_path_new_for_str(target);
            fm_launch_path_simple(win, NULL, real_path, open_folders, win);
            fm_path_unref(real_path);
        }
        else
            fm_launch_file_simple(win, NULL, fi, open_folders, win);
        break;
    case FM_FV_CONTEXT_MENU:
        if(fi)
        {
            FmFileMenu* menu;
            FmFileInfoList* files = iface->dup_selected_files(fv);

            /* workaround on ExoTreeView bug */
            if(files == NULL)
            {
                on_menu(NULL, fv);
                goto send_signal;
            }

            menu = fm_file_menu_new_for_files(win, files, fm_folder_view_get_cwd(fv), TRUE);
            fm_file_menu_set_folder_func(menu, open_folders, win);

            /* TODO: add info message on selection if enabled in config */
            /* merge some specific menu items */
            if(update_popup)
                update_popup(fv, win, fm_file_menu_get_ui(menu),
                             fm_file_menu_get_action_group(menu), files);
            fm_file_info_list_unref(files);

            popup = fm_file_menu_get_menu(menu);
            gtk_menu_popup(popup, NULL, NULL, NULL, fi, 3, gtk_get_current_event_time());
        }
        else /* no files are selected. Show context menu of current folder. */
            on_menu(NULL, fv);
        break;
    default: ;
    }
send_signal:
    /* send signal */
    g_signal_emit(fv, signals[CLICKED], 0, type, fi);
}

/**
 * fm_folder_view_sel_changed
 * @obj: some object; unused
 * @fv: the folder view widget to apply
 *
 * Emits the #FmFolderView::sel-changed signal.
 *
 * This API is internal for #FmFolderView and should be used only in
 * class implementations.
 *
 * Since: 1.0.1
 */
void fm_folder_view_sel_changed(GObject* obj, FmFolderView* fv)
{
    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    /* if someone is connected to our "sel-changed" signal. */
    if(g_signal_has_handler_pending(fv, signals[SEL_CHANGED], 0, TRUE))
    {
        FmFolderViewInterface* iface = FM_FOLDER_VIEW_GET_IFACE(fv);
        gint files = iface->count_selected_files(fv);

        /* emit a selection changed notification to the world. */
        g_signal_emit(fv, signals[SEL_CHANGED], 0, files);
    }
}

#if 0
/**
 * fm_folder_view_chdir
 * @fv: the folder view widget to apply
 *
 * Emits the #FmFolderView::chdir signal.
 *
 * This API is internal for #FmFolderView and should be used only in
 * class implementations.
 *
 * Since: 1.0.2
 */
void fm_folder_view_chdir(FmFolderView* fv, FmPath* path)
{
    g_return_if_fail(FM_IS_FOLDER_VIEW(fv));

    g_signal_emit(fv, signals[CHDIR], 0, path);
}
#endif

/* FIXME: make VTable entries after ABI change! */
gboolean _fm_standard_view_set_columns(FmFolderView* fv, const GSList* cols);
GSList* _fm_standard_view_get_columns(FmFolderView* fv);

/**
 * fm_folder_view_set_columns
 * @fv: the folder view widget to apply
 * @cols: (element-type FmFolderViewColumnInfo): new list of column infos
 *
 * Changes composition (rendering) of folder view @fv in accordance to
 * new list of column infos.
 *
 * Returns: %TRUE in case of success.
 *
 * Since: 1.0.2
 */
gboolean fm_folder_view_set_columns(FmFolderView* fv, const GSList* cols)
{
    // FIXME: call via VTable!
    return _fm_standard_view_set_columns(fv, cols);
}

/**
 * fm_folder_view_get_columns
 * @fv: the folder view widget to query
 *
 * Retrieves current composition of @fv as list of column infos. Returned
 * list should be freed with g_slist_free() after usage.
 *
 * Returns: (transfer container) (element-type FmFolderViewColumnInfo): list of column infos.
 *
 * Since: 1.0.2
 */
GSList* fm_folder_view_get_columns(FmFolderView* fv)
{
    // FIXME: call via VTable!
    return _fm_standard_view_get_columns(fv);
}
