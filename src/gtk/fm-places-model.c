/*
 *      fm-places-model.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-places-model.h"
#include <glib/gi18n-lib.h>

static void create_trash_item(FmPlacesModel* model);

static void update_sep_tp(FmPlacesModel* model)
{
    gtk_tree_path_free(model->sep_tp);
    model->sep_tp = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &model->sep_it);
}

static void place_item_free(FmPlaceItem* item)
{
    switch(item->type)
    {
    case FM_PLACES_ITEM_VOL:
        g_object_unref(item->vol);
        break;
    }
    fm_file_info_unref(item->fi);
    g_slice_free(FmPlaceItem, item);
}

static void on_file_info_job_finished(FmFileInfoJob* job, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    GList* l;
    GtkTreeIter it;
    FmPlaceItem* item;
    FmFileInfo* fi;

    /* g_debug("file info job finished"); */
    model->jobs = g_slist_remove(model->jobs, job);

    if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it))
        return;

    if(fm_list_is_empty(job->file_infos))
        return;

    /* optimize for one file case */
    if(fm_list_get_length(job->file_infos) == 1)
    {
        fi = FM_FILE_INFO(fm_list_peek_head(job->file_infos));
        do {
            item = NULL;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if( item && item->fi && item->fi->path && fm_path_equal(item->fi->path, fi->path) )
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
            if( item && item->fi && item->fi->path )
            {
                for(l = fm_list_peek_head_link(job->file_infos); l; l = l->next )
                {
                    fi = FM_FILE_INFO(l->data);
                    if(fm_path_equal(item->fi->path, fi->path))
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
}

static void update_vol(FmPlacesModel* model, FmPlaceItem* item, GtkTreeIter* it, FmFileInfoJob* job)
{
    FmIcon* icon;
    GIcon* gicon;
    char* name;
    GdkPixbuf* pix;
    GMount* mount;
    FmPath* path;

    name = g_volume_get_name(item->vol);
    if(item->fi->icon)
        fm_icon_unref(item->fi->icon);
    gicon = g_volume_get_icon(item->vol);
    icon = fm_icon_from_gicon(gicon);
    item->fi->icon = icon;
    g_object_unref(gicon);

    mount = g_volume_get_mount(item->vol);
    if(mount)
    {
        GFile* gf = g_mount_get_root(mount);
        path = fm_path_new_for_gfile(gf);
        g_object_unref(gf);
        g_object_unref(mount);
        item->vol_mounted = TRUE;
    }
    else
    {
        path = NULL;
        item->vol_mounted = FALSE;
    }

    if(!fm_path_equal(item->fi->path, path))
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

    pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
    gtk_list_store_set(GTK_LIST_STORE(model), it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, name, -1);
    g_object_unref(pix);
    g_free(name);
}

static void add_vol(FmPlacesModel* model, GVolume* vol, FmFileInfoJob* job)
{
    GtkTreeIter it;
    FmPlaceItem* item;
    item = g_slice_new0(FmPlaceItem);
    item->fi = fm_file_info_new();
    item->type = FM_PLACES_ITEM_VOL;
    item->vol = (GVolume*)g_object_ref(vol);
    gtk_list_store_insert_before(GTK_LIST_STORE(model), &it, &model->sep_it);
    gtk_list_store_set(GTK_LIST_STORE(model), &it, FM_PLACES_MODEL_COL_INFO, item, -1);
    update_vol(model, item, &it, job);
}

static FmPlaceItem* find_vol(FmPlacesModel* model, GVolume* vol, GtkTreeIter* _it)
{
    GtkTreeIter it;
    FmPlaceItem* item;
    /* FIXME: don't need to find from the first iter */
    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &it))
    {
        do
        {
            FmPlaceItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);

            if(item && item->type == FM_PLACES_ITEM_VOL && item->vol == vol)
            {
                *_it = it;
                return item;
            }
        }while(it.user_data != model->sep_it.user_data && gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it));
    }
    return NULL;
}

void on_vol_added(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    g_debug("add vol: %p, uuid: %s, udi: %s", vol, g_volume_get_identifier(vol, "uuid"), g_volume_get_identifier(vol, "hal-udi"));
    add_vol(model, vol, NULL);
    update_sep_tp(model);
}

void on_vol_removed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    FmPlaceItem* item;
    GtkTreeIter it;
    item = find_vol(model, vol, &it);
    /* g_debug("remove vol: %p, uuid: %s, udi: %s", vol, g_volume_get_identifier(vol, "uuid"), g_volume_get_identifier(vol, "hal-udi")); */
    if(item)
    {
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);
        place_item_free(item);
        update_sep_tp(model);
    }
}

