//      fm-side-pane.h
//
//      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.


#ifndef __FM_SIDE_PANE_H__
#define __FM_SIDE_PANE_H__

#include <gtk/gtk.h>
#include "fm-path.h"

G_BEGIN_DECLS


#define FM_TYPE_SIDE_PANE                (fm_side_pane_get_type())
#define FM_SIDE_PANE(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_TYPE_SIDE_PANE, FmSidePane))
#define FM_SIDE_PANE_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_TYPE_SIDE_PANE, FmSidePaneClass))
#define FM_IS_SIDE_PANE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_TYPE_SIDE_PANE))
#define FM_IS_SIDE_PANE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_TYPE_SIDE_PANE))
#define FM_SIDE_PANE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),\
            FM_TYPE_SIDE_PANE, FmSidePaneClass))

typedef struct _FmSidePane            FmSidePane;
typedef struct _FmSidePaneClass        FmSidePaneClass;

/**
 * FmSidePaneMode:
 * @FM_SP_NONE: invalid mode
 * @FM_SP_PLACES: #FmPlacesView mode
 * @FM_SP_DIR_TREE: #FmDirTreeView mode
 * @FM_SP_REMOTE: reserved mode
 *
 * Mode of side pane view.
 */
typedef enum
{
    FM_SP_NONE,
    FM_SP_PLACES,
    FM_SP_DIR_TREE,
    FM_SP_REMOTE
}FmSidePaneMode;

struct _FmSidePane
{
    GtkVBox parent;
    FmPath* cwd;
    GtkWidget* title_bar;
    GtkWidget* menu_btn;
    GtkWidget* menu_label;
    GtkWidget* menu;
    GtkWidget* scroll;
    GtkWidget* view;
    FmSidePaneMode mode;
    GtkUIManager* ui;
};

/**
 * FmSidePaneClass:
 * @parent_class: the parent class
 * @chdir: the class closure for the #FmSidePane::chdir signal
 * @mode_changed: the class closure for the #FmSidePane::mode-changed signal
 */
struct _FmSidePaneClass
{
    GtkVBoxClass parent_class;
    void (*chdir)(FmSidePane* sp, guint button, FmPath* path);
    void (*mode_changed)(FmSidePane* sp);
};


GType fm_side_pane_get_type        (void);
FmSidePane* fm_side_pane_new       (void);

FmPath* fm_side_pane_get_cwd(FmSidePane* sp);
void fm_side_pane_chdir(FmSidePane* sp, FmPath* path);

void fm_side_pane_set_mode(FmSidePane* sp, FmSidePaneMode mode);
FmSidePaneMode fm_side_pane_get_mode(FmSidePane* sp);

GtkWidget* fm_side_pane_get_title_bar(FmSidePane* sp);

G_END_DECLS

#endif /* __FM_SIDE_PANE_H__ */
