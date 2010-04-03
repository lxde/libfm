/*
 *      fm-cell-renderer-text.c
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

#include "fm-cell-renderer-text.h"

G_DEFINE_TYPE(FmCellRendererText, fm_cell_renderer_text, GTK_TYPE_CELL_RENDERER_TEXT);

static void fm_cell_renderer_text_render(GtkCellRenderer *cell, 
								GdkDrawable *window,
								GtkWidget *widget,
								GdkRectangle *background_area,
								GdkRectangle *cell_area,
								GdkRectangle *expose_area,
								GtkCellRendererState  flags);

static void fm_cell_renderer_text_class_init(FmCellRendererTextClass *klass)
{
	GtkCellRendererClass* render_class = GTK_CELL_RENDERER_CLASS(klass);
	render_class->render = fm_cell_renderer_text_render;
}


static void fm_cell_renderer_text_init(FmCellRendererText *self)
{
	
}


GtkCellRenderer *fm_cell_renderer_text_new(void)
{
	return (GtkCellRenderer*)g_object_new(FM_CELL_RENDERER_TEXT_TYPE, NULL);
}

void fm_cell_renderer_text_render(GtkCellRenderer *cell, 
								GdkDrawable *window,
								GtkWidget *widget,
								GdkRectangle *background_area,
								GdkRectangle *cell_area,
								GdkRectangle *expose_area,
								GtkCellRendererState flags)
{
	GtkCellRendererText* celltext = (GtkCellRendererText*)cell;
	GtkStateType state;
	gint text_width;
	gint text_height;
	gint x_offset;
	gint y_offset;
	gint x_align_offset;
	GdkRectangle rect;
	PangoWrapMode wrap_mode;
	gint wrap_width;
	PangoAlignment alignment;

	/* FIXME: this is time-consuming since it invokes pango_layout.
	 *        if we want to fix this, we must implement the whole cell
	 *        renderer ourselves instead of derived from GtkCellRendererText. */
	PangoContext* context = gtk_widget_get_pango_context(widget);

	PangoLayout* layout = pango_layout_new(context);

	g_object_get((GObject*)cell,
	             "wrap-mode" , &wrap_mode,
	             "wrap-width", &wrap_width,
	             "alignment" , &alignment,
	             NULL);

	pango_layout_set_alignment(layout, alignment);

	/* Setup the wrapping. */
	if (wrap_width < 0)
	{
		pango_layout_set_width(layout, -1);
		pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);
	}
	else
	{
		pango_layout_set_width(layout, wrap_width * PANGO_SCALE);
		pango_layout_set_wrap(layout, wrap_mode);
	}

	pango_layout_set_text(layout, celltext->text, -1);

	pango_layout_set_auto_dir(layout, TRUE);

	pango_layout_get_pixel_size(layout, &text_width, &text_height);

	/* Calculate the real x and y offsets. */
	x_offset = ((gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL) ? (1.0 - cell->xalign) : cell->xalign)
	         * (cell_area->width - text_width - (2 * cell->xpad));
	x_offset = MAX(x_offset, 0);

	y_offset = cell->yalign * (cell_area->height - text_height - (2 * cell->ypad));
	y_offset = MAX (y_offset, 0);

	if(flags & (GTK_CELL_RENDERER_SELECTED|GTK_CELL_RENDERER_FOCUSED))
	{
		rect.x = cell_area->x + x_offset;
		rect.y = cell_area->y + y_offset;
		rect.width = text_width + (2 * cell->xpad);
		rect.height = text_height + (2 * cell->ypad);
	}

	if(flags & GTK_CELL_RENDERER_SELECTED) /* item is selected */
	{
		cairo_t *cr = gdk_cairo_create (window);
		GdkColor clr;

		if(flags & GTK_CELL_RENDERER_INSENSITIVE) /* insensitive */
			state = GTK_STATE_INSENSITIVE;
		else
			state = GTK_STATE_SELECTED;

		clr = widget->style->bg[state];

		/* paint the background */
		if(expose_area)
		{
			gdk_cairo_rectangle(cr, expose_area);
			cairo_clip(cr);
		}
		gdk_cairo_rectangle(cr, &rect);

		cairo_set_source_rgb(cr, clr.red / 65535., clr.green / 65535., clr.blue / 65535.);
		cairo_fill (cr);

		cairo_destroy (cr);
	}
	else
		state = GTK_STATE_NORMAL;

	x_align_offset = (alignment == PANGO_ALIGN_CENTER) ? (wrap_width - text_width) / 2 : 0;

	gtk_paint_layout(widget->style, window, state, TRUE,
	                 expose_area, widget, "cellrenderertext",
	                 cell_area->x + x_offset + cell->xpad - x_align_offset,
	                 cell_area->y + y_offset + cell->ypad,
	                 layout);

	g_object_unref(layout);

	if(G_UNLIKELY( flags & GTK_CELL_RENDERER_FOCUSED) ) /* focused */
	{
		gtk_paint_focus(widget->style, window, state, background_area, 
						widget, "cellrenderertext", 
						rect.x, rect.y,
						rect.width, rect.height);
	}
}
