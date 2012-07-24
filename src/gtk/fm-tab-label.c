/*
 *      fm-tab-label.c
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
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
 * SECTION:fm-tab-label
 * @short_description: A tab label widget.
 * @title: FmTabLabel
 *
 * @include: libfm/fm-tab-label.h
 *
 * The #FmTabLabel is a widget that can be used as a label of tab in
 * notebook-like folders view.
 */

#include "fm-tab-label.h"

G_DEFINE_TYPE(FmTabLabel, fm_tab_label, GTK_TYPE_EVENT_BOX);

static void fm_tab_label_class_init(FmTabLabelClass *klass)
{
    /* special style used by close button */
    gtk_rc_parse_string(
        "style \"close-btn-style\" {\n"
            "GtkWidget::focus-padding = 0\n"
            "GtkWidget::focus-line-width = 0\n"
            "xthickness = 0\n"
            "ythickness = 0\n"
        "}\n"
        "widget \"*.tab-close-btn\" style \"close-btn-style\""
    );
}

static void on_close_btn_style_set(GtkWidget *btn, GtkRcStyle *prev, gpointer data)
{
    gint w, h;
    gtk_icon_size_lookup_for_settings(gtk_widget_get_settings(btn), GTK_ICON_SIZE_MENU, &w, &h);
    gtk_widget_set_size_request(btn, w + 2, h + 2);
}

static gboolean on_query_tooltip(GtkWidget *widget, gint x, gint y,
                                 gboolean    keyboard_mode,
                                 GtkTooltip *tooltip, gpointer user_data)
{
    /* We should only show the tooltip if the text is ellipsized */
    GtkLabel* label = GTK_LABEL(widget);
    PangoLayout* layout = gtk_label_get_layout(label);
    if(pango_layout_is_ellipsized(layout))
    {
        gtk_tooltip_set_text(tooltip, gtk_label_get_text(label));
        return TRUE;
    }
    return FALSE;
}

static void fm_tab_label_init(FmTabLabel *self)
{
    GtkBox* hbox;

    gtk_event_box_set_visible_window(GTK_EVENT_BOX(self), FALSE);
    hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));

    self->label = (GtkLabel*)gtk_label_new("");
    gtk_widget_set_has_tooltip((GtkWidget*)self->label, TRUE);
    gtk_box_pack_start(hbox, (GtkWidget*)self->label, FALSE, FALSE, 4 );
    g_signal_connect(self->label, "query-tooltip", G_CALLBACK(on_query_tooltip), self);

    self->close_btn = (GtkButton*)gtk_button_new();
    gtk_button_set_focus_on_click(self->close_btn, FALSE);
    gtk_button_set_relief(self->close_btn, GTK_RELIEF_NONE );
    gtk_container_add ( GTK_CONTAINER ( self->close_btn ),
                        gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
    gtk_container_set_border_width(GTK_CONTAINER(self->close_btn), 0);
    gtk_widget_set_name((GtkWidget*)self->close_btn, "tab-close-btn");
    g_signal_connect(self->close_btn, "style-set", G_CALLBACK(on_close_btn_style_set), NULL);

    gtk_box_pack_end( hbox, (GtkWidget*)self->close_btn, FALSE, FALSE, 0 );

    gtk_container_add(GTK_CONTAINER(self), (GtkWidget*)hbox);
    gtk_widget_show_all((GtkWidget*)hbox);

/*
    gtk_drag_dest_set ( GTK_WIDGET( evt_box ), GTK_DEST_DEFAULT_ALL,
                        drag_targets,
                        sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
                        GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );
    g_signal_connect ( ( gpointer ) evt_box, "drag-motion",
                       G_CALLBACK ( on_tab_drag_motion ),
                       file_browser );
*/
}

/**
 * fm_tab_label_new
 * @text: text to display as a tab label
 *
 * Creates new tab label widget.
 *
 * Returns: (transfer full): a new #FmTabLabel widget.
 *
 * Since: 0.1.10
 */
FmTabLabel *fm_tab_label_new(const char* text)
{
    FmTabLabel* label = (FmTabLabel*)g_object_new(FM_TYPE_TAB_LABEL, NULL);
    gtk_label_set_text(label->label, text);
    return label;
}

/**
 * fm_tab_label_set_text
 * @label: a tab label widget
 * @text: text to display as a tab label
 *
 * Changes text on the @label.
 *
 * Since: 0.1.10
 */
void fm_tab_label_set_text(FmTabLabel* label, const char* text)
{
    gtk_label_set_text(label->label, text);
}

/**
 * fm_tab_label_set_tooltip_text
 * @label: a tab label widget
 * @text: text to display in label tooltip
 *
 * Changes text of tooltip on the @label.
 *
 * Since: 1.0.0
 */
void fm_tab_label_set_tooltip_text(FmTabLabel* label, const char* text)
{
    gtk_widget_set_tooltip_text(GTK_WIDGET(label->label), text);
}
