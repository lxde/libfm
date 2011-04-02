/*
 *      fm-path-bar.c
 *
 *      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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


#include "fm-path-bar.h"
#include <string.h>

enum{
    CHDIR,
    N_SIGNALS
};

static GQuark btn_data_id = 0;
static guint signals[N_SIGNALS];

static void fm_path_bar_finalize            (GObject *object);

G_DEFINE_TYPE(FmPathBar, fm_path_bar, GTK_TYPE_HBOX)

static void on_size_allocate(GtkWidget* widget, GtkAllocation* alloc)
{
    FmPathBar* bar = FM_PATH_BAR(widget);
    GtkRequisition req;
    gtk_widget_size_request(bar->btn_box, &req);
    if(req.width > alloc->width) /* required width > allocated */
    {
        /* show scroll buttons */
        gtk_widget_show(bar->left_scroll);
        gtk_widget_show(bar->right_scroll);
    }
    else
    {
        /* hide scroll buttons and show all buttons */
        gtk_widget_hide(bar->left_scroll);
        gtk_widget_hide(bar->right_scroll);
    }
    GTK_WIDGET_CLASS(fm_path_bar_parent_class)->size_allocate(widget, alloc);
}

static void fm_path_bar_class_init(FmPathBarClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass* widget_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_path_bar_finalize;

    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->size_allocate = on_size_allocate;

    btn_data_id = g_quark_from_static_string("FmPathBtn");

    /* chdir signal is emitted when the user toggles a path element
     * in the bar and when a new path is set via fm_path_bar_set_path. */
    signals[CHDIR] =
        g_signal_new("chdir",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmPathBarClass, chdir),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);
}


static void fm_path_bar_finalize(GObject *object)
{
    FmPathBar *bar;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_PATH_BAR(object));

    bar = FM_PATH_BAR(object);
    if(bar->cur_path)
        fm_path_unref(bar->cur_path);
    if(bar->full_path)
        fm_path_unref(bar->full_path);

    G_OBJECT_CLASS(fm_path_bar_parent_class)->finalize(object);
}

static void emit_chdir(FmPathBar* bar, FmPath* path)
{
    g_signal_emit(bar, signals[CHDIR], 0, path);
}

static void on_scroll_btn_clicked(GtkButton* btn, FmPathBar* bar)
{
    GtkAdjustment* hadj = gtk_viewport_get_hadjustment(bar->viewport);
    gdouble value;
    if(btn == bar->left_scroll) /* scroll left */
        value = hadj->value - hadj->page_increment;
    else
        value = hadj->value + hadj->page_increment;
    value = CLAMP (value, hadj->lower, hadj->upper - hadj->page_size);
    gtk_adjustment_set_value(hadj, value);
}

static void on_path_btn_toggled(GtkToggleButton* btn, FmPathBar* bar)
{
    if(gtk_toggle_button_get_active(btn))
    {
        FmPath* path = FM_PATH(g_object_get_qdata(G_OBJECT(btn), btn_data_id));
        fm_path_unref(bar->cur_path);
        bar->cur_path = fm_path_ref(path);
        emit_chdir(bar, path);
    }
}

static void fm_path_bar_init(FmPathBar *bar)
{
    bar->viewport = gtk_viewport_new(NULL, NULL);
    gtk_widget_set_size_request(bar->viewport, 100, -1);

    gtk_viewport_set_shadow_type(GTK_VIEWPORT(bar->viewport), GTK_SHADOW_NONE);
    bar->btn_box = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(bar->viewport), bar->btn_box);

    bar->left_scroll = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(bar->left_scroll), GTK_RELIEF_HALF);
    gtk_container_add(GTK_CONTAINER(bar->left_scroll), gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_NONE));
    g_signal_connect(bar->left_scroll, "clicked", G_CALLBACK(on_scroll_btn_clicked), bar);

    bar->right_scroll = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(bar->right_scroll), GTK_RELIEF_HALF);
    gtk_container_add(GTK_CONTAINER(bar->right_scroll), gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_NONE));
    g_signal_connect(bar->right_scroll, "clicked", G_CALLBACK(on_scroll_btn_clicked), bar);

    gtk_box_pack_start(GTK_BOX(bar), bar->left_scroll, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bar), bar->viewport, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bar), bar->right_scroll, FALSE, TRUE, 0);
    gtk_widget_show_all(GTK_WIDGET(bar));
}


