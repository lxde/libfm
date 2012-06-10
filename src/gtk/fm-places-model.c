/*
 *      fm-places-model.c
 *
 *      Copyright 2010 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-places-model.h"
#include <glib/gi18n-lib.h>

struct _FmPlacesItem
{
    FmPlacesType type : 6;
    gboolean mounted : 1; /* used if type == FM_PLACES_ITEM_VOLUME */
    FmIcon* icon;
    FmFileInfo* fi;
    union
    {
        GVolume* volume; /* used if type == FM_PLACES_ITEM_VOLUME */
        GMount* mount; /* used if type == FM_PLACES_ITEM_MOUNT */
        FmBookmarkItem* bm_item; /* used if type == FM_PLACES_ITEM_PATH */
    };
};

struct _FmPlacesModel
{
    GtkListStore parent;

    GVolumeMonitor* vol_mon;
    FmBookmarks* bookmarks;
    GtkTreeIter separator_iter;
    GtkTreePath* separator_tree_path;
    GtkTreeIter trash_iter;
    GFileMonitor* trash_monitor;
    guint trash_idle_handler;
    guint theme_change_handler;
    guint use_trash_change_handler;
    guint pane_icon_size_change_handler;
    GdkPixbuf* eject_icon;

    GSList* jobs;
};

static void create_trash_item(FmPlacesModel* model);

static void update_separator_tree_path(FmPlacesModel* model)
{
    gtk_tree_path_free(model->separator_tree_path);
    model->separator_tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &model->separator_iter);
}

static void place_item_free(FmPlacesItem* item)
{
    switch(item->type)
    {
    case FM_PLACES_ITEM_VOLUME:
        g_object_unref(item->volume);
        break;
        break;
    }
    if(G_LIKELY(item->icon))
        fm_icon_unref(item->icon);
    if(G_LIKELY(item->fi))
        fm_file_info_unref(item->fi);
    g_slice_free(FmPlacesItem, item);
}

static void on_file_info_job_finished(FmFileInfoJob* job, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    GList* l;
    GtkTreeIter it;
    FmPlacesItem* item;
    FmFileInfo* fi;
    FmPath* path;

    /* g_debug("file info job finished"); */
    model->jobs = g_slist_remove(model->jobs, job);

    if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it))
        goto finished;

    if(fm_list_is_empty(job->file_infos))
        goto finished;

    /* optimize for one file case */
    if(fm_list_get_length(job->file_infos) == 1)
    {
        fi = FM_FILE_INFO(fm_list_peek_head(job->file_infos));
        do {
            item = NULL;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if( item && item->fi && (path = fm_file_info_get_path(item->fi)) && fm_path_equal(path, fm_file_info_get_path(fi)) )
            {
                fm_file_info_unref(item->fi);
                item->fi = fm_file_info_ref(fi);
                break;
            }
        }while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it));
    }
    else
    {
        do {
            item = NULL;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if( item && item->fi && (path = fm_file_info_get_path(item->fi)) )
            {
                for(l = fm_list_peek_head_link(job->file_infos); l; l = l->next )
                {
                    fi = FM_FILE_INFO(l->data);
                    if(fm_path_equal(path, fm_file_info_get_path(fi)))
                    {
                        fm_file_info_unref(item->fi);
                        item->fi = fm_file_info_ref(fi);
                        /* remove the file from list to speed up further loading.
                      * This won't cause problem since nobody else if using the list. */
                        fm_list_delete_link(job->file_infos, l);
                        break;
                    }
                }
            }
        }while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it));
    }
finished:
    g_object_unref(job);
}

