/*
 *      fm-folder.h
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


#ifndef __FM_FOLDER_H__
#define __FM_FOLDER_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "fm-path.h"
#include "fm-dir-list-job.h"
#include "fm-file-info.h"
#include "fm-job.h"
#include "fm-file-info-job.h"

G_BEGIN_DECLS

#define FM_TYPE_FOLDER              (fm_folder_get_type())
#define FM_FOLDER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_TYPE_FOLDER, FmFolder))
#define FM_FOLDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_TYPE_FOLDER, FmFolderClass))
#define FM_IS_FOLDER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_TYPE_FOLDER))
#define FM_IS_FOLDER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_TYPE_FOLDER))

typedef struct _FmFolder            FmFolder;
typedef struct _FmFolderClass       FmFolderClass;

struct _FmFolder
{
    GObject parent;

    /* private */
    FmPath* dir_path;
    GFile* gf;
    GFileMonitor* mon;
    FmDirListJob* job;
    FmFileInfo* dir_fi;
    FmFileInfoList* files;

    /* for file monitor */
    guint idle_handler;
    GSList* files_to_add;
    GSList* files_to_update;
    GSList* files_to_del;
    GSList* pending_jobs;

    /* filesystem info */
    guint64 fs_total_size;
    guint64 fs_free_size;
    GCancellable* fs_size_cancellable;
    gboolean has_fs_info : 1;
    gboolean fs_info_not_avail : 1;
};

struct _FmFolderClass
{
    GObjectClass parent_class;

    void (*files_added)(FmFolder* dir, GSList* files);
    void (*files_removed)(FmFolder* dir, GSList* files);
    void (*files_changed)(FmFolder* dir, GSList* files);
    void (*loaded)(FmFolder* dir);
    void (*unmount)(FmFolder* dir);
    void (*changed)(FmFolder* dir);
    void (*removed)(FmFolder* dir);
    void (*content_changed)(FmFolder* dir);
    void (*fs_info)(FmFolder* dir);
    FmJobErrorAction (*error)(FmFolder* dir, GError* err, FmJobErrorSeverity severity);
};

GType       fm_folder_get_type      (void);
FmFolder*   fm_folder_get(FmPath* path);
FmFolder*   fm_folder_get_for_gfile(GFile* gf);
FmFolder*   fm_folder_get_for_path_name(const char* path);
FmFolder*   fm_folder_get_for_uri(const char* uri);

FmFileInfoList* fm_folder_get_files (FmFolder* folder);
FmFileInfo* fm_folder_get_file_by_name(FmFolder* folder, const char* name);

gboolean fm_folder_get_is_loaded(FmFolder* folder);

void fm_folder_reload(FmFolder* folder);

gboolean fm_folder_get_filesystem_info(FmFolder* folder, guint64* total_size, guint64* free_size);
void fm_folder_query_filesystem_info(FmFolder* folder);

G_END_DECLS

#endif /* __FM_FOLDER_H__ */