void on_vol_changed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    FmPlaceItem* item;
    GtkTreeIter it;
    g_debug("vol-changed");
    item = find_vol(model, vol, &it);
    if(item)
        update_vol(model, item, &it, NULL);
}

void on_mount_added(GVolumeMonitor* vm, GMount* mount, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    GVolume* vol = g_mount_get_volume(mount);
    if(vol)
    {
        FmPlaceItem *item;
        GtkTreeIter it;
        item = find_vol(model, vol, &it);
        if(item && item->type == FM_PLACES_ITEM_VOL && !item->fi->path)
        {
            GtkTreePath* tp;
            GFile* gf = g_mount_get_root(mount);
            FmPath* path = fm_path_new_for_gfile(gf);
            g_debug("mount path: %s", path->name);
            g_object_unref(gf);
            fm_file_info_set_path(item->fi, path);
            if(path)
                fm_path_unref(path);
            item->vol_mounted = TRUE;

            /* inform the view to update mount indicator */
            tp = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &it);
            gtk_tree_model_row_changed(GTK_TREE_MODEL(model), tp, &it);
            gtk_tree_path_free(tp);
        }
        g_object_unref(vol);
    }
}

static void add_bookmarks(FmPlacesModel* model, FmFileInfoJob* job)
{
    FmPlaceItem* item;
    GList *bms, *l;
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
        item = g_slice_new0(FmPlaceItem);
        item->type = FM_PLACES_ITEM_PATH;
        item->fi = fm_file_info_new();
        item->fi->path = fm_path_ref(bm->path);
        fm_file_info_job_add(job, item->fi->path);

        if(fm_path_is_native(item->fi->path))
        {
            item->fi->icon = fm_icon_ref(icon);
            pix = folder_pix;
        }
        else
        {
            if(G_UNLIKELY(!remote_icon))
            {
                remote_icon = fm_icon_from_name("folder-remote");
                remote_pix = fm_icon_get_pixbuf(remote_icon, fm_config->pane_icon_size);
            }
            item->fi->icon = fm_icon_ref(remote_icon);
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
    GtkTreeIter it = model->sep_it;
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
            FmPlaceItem* item;
            GdkPixbuf* pix;
            guint32 n = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
            g_object_unref(inf);
            icon_name = n > 0 ? "user-trash-full" : "user-trash";
            icon = fm_icon_from_name(icon_name);
            gtk_tree_model_get(GTK_TREE_MODEL(model), &model->trash_it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if(item->fi->icon)
                fm_icon_unref(item->fi->icon);
            item->fi->icon = icon;
            /* update the icon */
            pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
            gtk_list_store_set(GTK_LIST_STORE(model), &model->trash_it, FM_PLACES_MODEL_COL_ICON, pix, -1);
            g_object_unref(pix);
        }
    }
    return FALSE;
}


static void on_trash_changed(GFileMonitor *monitor, GFile *gf, GFile *other, GFileMonitorEvent evt, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    if(model->trash_idle)
        g_source_remove(model->trash_idle);
    model->trash_idle = g_idle_add((GSourceFunc)update_trash_item, model);
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
        if(it.user_data != model->sep_it.user_data)
        {
            FmPlaceItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(model), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            /* FIXME: get icon size from FmConfig */
            pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
            gtk_list_store_set(GTK_LIST_STORE(model), &it, FM_PLACES_MODEL_COL_ICON, pix, -1);
            g_object_unref(pix);
        }
    }while( gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &it) );
}

static void on_use_trash_changed(FmConfig* cfg, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    if(cfg->use_trash && model->trash_it.user_data == NULL)
        create_trash_item(model);
    else
    {
        FmPlaceItem *item;
        gtk_tree_model_get(GTK_TREE_MODEL(model), &model->trash_it, FM_PLACES_MODEL_COL_INFO, &item, -1);
        gtk_list_store_remove(GTK_LIST_STORE(model), &model->trash_it);
        model->trash_it.user_data = NULL;
        place_item_free(item);

        if(model->trash_monitor)
        {
            g_signal_handlers_disconnect_by_func(model->trash_monitor, on_trash_changed, model);
            g_object_unref(model->trash_monitor);
            model->trash_monitor = NULL;
        }
        if(model->trash_idle)
        {
            g_source_remove(model->trash_idle);
            model->trash_idle = 0;
        }
    }
    update_sep_tp(model);
}

static void on_pane_icon_size_changed(FmConfig* cfg, gpointer user_data)
{
    FmPlacesModel* model = FM_PLACES_MODEL(user_data);
    update_icons(model);
}


