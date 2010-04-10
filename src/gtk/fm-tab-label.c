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

#include "fm-tab-label.h"

typedef struct _FmTabLabelPrivate			FmTabLabelPrivate;

#define FM_TAB_LABEL_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			FM_TYPE_TAB_LABEL, FmTabLabelPrivate))

static void fm_tab_label_finalize  			(GObject *object);

G_DEFINE_TYPE(FmTabLabel, fm_tab_label, GTK_TYPE_EVENT_BOX);

static void fm_tab_label_class_init(FmTabLabelClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_tab_label_finalize;

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

static void fm_tab_label_finalize(GObject *object)
{
	FmTabLabel *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_TAB_LABEL(object));

	self = FM_TAB_LABEL(object);

	G_OBJECT_CLASS(fm_tab_label_parent_class)->finalize(object);
}

static void on_close_btn_style_set(GtkWidget *btn, GtkRcStyle *prev, gpointer data)
{
	gint w, h;
	gtk_icon_size_lookup_for_settings(gtk_widget_get_settings(btn), GTK_ICON_SIZE_MENU, &w, &h);
	gtk_widget_set_size_request(btn, w + 2, h + 2);
}

static void fm_tab_label_init(FmTabLabel *self)
{
    GtkWidget* hbox;

    gtk_event_box_set_visible_window(GTK_EVENT_BOX(self), FALSE);
    hbox = gtk_hbox_new( FALSE, 0 );

    self->label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox), self->label, FALSE, FALSE, 4 );

    self->close_btn = gtk_button_new();
    gtk_button_set_focus_on_click ( GTK_BUTTON ( self->close_btn ), FALSE );
    gtk_button_set_relief( GTK_BUTTON ( self->close_btn ), GTK_RELIEF_NONE );
    gtk_container_add ( GTK_CONTAINER ( self->close_btn ),
                        gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
    gtk_container_set_border_width(GTK_CONTAINER(self->close_btn), 0);
    gtk_widget_set_name(self->close_btn, "tab-close-btn");
    g_signal_connect(self->close_btn, "style-set", G_CALLBACK(on_close_btn_style_set), NULL);

    gtk_box_pack_end( GTK_BOX( hbox ), self->close_btn, FALSE, FALSE, 0 );

    gtk_container_add(GTK_CONTAINER(self), hbox);
    gtk_widget_show_all(hbox);

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

GtkWidget *fm_tab_label_new(const char* text)
{
    FmTabLabel* label = (FmTabLabel*)g_object_new(FM_TYPE_TAB_LABEL, NULL);
    gtk_label_set_text(label->label, text);
	return (GtkWidget*)label;
}

void fm_tab_label_set_text(FmTabLabel* label, const char* text)
{
    gtk_label_set_text(label->label, text);
}
