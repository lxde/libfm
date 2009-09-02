/*
 *      fm-file-properties.c
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

#include <config.h>
#include <glib/gi18n.h>

#include "fm-file-info.h"
#include "fm-file-properties.h"
#include "fm-deep-count-job.h"
#include "fm-utils.h"
#include "fm-path.h"

#include "fm-gtk-utils.h"

#define UI_FILE        PACKAGE_UI_DIR"/file-prop.ui"
#define    GET_WIDGET(name)    data->name = (GtkWidget*)gtk_builder_get_object(builder, #name);

typedef struct _FmFilePropData FmFilePropData;
struct _FmFilePropData
{
    GtkWidget* dlg;
    GtkWidget* icon;
    GtkWidget* name;
    GtkWidget* dir;
    GtkWidget* target;
    GtkWidget* target_label;
    GtkWidget* type;
    GtkWidget* open_with_label;
    GtkWidget* open_with;
    GtkWidget* total_size;
    GtkWidget* size_on_disk;
    GtkWidget* mtime;
    GtkWidget* atime;

    FmFileInfoList* files;
    FmFileInfo* fi;
    gboolean single_type;
    gboolean single_file;
    FmMimeType* mime_type;
    gpointer separator_iter_user_data;
    gpointer other_apps_iter_user_data;
    GAppInfo* new_app;

    guint timeout;
    FmJob* dc_job;
};


static gboolean on_timeout(FmFilePropData* data)
{
    char size_str[128];
    FmDeepCountJob* dc = data->dc_job;
    gdk_threads_enter();

    if(G_LIKELY(dc))
    {
        fm_file_size_to_str(size_str, dc->total_size, TRUE);
        gtk_label_set_text(data->total_size, size_str);

        fm_file_size_to_str(size_str, dc->total_block_size, TRUE);
        gtk_label_set_text(data->size_on_disk, size_str);
    }
    gdk_threads_leave();
    return TRUE;
}

static void on_finished(FmDeepCountJob* job, FmFilePropData* data)
{
    on_timeout(data); /* update display */
    if(data->timeout)
    {
        g_source_remove(data->timeout);
        data->timeout = 0;
    }
    data->dc_job = NULL;
}

static void fm_file_prop_data_free(FmFilePropData* data)
{
    if(data->timeout)
        g_source_remove(data->timeout);
    if(data->dc_job) /* FIXME: check if it's running */
        fm_job_cancel(data->dc_job);
    if(data->new_app)
        g_object_unref(data->new_app);
    fm_list_unref(data->files);
    g_slice_free(FmFilePropData, data);
}

static void on_response(GtkDialog* dlg, int response, FmFilePropData* data)
{
    if( response == GTK_RESPONSE_OK )
    {
        /* change default application for the mime-type */
        if(data->new_app && data->mime_type && data->mime_type->type)
        {
            GError* err = NULL;
            g_app_info_set_as_default_for_type(data->new_app, data->mime_type->type, &err);
            if(err)
            {
                fm_show_error(dlg, err->message);
                g_error_free(err);
            }
        }

        if(data->single_file) /* when only one file is shown */
        {
            /* if the user has changed its name */
            if( g_strcmp0(data->fi->disp_name, gtk_entry_get_text(data->name)) )
            {
                /* FIXME: rename the file or set display name for it. */
            }
        }
    }
    gtk_widget_destroy(dlg);
}

static void update_ui(FmFilePropData* data)
{
    GtkImage* img = (GtkImage*)data->icon;
    const char* FILES_OF_MULTIPLE_TYPE = _("Files of different types");

    if( data->single_type ) /* all files are of the same mime-type */
    {
        GIcon* icon = NULL;
        /* FIXME: handle custom icons for some files */

        /* FIXME: display special property pages for special files or
         * some specified mime-types. */
        if( data->single_file ) /* only one file is selected. */
        {
            FmFileInfo* fi = (FmFileInfo*)fm_list_peek_head(data->files);
            if(fi->icon)
                icon = fi->icon->gicon;
        }

        if(data->mime_type)
        {
            if(!icon)
            {
                FmIcon* ficon = fm_mime_type_get_icon(data->mime_type);
                if(ficon)
                    icon = ficon->gicon;
            }
            gtk_label_set_text(data->type, fm_mime_type_get_desc(data->mime_type));
        }

        if(icon)
            gtk_image_set_from_gicon(img, icon, GTK_ICON_SIZE_DIALOG);

        if( data->single_file && fm_file_info_is_symlink(data->fi) )
        {
            /* FIXME: display symlink target */
        }
        else
        {
            gtk_widget_destroy(data->target_label);
            gtk_widget_destroy(data->target);
        }
    }
    else
    {
        gtk_image_set_from_stock(img, GTK_STOCK_DND_MULTIPLE, GTK_ICON_SIZE_DIALOG);
        gtk_entry_set_text(data->name, FILES_OF_MULTIPLE_TYPE);
        gtk_widget_set_sensitive(data->name, FALSE);

        gtk_label_set_text(data->type, FILES_OF_MULTIPLE_TYPE);

        gtk_widget_destroy(data->target_label);
        gtk_widget_destroy(data->target);

        gtk_widget_destroy(data->open_with_label);
        gtk_widget_destroy(data->open_with);
    }

    /* FIXME: check if all files has the same parent dir, mtime, or atime */
    if( data->single_file )
    {
        FmPath* parent = fm_path_get_parent(fm_file_info_get_path(data->fi));
        char* parent_str = fm_path_to_str(parent);
        fm_path_unref(parent);
        gtk_entry_set_text(GTK_ENTRY(data->name), fm_file_info_get_disp_name(data->fi));
        gtk_label_set_text(GTK_LABEL(data->dir), parent_str);
        g_free(parent_str);
        gtk_label_set_text(GTK_LABEL(data->mtime), fm_file_info_get_disp_mtime(data->fi));
    }

    on_timeout(data);
}