static void create_trash_item(FmPlacesModel* model)
{
    GtkTreeIter it;
    FmPlaceItem* item;
    GdkPixbuf* pix;
    GFile* gf;

    gf = g_file_new_for_uri("trash:///");
    model->trash_monitor = fm_monitor_directory(gf, NULL);
    g_signal_connect(model->trash_monitor, "changed", G_CALLBACK(on_trash_changed), model);
    g_object_unref(gf);

    item = g_slice_new0(FmPlaceItem);
    item->type = FM_PLACES_ITEM_PATH;
    item->fi = fm_file_info_new();
    item->fi->path = fm_path_ref(fm_path_get_trash());
    item->fi->icon = fm_icon_from_name("user-trash");
    gtk_list_store_insert(GTK_LIST_STORE(model), &it, 2);
    pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
    gtk_list_store_set(GTK_LIST_STORE(model), &it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, _("Trash"), FM_PLACES_MODEL_COL_INFO, item, -1);
    g_object_unref(pix);
    model->trash_it = it;

    if(0 == model->trash_idle)
        model->trash_idle = g_idle_add((GSourceFunc)update_trash_item, model);
}

static void fm_places_model_init(FmPlacesModel *self)
{
    GType types[] = {GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER};
    GtkTreeIter it;
    GtkTreePath* tp;
    FmPlaceItem* item;
    GList *vols, *l;
    GIcon* gicon;
    FmIcon* icon;
    GFile* gf;
    GdkPixbuf* pix;
    FmFileInfoJob* job = fm_file_info_job_new(NULL, FM_FILE_INFO_JOB_FOLLOW_SYMLINK);
    GtkListStore* model = GTK_LIST_STORE(self);

    gtk_list_store_set_column_types(GTK_LIST_STORE(self), FM_PLACES_MODEL_N_COLS, types);

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

    item = g_slice_new0(FmPlaceItem);
    item->type = FM_PLACES_ITEM_PATH;
    item->fi = fm_file_info_new();
    item->fi->path = fm_path_ref(fm_path_get_home());
    item->fi->icon = fm_icon_from_name("user-home");
    gtk_list_store_append(model, &it);
    pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
    gtk_list_store_set(model, &it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, item->fi->path->name, FM_PLACES_MODEL_COL_INFO, item, -1);
    g_object_unref(pix);
    fm_file_info_job_add(job, item->fi->path);

    /* Only show desktop in side pane when the user has a desktop dir. */
    if(g_file_test(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP), G_FILE_TEST_IS_DIR))
    {
        item = g_slice_new0(FmPlaceItem);
        item->type = FM_PLACES_ITEM_PATH;
        item->fi = fm_file_info_new();
        item->fi->path = fm_path_ref(fm_path_get_desktop());
        item->fi->icon = fm_icon_from_name("user-desktop");
        gtk_list_store_append(model, &it);
        pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
        gtk_list_store_set(model, &it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, _("Desktop"), FM_PLACES_MODEL_COL_INFO, item, -1);
        g_object_unref(pix);
        fm_file_info_job_add(job, item->fi->path);
    }

    if(fm_config->use_trash)
        create_trash_item(self); /* FIXME: how to handle trash can? */

    item = g_slice_new0(FmPlaceItem);
    item->type = FM_PLACES_ITEM_PATH;
    item->fi = fm_file_info_new();
    item->fi->path = fm_path_ref(fm_path_get_apps_menu());
    item->fi->icon = fm_icon_from_name("system-software-install");
    gtk_list_store_append(model, &it);
    pix = fm_icon_get_pixbuf(item->fi->icon, fm_config->pane_icon_size);
    gtk_list_store_set(model, &it, FM_PLACES_MODEL_COL_ICON, pix, FM_PLACES_MODEL_COL_LABEL, _("Applications"), FM_PLACES_MODEL_COL_INFO, item, -1);
    g_object_unref(pix);
    /* fm_file_info_job_add(job, item->fi->path); */

    /* volumes */
    self->vol_mon = g_volume_monitor_get();
    g_signal_connect(self->vol_mon, "volume-added", G_CALLBACK(on_vol_added), self);
    g_signal_connect(self->vol_mon, "volume-removed", G_CALLBACK(on_vol_removed), self);
    g_signal_connect(self->vol_mon, "volume-changed", G_CALLBACK(on_vol_changed), self);
    g_signal_connect(self->vol_mon, "mount-added", G_CALLBACK(on_mount_added), self);

    /* separator */
    gtk_list_store_append(model, &self->sep_it);

    /* add volumes to side-pane */
    vols = g_volume_monitor_get_volumes(self->vol_mon);
    for(l=vols;l;l=l->next)
    {
        GVolume* vol = G_VOLUME(l->data);
        add_vol(self, vol, job);
        g_object_unref(vol);
    }
    g_list_free(vols);

    /* get the path of separator */
    self->sep_tp = gtk_tree_model_get_path(GTK_TREE_MODEL(self), &self->sep_it);

    self->bookmarks = fm_bookmarks_get(); /* bookmarks */
    g_signal_connect(self->bookmarks, "changed", G_CALLBACK(on_bookmarks_changed), self);

    /* add bookmarks to side pane */
    add_bookmarks(self, job);

    g_signal_connect(job, "finished", G_CALLBACK(on_file_info_job_finished), self);
    self->jobs = g_slist_prepend(self->jobs, job);
    fm_job_run_async(FM_JOB(job));
}