static void update_volume_or_mount(FmPlacesModel* model, FmPlacesItem* item, GtkTreeIter* it, FmFileInfoJob* job)
{
    GIcon* gicon;
    char* name;
    GdkPixbuf* pix;
    GMount* mount;
    FmPath* path;

    if(item->icon)
        fm_icon_unref(item->icon);

    if(item->type == FM_PLACES_ITEM_VOLUME)
    {
        name = g_volume_get_name(item->volume);
        gicon = g_volume_get_icon(item->volume);
        mount = g_volume_get_mount(item->volume);
    }
    else if(item->type == FM_PLACES_ITEM_MOUNT)
    {
        name = g_mount_get_name(item->mount);
        gicon = g_mount_get_icon(item->mount);
        mount = g_object_ref(item->mount);
    }
    item->icon = fm_icon_from_gicon(gicon);
    g_object_unref(gicon);

    if(mount)
    {
        GFile* gf = g_mount_get_root(mount);
        path = fm_path_new_for_gfile(gf);
        g_object_unref(gf);
        g_object_unref(mount);
        item->mounted = TRUE;
    }
    else
    {
        path = NULL;
        item->mounted = FALSE;
    }

    if(!fm_path_equal(fm_file_info_get_path(item->fi), path))
    {
        fm_file_info_set_path(item->fi, path);
        if(path)
        {
            if(job)
                fm_file_info_job_add(job, path);
            else
            {
                job = fm_file_info_job_new(NULL, FM_FILE_INFO_JOB_FOLLOW_SYMLINK);
                model->jobs = g_slist_prepend(model->jobs, job);
                g_signal_connect(job, "finished", G_CALLBACK(on_file_info_job_finished), model);
                fm_job_run_async(FM_JOB(job));
            }
            fm_path_unref(path);
        }
    }

    pix = fm_icon_get_pixbuf(item->icon, fm_config->pane_icon_size);
    gtk_list_store_set(GTK_LIST_STORE(model), it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, name, -1);
    g_object_unref(pix);
    g_free(name);
}

static void add_volume_or_mount(FmPlacesModel* model, GObject* volume_or_mount, FmFileInfoJob* job)
{
    GtkTreeIter it;
    FmPlacesItem* item;
    item = g_slice_new0(FmPlacesItem);
    item->fi = fm_file_info_new();
    if(G_IS_VOLUME(volume_or_mount))
    {
        item->type = FM_PLACES_ITEM_VOLUME;
        item->volume = G_VOLUME(g_object_ref(volume_or_mount));
    }
    else if(G_IS_MOUNT(volume_or_mount))
    {
        item->type = FM_PLACES_ITEM_MOUNT;
        item->mount = G_MOUNT(g_object_ref(volume_or_mount));
    }
    else
    {
        /* NOTE: this is impossible, unless a bug exists */
    }
    gtk_list_store_insert_before(GTK_LIST_STORE(model), &it, &model->separator_iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &it, FM_PLACES_MODEL_COL_INFO, item, -1);
    update_volume_or_mount(model, item, &it, job);
}

static FmPlacesItem* find_volume(FmPlacesModel* model, GVolume* volume, GtkTreeIter* _it)
{
    GtkTreeIter it;
    /* FIXME: don't need to find from the first iter */
    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it))
    {
        do
        {
            FmPlacesItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if(item && item->type == FM_PLACES_ITEM_VOLUME && item->volume == volume)
            {
                *_it = it;
                return item;
            }
        }while(it.user_data != model->separator_iter.user_data && gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it));
    }
    return NULL;
}

static FmPlacesItem* find_mount(FmPlacesModel* model, GMount* mount, GtkTreeIter* _it)
{
    GtkTreeIter it;
    /* FIXME: don't need to find from the first iter */
    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it))
    {
        do
        {
            FmPlacesItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if(item && item->type == FM_PLACES_ITEM_MOUNT && item->mount == mount)
            {
                *_it = it;
                return item;
            }
        }while(it.user_data != model->separator_iter.user_data && gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it));
    }
    return NULL;
}

static void on_volume_added(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    /* g_debug("add vol: %p, uuid: %s, udi: %s", vol, g_volume_get_identifier(vol, "uuid"), g_volume_get_identifier(vol, "hal-udi")); */
    add_volume_or_mount(model, G_OBJECT(vol), NULL);
    update_separator_tree_path(model);
}

static void on_volume_removed(GVolumeMonitor* vm, GVolume* volume, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    FmPlacesItem* item;
    GtkTreeIter it;
    item = find_volume(model, volume, &it);
    if(item)
    {
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);
        place_item_free(item);
        update_separator_tree_path(model);
    }
}

static void on_volume_changed(GVolumeMonitor* vm, GVolume* volume, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    FmPlacesItem* item;
    GtkTreeIter it;
    /* g_debug("vol-changed"); */
    item = find_volume(model, volume, &it);
    if(item)
        update_volume_or_mount(model, item, &it, NULL);
}

