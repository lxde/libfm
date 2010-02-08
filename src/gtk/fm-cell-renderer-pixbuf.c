/*
 *      fm-cell-renderer-pixbuf.c
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

#include "fm-cell-renderer-pixbuf.h"

static void fm_cell_renderer_pixbuf_finalize  			(GObject *object);

static void fm_cell_renderer_pixbuf_get_size   (GtkCellRenderer            *cell,
						 GtkWidget                  *widget,
						 GdkRectangle               *rectangle,
						 gint                       *x_offset,
						 gint                       *y_offset,
						 gint                       *width,
						 gint                       *height);
static void fm_cell_renderer_pixbuf_render     (GtkCellRenderer            *cell,
						 GdkDrawable                *window,
						 GtkWidget                  *widget,
						 GdkRectangle               *background_area,
						 GdkRectangle               *cell_area,
						 GdkRectangle               *expose_area,
						 GtkCellRendererState        flags);

static void fm_cell_renderer_pixbuf_get_property ( GObject *object,
                                      guint param_id,
                                      GValue *value,
                                      GParamSpec *pspec );

static void fm_cell_renderer_pixbuf_set_property ( GObject *object,
                                      guint param_id,
                                      const GValue *value,
                                      GParamSpec *pspec );

enum
{
    PROP_INFO = 1,
    N_PROPS
};

G_DEFINE_TYPE(FmCellRendererPixbuf, fm_cell_renderer_pixbuf, GTK_TYPE_CELL_RENDERER_PIXBUF);

static GdkPixbuf* link_icon = NULL;

/* GdkPixbuf RGBA C-Source image dump */
#ifdef __SUNPRO_C
#pragma align 4 (link_icon_data)
#endif
#ifdef __GNUC__
static const guint8 link_icon_data[] __attribute__ ((__aligned__ (4))) =
#else
static const guint8 link_icon_data[] =
#endif
    { ""
      /* Pixbuf magic (0x47646b50) */
      "GdkP"
      /* length: header (24) + pixel_data (400) */
      "\0\0\1\250"
      /* pixdata_type (0x1010002) */
      "\1\1\0\2"
      /* rowstride (40) */
      "\0\0\0("
      /* width (10) */
      "\0\0\0\12"
      /* height (10) */
      "\0\0\0\12"
      /* pixel_data: */
      "\200\200\200\377\200\200\200\377\200\200\200\377\200\200\200\377\200"
      "\200\200\377\200\200\200\377\200\200\200\377\200\200\200\377\200\200"
      "\200\377\0\0\0\377\200\200\200\377\377\377\377\377\377\377\377\377\377"
      "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
      "\377\377\377\377\377\377\0\0\0\377\200\200\200\377\377\377\377\377\0"
      "\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\377\377\377\377\377"
      "\377\377\377\0\0\0\377\200\200\200\377\377\377\377\377\0\0\0\377\0\0"
      "\0\377\0\0\0\377\0\0\0\377\377\377\377\377\377\377\377\377\377\377\377"
      "\377\0\0\0\377\200\200\200\377\377\377\377\377\0\0\0\377\0\0\0\377\0"
      "\0\0\377\0\0\0\377\0\0\0\377\377\377\377\377\377\377\377\377\0\0\0\377"
      "\200\200\200\377\377\377\377\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0"
      "\377\0\0\0\377\0\0\0\377\377\377\377\377\0\0\0\377\200\200\200\377\377"
      "\377\377\377\0\0\0\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0"
      "\0\377\0\0\0\377\377\377\377\377\0\0\0\377\200\200\200\377\377\377\377"
      "\377\377\377\377\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0\0"
      "\377\0\0\0\377\377\377\377\377\0\0\0\377\200\200\200\377\377\377\377"
      "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
      "\377\377\377\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0\0\377"
      "\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377"
      "\0\0\0\377\0\0\0\377"
    };


static void fm_cell_renderer_pixbuf_class_init(FmCellRendererPixbufClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(klass);

	g_object_class->finalize = fm_cell_renderer_pixbuf_finalize;
    g_object_class->get_property = fm_cell_renderer_pixbuf_get_property;
    g_object_class->set_property = fm_cell_renderer_pixbuf_set_property;

    cell_class->get_size = fm_cell_renderer_pixbuf_get_size;
    cell_class->render = fm_cell_renderer_pixbuf_render;


    g_object_class_install_property ( g_object_class,
                                      PROP_INFO,
                                      g_param_spec_pointer ( "info",
                                                             "File info",
                                                             "File info",
                                                             G_PARAM_READWRITE ) );
}


