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
	GtkCellRendererClass* render_class = (GtkCellRendererClass*)fm_cell_renderer_text_parent_class;
	int state;
	GdkRectangle rect;

	/* FIXME: this is time-consuming since it invokes pango_layout.
	 *        if we want to fix this, we must implement the whole cell
	 *        renderer ourselves instead of derived from GtkCellRendererText. */
	if(flags & (GTK_CELL_RENDERER_SELECTED|GTK_CELL_RENDERER_FOCUSED))
	{
		gint x_off, y_off, width, height;
		render_class->get_size(cell, widget, cell_area, &x_off, &y_off, &width, &height);
		/* FIXME: is the x & y offsets calculation correct? */
		rect.x = cell_area->x + x_off /*+ cell->xpad*/ + (cell_area->width-width)/2;
		rect.y = cell_area->y + y_off /*+ cell->ypad*/;
		rect.width = width;
		rect.height = height;
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

	render_class->render(cell, window, widget, background_area, cell_area, expose_area, flags);

	if(G_UNLIKELY( flags & GTK_CELL_RENDERER_FOCUSED) ) /* focused */
	{
		gtk_paint_focus(widget->style, window, state, background_area, 
						widget, "cellrenderertext", 
						rect.x, rect.y,
						rect.width, rect.height);
	}
}