static void on_mount_added(GVolumeMonitor* vm, GMount* mount, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    GVolume* vol = g_mount_get_volume(mount);
    if(vol)
    {
        FmPlacesItem *item;
        GtkTreeIter it;
        item = find_volume(model, vol, &it);
        if(item && item->type == FM_PLACES_ITEM_VOLUME && !fm_file_info_get_path(item->fi))
        {
            GtkTreePath* tp;
            GFile* gf = g_mount_get_root(mount);
            FmPath* path = fm_path_new_for_gfile(gf);
            /* g_debug("mount path: %s", path->name); */
            g_object_unref(gf);
            fm_file_info_set_path(item->fi, path);
            if(path)
                fm_path_unref(path);
            item->mounted = TRUE;

            /* inform the view to update mount indicator */
            tp = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &it);
            gtk_tree_model_row_changed(GTK_TREE_MODEL(model), tp, &it);
            gtk_tree_path_free(tp);
        }
        g_object_unref(vol);
    }
    else /* network mounts and others */
    {
        add_volume_or_mount(model, G_OBJECT(mount), NULL);
    }
}

static void on_mount_changed(GVolumeMonitor* vm, GMount* mount, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    FmPlacesItem* item;
    GtkTreeIter it;
    item = find_mount(model, mount, &it);
    if(item)
        update_volume_or_mount(model, item, &it, NULL);
}

static void on_mount_removed(GVolumeMonitor* vm, GMount* mount, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    GVolume* vol = g_mount_get_volume(mount);
    if(vol) /* we handle volumes in volume-removed handler */
        g_object_unref(vol);
    else /* network mounts and others */
    {
        GtkTreeIter it;        
        FmPlacesItem* item = find_mount(model, mount, &it);
        if(item)
        {
            gtk_list_store_remove(GTK_LIST_STORE(model), &it);
            place_item_free(item);
            update_separator_tree_path(model);
        }
    }
}

static void add_bookmarks(FmPlacesModel* model, FmFileInfoJob* job)
{
    FmPlaceItem* item;
    const GList *bms, *l;
    FmIcon* icon = fm_icon_from_name("folder");
    FmIcon* remote_icon = NULL;
    GdkPixbuf* folder_pix = fm_icon_get_pixbuf(icon, fm_config->pane_icon_size);
    GdkPixbuf* remote_pix = NULL;
    bms = fm_bookmarks_list_all(model->bookmarks);
    for(l=bms;l;l=l->next)
    {
        FmBookmarkItem* bm = (FmBookmarkItem*)l->data;
        GtkTreeIter it;
        GdkPixbuf* pix;
        FmPath* path = bm->path;
        item = g_slice_new0(FmPlacesItem);
        item->type = FM_PLACES_ITEM_PATH;
        item->fi = fm_file_info_new();
        fm_file_info_set_path(item->fi, path);
        fm_file_info_job_add(job, path);

        if(fm_path_is_native(path))
        {
            item->icon = fm_icon_ref(icon);
            pix = folder_pix;
        }
        else
        {
            if(G_UNLIKELY(!remote_icon))
            {
                remote_icon = fm_icon_from_name("folder-remote");
                remote_pix = fm_icon_get_pixbuf(remote_icon, fm_config->pane_icon_size);
            }
            item->icon = fm_icon_ref(remote_icon);
            pix = remote_pix;
        }
        item->bm_item = bm;
        gtk_list_store_append(GTK_LIST_STORE(model), &it);
        gtk_list_store_set(GTK_LIST_STORE(model), &it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, bm->name, FM_PLACES_MODEL_COL_INFO, item, -1);
    }
    g_object_unref(folder_pix);
    fm_icon_unref(icon);
    if(remote_icon)
    {
        fm_icon_unref(remote_icon);
        if(remote_pix)
            g_object_unref(remote_pix);
    }
}

static void on_bookmarks_changed(FmBookmarks* bm, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    FmFileInfoJob* job = fm_file_info_job_new(NULL, FM_FILE_INFO_JOB_FOLLOW_SYMLINK);
    GtkTreeIter it = model->separator_iter;
    /* remove all old bookmarks */
    if(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it))
    {
        while(gtk_list_store_remove(GTK_LIST_STORE(model), &it))
            continue;
    }
    add_bookmarks(model, job);

    g_signal_connect(job, "finished", G_CALLBACK(on_file_info_job_finished), model);
    model->jobs = g_slist_prepend(model->jobs, job);
    fm_job_run_async(FM_JOB(job));
}