static void fm_cell_renderer_pixbuf_finalize(GObject *object)
{
	FmCellRendererPixbuf *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_CELL_RENDERER_PIXBUF(object));

	self = FM_CELL_RENDERER_PIXBUF(object);
    if( self->fi )
        fm_file_info_unref(self->fi);

	G_OBJECT_CLASS(fm_cell_renderer_pixbuf_parent_class)->finalize(object);
}


static void fm_cell_renderer_pixbuf_init(FmCellRendererPixbuf *self)
{
    if( !link_icon )
    {
        link_icon = gdk_pixbuf_new_from_inline(
                            sizeof(link_icon_data),
                            link_icon_data,
                            FALSE, NULL );
        g_object_add_weak_pointer((GObject*)link_icon, (gpointer)&link_icon);
    }
    else
        g_object_ref(link_icon);
}


GtkCellRenderer *fm_cell_renderer_pixbuf_new(void)
{
	return g_object_new(FM_TYPE_CELL_RENDERER_PIXBUF, NULL);
}

static void fm_cell_renderer_pixbuf_get_property ( GObject *object,
                                      guint param_id,
                                      GValue *value,
                                      GParamSpec *psec )
{
    FmCellRendererPixbuf* renderer = (FmCellRendererPixbuf*)object;
    switch( param_id )
    {
    case PROP_INFO:
        g_value_set_pointer(value, renderer->fi ? fm_file_info_ref(renderer->fi) : NULL);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID ( object, param_id, psec );
        break;
    }
}

static void fm_cell_renderer_pixbuf_set_property ( GObject *object,
                                      guint param_id,
                                      const GValue *value,
                                      GParamSpec *psec )
{
    FmCellRendererPixbuf* renderer = (FmCellRendererPixbuf*)object;
    switch ( param_id )
    {
    case PROP_INFO:
        if( renderer->fi )
            fm_file_info_unref(renderer->fi);
        renderer->fi = fm_file_info_ref((FmFileInfo*)g_value_get_pointer(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID ( object, param_id, psec );
        break;
    }
}

void fm_cell_renderer_pixbuf_set_fixed_size(FmCellRendererPixbuf* render, gint w, gint h)
{
    render->fixed_w = w;
    render->fixed_h = h;
}

void fm_cell_renderer_pixbuf_get_size   (GtkCellRenderer            *cell,
						 GtkWidget                  *widget,
						 GdkRectangle               *rectangle,
						 gint                       *x_offset,
						 gint                       *y_offset,
						 gint                       *width,
						 gint                       *height)
{
    FmCellRendererPixbuf* render = (FmCellRendererPixbuf*)cell;
    if(render->fixed_w > 0 && render->fixed_h > 0)
    {
        *width = render->fixed_w;
        *height = render->fixed_h;
    }
    else
    {
        GTK_CELL_RENDERER_CLASS(fm_cell_renderer_pixbuf_parent_class)->get_size(cell, widget, rectangle, x_offset, y_offset, width, height);
    }
}

void fm_cell_renderer_pixbuf_render     (GtkCellRenderer            *cell,
						 GdkDrawable                *window,
						 GtkWidget                  *widget,
						 GdkRectangle               *background_area,
						 GdkRectangle               *cell_area,
						 GdkRectangle               *expose_area,
						 GtkCellRendererState        flags)
{
    FmCellRendererPixbuf* render = (FmCellRendererPixbuf*)cell;
    /* we don't need to follow state for prelit items */
    if(flags & GTK_CELL_RENDERER_PRELIT)
        flags &= ~GTK_CELL_RENDERER_PRELIT;
    GTK_CELL_RENDERER_CLASS(fm_cell_renderer_pixbuf_parent_class)->render(cell, window, widget, background_area, cell_area, expose_area, flags);

    if(render->fi && G_UNLIKELY(fm_file_info_is_symlink(render->fi)))
    {
        GdkRectangle pix_rect;
        GdkPixbuf* pix;
        g_object_get(render, "pixbuf", &pix, NULL);
        if(pix)
        {
            int x = cell_area->x + (cell_area->width - gdk_pixbuf_get_width(pix))/2;
            int y = cell_area->y + (cell_area->height - gdk_pixbuf_get_height(pix))/2;

            gdk_draw_pixbuf ( GDK_DRAWABLE ( window ), NULL, link_icon, 0, 0,
                              x, y, -1, -1, GDK_RGB_DITHER_NORMAL, 0, 0 );
            g_object_unref(pix);
        }
    }
}
