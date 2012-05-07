//      fm-thumbnailer.h
//      
//      Copyright 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
//      
//      


#ifndef __FM_THUMBNAILER_H__
#define __FM_THUMBNAILER_H__

#include <glib.h>

G_BEGIN_DECLS

#define	FM_THUMBNAILER(p)	((FmThumbnailer*)p)

typedef struct _FmThumbnailer	FmThumbnailer;

struct _FmThumbnailer
{
	char* id;
	char* try_exec; /* FIXME: is this useful? */
	char* exec;
	GList* mime_types;
};

FmThumbnailer* fm_thumbnailer_new_from_keyfile(const char* id, GKeyFile* kf);
void fm_thumbnailer_free(FmThumbnailer* thumbnailer);

gboolean fm_thumbnailer_launch_for_uri(FmThumbnailer* thumbnailer, const char* uri, const char* output_file, guint size);

/* reload the thumbnailers if needed */
void fm_thumbnailer_check_update();

void _fm_thumbnailer_init();
void _fm_thumbnailer_finalize();

G_END_DECLS

#endif /* __FM_THUMBNAILER_H__ */