static gboolean update_trash_item(gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    if(fm_config->use_trash)
    {
        GFile* gf = g_file_new_for_uri("trash:///");
        GFileInfo* inf = g_file_query_info(gf, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT, 0, NULL, NULL);
        g_object_unref(gf);
        if(inf)
        {
            FmIcon* icon;
            const char* icon_name;
            FmPlacesItem* item;
            GdkPixbuf* pix;
            guint32 n = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
            g_object_unref(inf);
            icon_name = n > 0 ? "user-trash-full" : "user-trash";
            icon = fm_icon_from_name(icon_name);
            gtk_tree_model_get(GTK_TREE_MODEL(model), &model->trash_iter, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if(item->icon)
                fm_icon_unref(item->icon);
            item->icon = icon;
            /* update the icon */
            pix = fm_icon_get_pixbuf(item->icon, fm_config->pane_icon_size);
            gtk_list_store_set(GTK_LIST_STORE(model), &model->trash_iter, FM_PLACES_MODEL_COL_ICON, pix, -1);
            g_object_unref(pix);
        }
    }
    return FALSE;
}


static void on_trash_changed(GFileMonitor *monitor, GFile *gf, GFile *other, GFileMonitorEvent evt, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    if(model->trash_idle_handler)
        g_source_remove(model->trash_idle_handler);
    model->trash_idle_handler = g_idle_add(update_trash_item, model);
}

static void update_icons(FmPlacesModel* model)
{
    GtkTreeIter it;
    FmIcon* icon;
    GdkPixbuf* pix;

    /* update the eject icon */
    icon = fm_icon_from_name("media-eject");
    pix = fm_icon_get_pixbuf(icon, fm_config->pane_icon_size);
    fm_icon_unref(icon);
    if(model->eject_icon)
        g_object_unref(model->eject_icon);
    model->eject_icon = pix;

    /* reload icon for every item */
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it);
    do{
        if(it.user_data != model->separator_iter.user_data)
        {
            FmPlacesItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
			if(item)
			{
				pix = fm_icon_get_pixbuf(item->icon, fm_config->pane_icon_size);
				gtk_list_store_set(GTK_LIST_STORE(model), &it, FM_PLACES_MODEL_COL_ICON, pix, -1);
				g_object_unref(pix);
			}
			else
			{
				/* #3497049 - PCManFM crashed with SIGSEV in update_icons().
				 * Item should not be null. Otherwise there must be a bug. */
				g_debug("bug? FmPlacesItem* item should not be null.");
			}
        }
    }while( gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it) );
}

static void on_use_trash_changed(FmConfig* cfg, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    if(cfg->use_trash && model->trash_iter.user_data == NULL)
        create_trash_item(model);
    else
    {
        FmPlacesItem *item;
        gtk_tree_model_get(GTK_TREE_MODEL(model), &model->trash_iter, FM_PLACES_MODEL_COL_INFO, &item, -1);
        gtk_list_store_remove(GTK_LIST_STORE(model), &model->trash_iter);
        model->trash_iter.user_data = NULL;
        place_item_free(item);

        if(model->trash_monitor)
        {
            g_signal_handlers_disconnect_by_func(model->trash_monitor, on_trash_changed, model);
            g_object_unref(model->trash_monitor);
            model->trash_monitor = NULL;
        }
        if(model->trash_idle_handler)
        {
            g_source_remove(model->trash_idle_handler);
            model->trash_idle_handler = 0;
        }
    }
    update_separator_tree_path(model);
}

static void on_pane_icon_size_changed(FmConfig* cfg, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    update_icons(model);
}


static void create_trash_item(FmPlacesModel* model)
{
    GtkTreeIter it;
    FmPlacesItem* item;
    GdkPixbuf* pix;
    GFile* gf;

    gf = g_file_new_for_uri("trash:///");
    model->trash_monitor = fm_monitor_directory(gf, NULL);
    g_signal_connect(model->trash_monitor, "changed", G_CALLBACK(on_trash_changed), model);
    g_object_unref(gf);

    item = g_slice_new0(FmPlacesItem);
    item->type = FM_PLACES_ITEM_PATH;
    item->fi = fm_file_info_new();
    fm_file_info_set_path(item->fi, fm_path_get_trash());
    item->icon = fm_icon_from_name("user-trash");
    gtk_list_store_insert(GTK_LIST_STORE(model), &it, 2);
    pix = fm_icon_get_pixbuf(item->icon, fm_config->pane_icon_size);
    gtk_list_store_set(GTK_LIST_STORE(model), &it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, _("Trash"), FM_PLACES_MODEL_COL_INFO, item, -1);
    g_object_unref(pix);
    model->trash_iter = it;

    if(0 == model->trash_idle_handler)
        model->trash_idle_handler = g_idle_add(update_trash_item, model);
}

