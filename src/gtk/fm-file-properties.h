/*
 *      fm-file-properties.h
 *
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
 *      Copyright 2013 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

#ifndef __FM_FILE_PROPERTIES_H__
#define __FM_FILE_PROPERTIES_H__

#include <gtk/gtk.h>
#include "fm-file-info.h"

G_BEGIN_DECLS

/* FIXME: use list of FmFileInfo is much better here. */
GtkDialog* fm_file_properties_widget_new(FmFileInfoList* files, gboolean toplevel);
gboolean fm_show_file_properties(GtkWindow* parent, FmFileInfoList* files);

typedef struct _FmFilePropertiesExtensionInit   FmFilePropertiesExtensionInit;

/**
 * FmFilePropertiesExtensionInit:
 * @init: callback to make ui fields specific to file type
 * @finish: callback to optionally apply changes
 *
 * The structure describing callbacks for File Properties dialog extension
 * specific for some file type.
 *
 * The @init callback is called before dialog window is opened. Callback
 * can disable or enable elements in dialog window, add triggers on those
 * elements, append new elements, etc. Callback gets three arguments: the
 * actual UI from builder, internal widget data pointer, and file infos
 * list. Internal widget data pointer may be used to block widget builtin
 * signal handler (that may be required if extension has own handler for
 * some GtkEntry element). Callback should return some own internal data
 * pointer which will be used as argument for @finish callback later.
 *
 * The @finish callback is called before dialog window is closed. It gets
 * two arguments: the data pointer that was returned by @init callback
 * before and boolean value indicating if dialog was closed not by "OK"
 * button. Callback should free any resources allocated by @init callback
 * before.
 */
struct _FmFilePropertiesExtensionInit
{
    gpointer (*init)(GtkBuilder *ui, gpointer uidata, FmFileInfoList *files);
    void (*finish)(gpointer data, gboolean cancelled);
};

gboolean fm_file_properties_add_for_mime_type(const char *mime_type,
                                              FmFilePropertiesExtensionInit *callbacks);

G_END_DECLS

#endif