const GtkTreePath* fm_places_model_get_separator_path(FmPlacesModel* model)
{
    return model->sep_tp;
}

gboolean fm_places_model_iter_is_separator(FmPlacesModel* model, GtkTreeIter* it)
{
    return it && it->user_data == model->sep_it.user_data;
}

gboolean fm_places_model_path_is_separator(FmPlacesModel* model, GtkTreePath* tp)
{
    return tp && gtk_tree_path_compare(model->sep_tp, tp) == 0;
}

gboolean fm_places_model_path_is_bookmark(FmPlacesModel* model, GtkTreePath* tp)
{
    return tp && gtk_tree_path_compare(model->sep_tp, tp) < 0;
}

gboolean fm_places_model_path_is_places(FmPlacesModel* model, GtkTreePath* tp)
{
    return tp && gtk_tree_path_compare(model->sep_tp, tp) > 0;
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


static void fm_places_model_finalize(GObject *object)
{
    FmPlacesModel *self;
    GtkTreeIter it;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_PLACES_MODEL(object));

    self = FM_PLACES_MODEL(object);

    if(self->jobs)
    {
        GSList* l;
        for(l = self->jobs; l; l=l->next)
        {
            fm_job_cancel(FM_JOB(l->data));
            g_object_unref(l->data);
        }
        g_slist_free(self->jobs);
    }

    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(self), &it))
    {
        do
        {
            FmPlaceItem* item;
            gtk_tree_model_get(GTK_TREE_MODEL(self), &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if(G_LIKELY(item))
                place_item_free(item);
        }while(gtk_tree_model_iter_next(GTK_TREE_MODEL(self), &it));
    }

    gtk_tree_path_free(self->sep_tp);

    g_signal_handler_disconnect(gtk_icon_theme_get_default(), self->theme_change_handler);
    g_signal_handler_disconnect(fm_config, self->use_trash_change_handler);
    g_signal_handler_disconnect(fm_config, self->pane_icon_size_change_handler);

    g_signal_handlers_disconnect_by_func(self->vol_mon, on_vol_added, self);
    g_signal_handlers_disconnect_by_func(self->vol_mon, on_vol_removed, self);
    g_signal_handlers_disconnect_by_func(self->vol_mon, on_vol_changed, self);
    g_signal_handlers_disconnect_by_func(self->vol_mon, on_mount_added, self);
    g_object_unref(self->vol_mon);

    if(self->trash_monitor)
    {
        g_signal_handlers_disconnect_by_func(self->trash_monitor, on_trash_changed, self);
        g_object_unref(self->trash_monitor);
    }
    if(self->trash_idle)
        g_source_remove(self->trash_idle);

    G_OBJECT_CLASS(fm_places_model_parent_class)->finalize(object);
}

static void fm_places_model_class_init(FmPlacesModelClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_places_model_finalize;
}


GtkListStore *fm_places_model_new(void)
{
    return g_object_new(FM_TYPE_PLACES_MODEL, NULL);
}

void fm_places_model_mount_indicator_cell_data_func(GtkCellLayout *cell_layout,
                                           GtkCellRenderer *render,
                                           GtkTreeModel *tree_model,
                                           GtkTreeIter *it,
                                           gpointer user_data)
{
    FmPlaceItem* item;
    GdkPixbuf* pix = NULL;
    gtk_tree_model_get(tree_model, it, FM_PLACES_MODEL_COL_INFO, &item, -1);
    if(item && item->vol_mounted)
        pix = FM_PLACES_MODEL(tree_model)->eject_icon;
    g_object_set(render, "pixbuf", pix, NULL);
}

gboolean fm_places_model_find_path(FmPlacesModel* model, GtkTreeIter* iter, FmPath* path)
{
    GtkTreeIter it;
    GtkTreeModel* model_ = GTK_TREE_MODEL(model);
    if(gtk_tree_model_get_iter_first(model_, &it))
    {
        FmPlaceItem* item;
        do{
            item = NULL;
            gtk_tree_model_get(model_, &it, FM_PLACES_MODEL_COL_INFO, &item, -1);
            if(item && item->fi && fm_path_equal(item->fi->path, path))
            {
                *iter = it;
                return TRUE;
            }
        }while(gtk_tree_model_iter_next(model_, &it));
    }
    return FALSE;
}