static void fm_places_model_init(FmPlacesModel *self)
{
    GType types[] = {GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER};
    GtkTreeIter it;
    FmPlacesItem* item;
    GList *vols, *l;
    FmIcon* icon;
    GdkPixbuf* pix;
    FmFileInfoJob* job = fm_file_info_job_new(NULL, FM_FILE_INFO_JOB_FOLLOW_SYMLINK);
    GtkListStore* model = &self->parent;
    FmPath *path;

    gtk_list_store_set_column_types(&self->parent, FM_PLACES_MODEL_N_COLS, types);

    self->theme_change_handler = g_signal_connect_swapped(gtk_icon_theme_get_default(), "changed",
                                            G_CALLBACK(update_icons), self);

    self->use_trash_change_handler = g_signal_connect(fm_config, "changed::use_trash",
                                             G_CALLBACK(on_use_trash_changed), self);

    self->pane_icon_size_change_handler = g_signal_connect(fm_config, "changed::pane_icon_size",
                                             G_CALLBACK(on_pane_icon_size_changed), self);
    icon = fm_icon_from_name("media-eject");
    pix = fm_icon_get_pixbuf(icon, fm_config->pane_icon_size);
    fm_icon_unref(icon);
    self->eject_icon = pix;

    item = g_slice_new0(FmPlacesItem);
    item->type = FM_PLACES_ITEM_PATH;
    item->fi = fm_file_info_new();
    path = fm_path_get_home();
    fm_file_info_set_path(item->fi, path);
    item->icon = fm_icon_from_name("user-home");
    gtk_list_store_append(model, &it);
    pix = fm_icon_get_pixbuf(item->icon, fm_config->pane_icon_size);
    gtk_list_store_set(model, &it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, path->name, FM_PLACES_MODEL_COL_INFO, item, -1);
    g_object_unref(pix);
    fm_file_info_job_add(job, path);

    /* Only show desktop in side pane when the user has a desktop dir. */
    if(g_file_test(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP), G_FILE_TEST_IS_DIR))
    {
        item = g_slice_new0(FmPlacesItem);
        item->type = FM_PLACES_ITEM_PATH;
        item->fi = fm_file_info_new();
        fm_file_info_set_path(item->fi, fm_path_get_desktop());
        item->icon = fm_icon_from_name("user-desktop");
        gtk_list_store_append(model, &it);
        pix = fm_icon_get_pixbuf(item->icon, fm_config->pane_icon_size);
        gtk_list_store_set(model, &it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, _("Desktop"), FM_PLACES_MODEL_COL_INFO, item, -1);
        g_object_unref(pix);
        fm_file_info_job_add(job, path);
    }

    if(fm_config->use_trash)
        create_trash_item(self); /* FIXME: how to handle trash can? */

    item = g_slice_new0(FmPlacesItem);
    item->type = FM_PLACES_ITEM_PATH;
    item->fi = fm_file_info_new();
    fm_file_info_set_path(item->fi, fm_path_get_apps_menu());
    item->icon = fm_icon_from_name("system-software-install");
    gtk_list_store_append(model, &it);
    pix = fm_icon_get_pixbuf(item->icon, fm_config->pane_icon_size);
    gtk_list_store_set(model, &it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, _("Applications"), FM_PLACES_MODEL_COL_INFO, item, -1);
    g_object_unref(pix);
    /* fm_file_info_job_add(job, item->fi->path); */

    /* volumes */
    self->vol_mon = g_volume_monitor_get();
    g_signal_connect(self->vol_mon, "volume-added", G_CALLBACK(on_volume_added), self);
    g_signal_connect(self->vol_mon, "volume-removed", G_CALLBACK(on_volume_removed), self);
    g_signal_connect(self->vol_mon, "volume-changed", G_CALLBACK(on_volume_changed), self);
    g_signal_connect(self->vol_mon, "mount-added", G_CALLBACK(on_mount_added), self);
    g_signal_connect(self->vol_mon, "mount-changed", G_CALLBACK(on_mount_changed), self);
    g_signal_connect(self->vol_mon, "mount-removed", G_CALLBACK(on_mount_removed), self);

    /* separator */
    gtk_list_store_append(model, &self->separator_iter);

    /* add volumes to side-pane */
    vols = g_volume_monitor_get_volumes(self->vol_mon);
    for(l=vols;l;l=l->next)
    {
        GVolume* vol = G_VOLUME(l->data);
        add_volume_or_mount(self, vol, job);
        g_object_unref(vol);
    }
    g_list_free(vols);

    /* add mounts to side-pane */
    vols = g_volume_monitor_get_mounts(self->vol_mon);
    for(l=vols;l;l=l->next)
    {
        GMount* mount = G_MOUNT(l->data);
        GVolume* volume = g_mount_get_volume(mount);
        if(volume)
            g_object_unref(volume);
        else /* network mounts or others */
            add_volume_or_mount(self, mount, job);
        g_object_unref(mount);
    }
    g_list_free(vols);

    /* get the path of separator */
    self->separator_tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(self), &self->separator_iter);

    self->bookmarks = fm_bookmarks_dup(); /* bookmarks */
    g_signal_connect(self->bookmarks, "changed", G_CALLBACK(on_bookmarks_changed), self);

    /* add bookmarks to side pane */
    add_bookmarks(self, job);

    g_signal_connect(job, "finished", G_CALLBACK(on_file_info_job_finished), self);
    self->jobs = g_slist_prepend(self->jobs, job);
    fm_job_run_async(FM_JOB(job));
}

