/*
 *      fm-icon-pixbuf.c
 *      
 *      Copyright 2009 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include "fm-icon-pixbuf.h"

static gboolean init = FALSE;
static guint changed_handler = 0;

typedef struct _PixEntry
{
    int size;
    GdkPixbuf* pix;
}PixEntry;

static void destroy_pixbufs(GSList* pixs)
{
    GSList* l;
    gdk_threads_enter(); /* FIXME: is this needed? */
    for(l = pixs; l; l=l->next)
    {
        PixEntry* ent = (PixEntry*)l->data;
        if(G_LIKELY(ent->pix))
            g_object_unref(ent->pix);
        g_slice_free(PixEntry, ent);
    }
    gdk_threads_leave();
    g_slist_free(pixs);
}

GdkPixbuf* fm_icon_get_pixbuf(FmIcon* icon, int size)
{
    GtkIconInfo* ii;
    GdkPixbuf* pix;
    GSList *pixs, *l;
    PixEntry* ent;

    pixs = (GSList*)fm_icon_get_user_data(icon);
    for( l = pixs; l; l=l->next )
    {
        ent = (PixEntry*)l->data;
        if(ent->size == size) /* cached pixbuf is found! */
        {
            return ent->pix ? (GdkPixbuf*)g_object_ref(ent->pix) : NULL;
        }
    }

    /* not found! load the icon from disk */
    ii = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon->gicon, size, GTK_ICON_LOOKUP_FORCE_SIZE);
    if(ii)
    {
        pix = gtk_icon_info_load_icon(ii, NULL);
        gtk_icon_info_free(ii);

        /* increase ref_count to keep this pixbuf in memory
           even when no one is using it. */
        if(pix)
            g_object_ref(pix);
    }
    else
	{
		char* str = g_icon_to_string(icon->gicon);
		g_debug("unable to load icon %s", str);
		g_free(str);
        /* pix = NULL; */
		pix = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "unknown", 
					size, GTK_ICON_LOOKUP_USE_BUILTIN|GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        if(G_LIKELY(pix))
            g_object_ref(pix);
	}

    /* cache this! */
    ent = g_slice_new(PixEntry);
    ent->size = size;
    ent->pix = pix;

    /* FIXME: maybe we should unload icons that nobody is using to reduce memory usage. */
    /* g_object_weak_ref(); */
    pixs = g_slist_prepend(pixs, ent);
    fm_icon_set_user_data(icon, pixs);

    return pix;
}

static void on_icon_theme_changed(GtkIconTheme* theme, gpointer user_data)
{
    g_debug("icon theme changed!");
    /* unload pixbufs cached in FmIcon's hash table. */
    fm_icon_unload_user_data_cache();
}

void _fm_icon_pixbuf_init()
{
    /* FIXME: GtkIconTheme object is different on different GdkScreen */
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    changed_handler = g_signal_connect(theme, "changed", G_CALLBACK(on_icon_theme_changed), NULL);

    fm_icon_set_user_data_destroy( (GDestroyNotify)destroy_pixbufs );
}

void _fm_icon_pixbuf_finalize()
{
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    g_signal_handler_disconnect(theme, changed_handler);
}