GtkWidget *fm_path_bar_new(void)
{
    return g_object_new(FM_TYPE_PATH_BAR, NULL);
}

FmPath* fm_path_bar_get_path(FmPathBar* bar)
{
    return bar->cur_path;
}

static GtkWidget* create_btn(FmPathBar* bar, GSList* grp, FmPath* path_element)
{
    GtkButton* btn;
    char* label = fm_path_display_basename(path_element);
    if(!path_element->parent) /* this element is root */
    {
        GtkWidget* hbox = gtk_hbox_new(FALSE, 2);
        btn = gtk_radio_button_new(grp);
        gtk_container_add(GTK_CONTAINER(btn), hbox);
        gtk_box_pack_start(GTK_BOX(hbox),
                           gtk_image_new_from_icon_name("drive-harddisk", GTK_ICON_SIZE_BUTTON),
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                           gtk_label_new(label),
                           FALSE, FALSE, 0);
        gtk_widget_show_all(hbox);
    }
    else
    {
        btn = gtk_radio_button_new_with_label(grp, label);
    }
    g_free(label);

    gtk_toggle_button_set_mode(btn, FALSE);
    gtk_widget_show(btn);

    g_object_set_qdata(G_OBJECT(btn), btn_data_id, path_element);
    g_signal_connect(btn, "toggled", G_CALLBACK(on_path_btn_toggled), bar);
    return btn;
}

void fm_path_bar_set_path(FmPathBar* bar, FmPath* path)
{
    FmPath* path_element;
    GtkWidget* btn;
    GSList* grp;
    GList* btns, *l;

    if(bar->cur_path)
    {
        if(path && fm_path_equal(bar->cur_path, path))
            return;
        fm_path_unref(bar->cur_path);
    }
    bar->cur_path = fm_path_ref(path);

    /* check if we already have a button for this path */
    if(bar->full_path)
    {
        int n = 0;
        path_element = bar->full_path;
        while(path_element)
        {
            /* we have a button for this path */
            if(fm_path_equal(path_element, path))
            {
                /* toggle the button */
                btns = gtk_container_get_children(GTK_CONTAINER(bar->btn_box));
                l = g_list_nth_prev(g_list_last(btns), n);
                btn = GTK_WIDGET(l->data);
                g_list_free(btns);
                /* we don't need to emit chdir signal here since later
                 * toggled signal will be triggered on the button, which
                 * in turns emit chdir. */
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn), TRUE);
                return;
            }
            path_element = fm_path_get_parent(path_element);
            ++n;
        }
        /* we don't have a button for it. */
        fm_path_unref(bar->full_path);
    }
    bar->full_path = fm_path_ref(path);

    /* FIXME: if the new path is the subdir of our full_path, actually
     *        we can append several new buttons rather than re-create
     *        all of the buttons. */

    /* destroy existing path element buttons */
    gtk_container_foreach(bar->btn_box, (GtkCallback)gtk_widget_destroy, NULL);
    grp = NULL;
    path_element = path;
    btns = NULL;
    /* create new buttons for the new path */
    while(path_element)
    {
        btn = create_btn(bar, grp, path_element);
        grp = gtk_radio_button_get_group(btn);
        path_element = fm_path_get_parent(path_element);
        btns = g_list_prepend(btns, btn);
    }

    for(l = btns; l; l=l->next)
    {
        btn = GTK_WIDGET(l->data);
        gtk_box_pack_start(GTK_BOX(bar->btn_box), btn, FALSE, TRUE, 0);
    }
    g_list_free(btns);

    emit_chdir(bar, path);
}