GtkTreePath* fm_places_model_get_separator_path(FmPlacesModel* model)
{
    return model->separator_tree_path;
}

FmBookmarks* fm_places_model_get_bookmarks(FmPlacesModel* model)
{
    return model->bookmarks;
}

gboolean fm_places_model_iter_is_separator(FmPlacesModel* model, GtkTreeIter* it)
{
    return it && it->user_data == model->separator_iter.user_data;
}

gboolean fm_places_model_path_is_separator(FmPlacesModel* model, GtkTreePath* tp)
{
    return tp && gtk_tree_path_compare(model->separator_tree_path, tp) == 0;
}

gboolean fm_places_model_path_is_bookmark(FmPlacesModel* model, GtkTreePath* tp)
{
    return tp && gtk_tree_path_compare(model->separator_tree_path, tp) < 0;
}

gboolean fm_places_model_path_is_places(FmPlacesModel* model, GtkTreePath* tp)
{
    return tp && gtk_tree_path_compare(model->separator_tree_path, tp) > 0;
}

static gboolean row_draggable(GtkTreeDragSource* drag_source, GtkTreePath* tp)
{
    FmPlacesModel* model = FM_PLACES_MODEL(drag_source);
    return fm_places_model_path_is_bookmark(model, tp);
}

static void fm_places_model_drag_source_init(GtkTreeDragSourceIface *iface)
{
    iface->row_draggable = row_draggable;
}

G_DEFINE_TYPE_WITH_CODE (FmPlacesModel, fm_places_model, GTK_TYPE_LIST_STORE,
             G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                        fm_places_model_drag_source_init))