static gboolean is_row_separator(GtkTreeModel* model, GtkTreeIter* it, gpointer user_data)
{
    FmFilePropData* data = (FmFilePropData*)user_data;
    /* FIXME: this is dirty but it works! */
    return data->separator_iter_user_data == it->user_data;
}

static void on_app_selected(GtkComboBox* cb, FmFilePropData* data)
{
    GtkTreeIter it;
    if( gtk_combo_box_get_active_iter(cb, &it) )
    {
        GtkTreeModel* model = gtk_combo_box_get_model(cb);
        /* if the user chooses "Other Applications..." */
        if(it.user_data == data->other_apps_iter_user_data)
        {
            /* FIXME: let the user choose a app or add custom actions here. */
        }
        else /* an application in the list is selected */
        {
            if(data->new_app) /* unref the old one */
                g_object_unref(data->new_app);
            gtk_tree_model_get(model, &it, 2, &data->new_app, -1);
        }
    }
}

static void init_application_list(FmFilePropData* data)
{
    if(data->single_type && data->mime_type)
    {
        if(g_strcmp0(data->mime_type->type, "inode/directory"))
        {
            GtkListStore* store = gtk_list_store_new(3, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_APP_INFO);
            GtkTreeIter it ,def_it = {0};
            GList *apps = g_app_info_get_all_for_type(data->mime_type->type), *l;
            GAppInfo* def_app = g_app_info_get_default_for_type(data->mime_type->type, FALSE);
            for(l = apps; l; l = l->next)
            {
                GAppInfo* app = G_APP_INFO(l->data);
                gtk_list_store_append(store, &it);
                gtk_list_store_set(store, &it,
                                   0, g_app_info_get_icon(app),
                                   1, g_app_info_get_name(app),
                                   2, app, -1);
                if(g_app_info_equal(app, def_app))
                {
                    /* this is the default app */
                    def_it = it;
                }
                g_object_unref(app);
            }
            g_object_unref(def_app); /* free default app */
            g_list_free(apps);
            gtk_list_store_append(store, &it); /* separator */
            data->separator_iter_user_data = it.user_data;

            gtk_list_store_append(store, &it); /* other applications */
            data->other_apps_iter_user_data = it.user_data;
            gtk_list_store_set(store, &it,
                               0, NULL,
                               1, _("Other Applications"),
                               2, NULL);
            gtk_combo_box_set_model(data->open_with, store);
            if(def_it.user_data) /* default app is found */
                gtk_combo_box_set_active_iter(data->open_with, &def_it);
            gtk_combo_box_set_row_separator_func(data->open_with, is_row_separator, data, NULL);
            g_object_unref(store);

            g_signal_connect(data->open_with, "changed",
                             G_CALLBACK(on_app_selected), data);
        }
        else /* shouldn't allow set file association for folders. */
        {
            gtk_widget_destroy(data->open_with_label);
            gtk_widget_destroy(data->open_with);
        }
    }
}

GtkWidget* fm_file_properties_widget_new(FmFileInfoList* files, gboolean toplevel)
{
    GtkBuilder* builder=gtk_builder_new();
    GtkWidget* dlg, *total_size;
    FmFilePropData* data;
    FmPathList* paths;

    data = g_slice_new0(FmFilePropData);

    data->files = fm_list_ref(files);
    data->single_type = fm_file_info_list_is_same_type(files);
    data->single_file = (fm_list_get_length(files) == 1);
    data->fi = fm_list_peek_head(files);
    if(data->single_type)
        data->mime_type = data->fi->type; /* FIXME: do we need ref counting here? */
    paths = fm_path_list_new_from_file_info_list(files);
    data->dc_job = fm_deep_count_job_new(paths, FM_DC_JOB_DEFAULT);
    fm_list_unref(paths);

    if(toplevel)
    {
        gtk_builder_add_from_file(builder, UI_FILE, NULL);
        GET_WIDGET(dlg);
        gtk_dialog_set_alternative_button_order(data->dlg, GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
    }
    else
    {
        /* FIXME: is this really useful? */
        const char *names[]={"notebook", NULL};
        gtk_builder_add_objects_from_file(builder, UI_FILE, names, NULL);
        data->dlg = (GtkWidget*)gtk_builder_get_object(builder, "notebook");
    }

    dlg = data->dlg;

    GET_WIDGET(icon);
    GET_WIDGET(name);
    GET_WIDGET(dir);
    GET_WIDGET(target);
    GET_WIDGET(target_label);
    GET_WIDGET(type);
    GET_WIDGET(open_with_label);
    GET_WIDGET(open_with);
    GET_WIDGET(total_size);
    GET_WIDGET(size_on_disk);
    GET_WIDGET(mtime);
    GET_WIDGET(atime);

    g_object_unref(builder);

    init_application_list(data);

    data->timeout = g_timeout_add(600, (GSourceFunc)on_timeout, data);
    g_signal_connect(dlg, "response", G_CALLBACK(on_response), data);
    g_signal_connect_swapped(dlg, "destroy", G_CALLBACK(fm_file_prop_data_free), data);
    g_signal_connect(data->dc_job, "finished", on_finished, data);

    fm_job_run_async(data->dc_job);

    update_ui(data);

    return dlg;
}

gboolean fm_show_file_properties(FmFileInfoList* files)
{
    GtkWidget* dlg = fm_file_properties_widget_new(files, TRUE);
    gtk_widget_show(dlg);
    return TRUE;
}

