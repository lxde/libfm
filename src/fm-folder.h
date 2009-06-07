/*
 *      fm-folder.h
 *      
 *      Copyright 2009 PCMan <pcman@thinkpad>
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


#ifndef __FM_FOLDER_H__
#define __FM_FOLDER_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "fm-dir-list-job.h"
#include "fm-file-info.h"

G_BEGIN_DECLS

#define FM_TYPE_FOLDER				(fm_folder_get_type())
#define FM_FOLDER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_TYPE_FOLDER, FmFolder))
#define FM_FOLDER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_TYPE_FOLDER, FmFolderClass))
#define FM_IS_FOLDER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_TYPE_FOLDER))
#define FM_IS_FOLDER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_TYPE_FOLDER))

typedef struct _FmFolder			FmFolder;
typedef struct _FmFolderClass		FmFolderClass;

struct _FmFolder
{
	GObject parent;

	/* private */
	GFile* gf;
	GFileMonitor* mon;
	FmDirListJob* job;
	GList* files;
};

struct _FmFolderClass
{
	GObjectClass parent_class;

    void (*files_added)(FmFolder* dir, GSList* files);
    void (*files_removed)(FmFolder* dir, GSList* files);
    void (*files_changed)(FmFolder* dir, GSList* files);
};

GType		fm_folder_get_type		(void);
FmFolder*	fm_folder_new			(GFile* gf);
FmFolder*	fm_folder_new_for_path	(const char* path);
FmFolder*	fm_folder_new_for_uri	(const char* uri);

void fm_folder_reload(FmFolder* folder);

G_END_DECLS

#endif /* __FM-FOLDER_H__ */