static void fm_places_model_dispose(GObject *object)
{
    FmPlacesModel *self = FM_PLACES_MODEL(object);
    GtkTreeIter it;

    if(self->jobs)
    {
        GSList* l;
        for(l = self->jobs; l; l=l->next)
        {
            fm_job_cancel(FM_JOB(l->data));
            g_object_unref(l->data);
        }
        g_slist_free(self->jobs);
        self->jobs = NULL;
    }

    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(self), &it))
    {
        do
        {
            FmPlacesItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(self), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if(G_LIKELY(item))
                place_item_free(item);
        }while(gtk_tree_model_iter_next(GTK_TREE_MODEL(self), &it));
    }

    if(self->separator_tree_path)
    {
        gtk_tree_path_free(self->separator_tree_path);
        self->separator_tree_path = NULL;
    }

    if(self->theme_change_handler)
    {
        g_signal_handler_disconnect(gtk_icon_theme_get_default(), self->theme_change_handler);
        self->theme_change_handler = 0;
        g_signal_handler_disconnect(fm_config, self->use_trash_change_handler);
        self->use_trash_change_handler = 0;
        g_signal_handler_disconnect(fm_config, self->pane_icon_size_change_handler);
        self->pane_icon_size_change_handler = 0;
    }

    if(self->vol_mon)
    {
        g_signal_handlers_disconnect_by_func(self->vol_mon, on_volume_added, self);
        g_signal_handlers_disconnect_by_func(self->vol_mon, on_volume_removed, self);
        g_signal_handlers_disconnect_by_func(self->vol_mon, on_volume_changed, self);
        g_signal_handlers_disconnect_by_func(self->vol_mon, on_mount_added, self);
        g_signal_handlers_disconnect_by_func(self->vol_mon, on_mount_changed, self);
        g_signal_handlers_disconnect_by_func(self->vol_mon, on_mount_removed, self);
        g_object_unref(self->vol_mon);
        self->vol_mon = NULL;
    }

    if(self->bookmarks)
    {
        g_signal_handlers_disconnect_by_func(self->bookmarks, on_bookmarks_changed, self);
        g_object_unref(self->bookmarks);
        self->bookmarks = NULL;
    }

    if(self->trash_monitor)
    {
        g_signal_handlers_disconnect_by_func(self->trash_monitor, on_trash_changed, self);
        g_object_unref(self->trash_monitor);
        self->trash_monitor = NULL;
    }

    if(self->trash_idle_handler)
    {
        g_source_remove(self->trash_idle_handler);
        self->trash_idle_handler = 0;
    }

    G_OBJECT_CLASS(fm_places_model_parent_class)->dispose(object);
}

static void fm_places_model_finalize(GObject *object)
{
    FmPlacesModel *self = FM_PLACES_MODEL(object);
    G_OBJECT_CLASS(fm_places_model_parent_class)->finalize(object);
}

static void fm_places_model_class_init(FmPlacesModelClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->dispose = fm_places_model_dispose;
    g_object_class->finalize = fm_places_model_finalize;
}


FmPlacesModel *fm_places_model_new(void)
{
    return g_object_new(FM_TYPE_PLACES_MODEL, NULL);
}

void fm_places_model_mount_indicator_cell_data_func(GtkCellLayout *cell_layout,
                                           GtkCellRenderer *render,
                                           GtkTreeModel *tree_model,
                                           GtkTreeIter *it,
                                           gpointer user_data)
{
    FmPlacesItem* item;
    GdkPixbuf* pix = NULL;
    gtk_tree_model_get(tree_model, it, FM_PLACES_MODEL_COL_INFO, &item, -1);
    if(item && item->mounted)
        pix = FM_PLACES_MODEL(tree_model)->eject_icon;
    g_object_set(render, "pixbuf", pix, NULL);
}

gboolean fm_places_model_get_iter_by_fm_path(FmPlacesModel* model, GtkTreeIter* iter, FmPath* path)
{
    GtkTreeIter it;
    GtkTreeModel* model_ = GTK_TREE_MODEL(model);
    if(gtk_tree_model_get_iter_first(model_, &it))
    {
        FmPlacesItem* item;
        do{
            item = NULL;
            gtk_tree_model_get(model_, &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if(item && item->fi && fm_path_equal(fm_file_info_get_path(item->fi), path))
            {
                *iter = it;
                return TRUE;
            }
        }while(gtk_tree_model_iter_next(model_, &it));
    }
    return FALSE;
}


FmPlacesType fm_places_item_get_type(FmPlacesItem* item)
{
    return item->type;
}

gboolean fm_places_item_is_mounted(FmPlacesItem* item)
{
    return item->mounted ? TRUE : FALSE;
}

FmIcon* fm_places_item_get_icon(FmPlacesItem* item)
{
    return item->icon;
}

FmFileInfo* fm_places_item_get_info(FmPlacesItem* item)
{
    return item->fi;
}

GVolume* fm_places_item_get_volume(FmPlacesItem* item)
{
    return item->type == FM_PLACES_ITEM_VOLUME ? item->volume : NULL;
}

GMount* fm_places_item_get_mount(FmPlacesItem* item)
{
    return item->type == FM_PLACES_ITEM_MOUNT ? item->mount : NULL;
}

FmPath* fm_places_item_get_path(FmPlacesItem* item)
{
    return item->fi ? fm_file_info_get_path(item->fi) : NULL;
}

FmBookmarkItem* fm_places_item_get_bookmark_item(FmPlacesItem* item)
{
    return item->type == FM_PLACES_ITEM_PATH ? item->bm_item : NULL;
}
