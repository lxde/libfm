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
#include "fm-file-properties.h"
#include "fm-deep-count-job.h"
#include "fm-utils.h"
#include "fm-path.h"

#define UI_FILE		PACKAGE_UI_DIR"/file-prop.ui"
#define	GET_WIDGET(name)	data->name = (GtkWidget*)gtk_builder_get_object(builder, #name);

typedef struct _FmFilePropData FmFilePropData;
struct _FmFilePropData
{
	GtkWidget* dlg;
	GtkWidget* name;
	GtkWidget* dir;
	GtkWidget* target;
	GtkWidget* target_label;
	GtkWidget* type;
	GtkWidget* open_with;
	GtkWidget* total_size;
	GtkWidget* mtime;
	GtkWidget* atime;

	FmFileInfoList* files;
	guint timeout;
	FmJob* dc_job;
};

static GQuark data_id = 0;
#define	get_data(obj)	(FmFilePropData*)g_object_get_qdata(obj, data_id);

static gboolean on_timeout(FmDeepCountJob* dc)
{
	char size_str[128];
	FmFilePropData* data = get_data(dc);
	fm_file_size_to_str(size_str, dc->total_size, TRUE);
	gtk_label_set_text(data->total_size, size_str);
	return TRUE;
}

static void on_finished(FmDeepCountJob* job, FmFilePropData* data)
{
	on_timeout(job); /* update display */
	if(data->timeout)
	{
		g_source_remove(data->timeout);
		data->timeout = 0;
	}
	g_debug("Finished!");
	data->dc_job = NULL;
}

static void fm_file_prop_data_free(FmFilePropData* data)
{
	if(data->timeout)
		g_source_remove(data->timeout);
	if(data->dc_job) /* FIXME: check if it's running */
	{
		fm_job_cancel(data->dc_job);
	}
	fm_list_unref(data->files);
	g_slice_free(FmFilePropData, data);
}

static void on_response(GtkDialog* dlg, int response, FmFilePropData* data)
{
	gtk_widget_destroy(dlg);
}

GtkWidget* fm_file_properties_widget_new(FmFileInfoList* files, gboolean toplevel)
{
	GtkBuilder* builder=gtk_builder_new();
	GtkWidget* dlg, *total_size;
	FmFilePropData* data;
	FmPathList* paths;

	data = g_slice_new(FmFilePropData);

	data->files = fm_list_ref(files);
	paths = fm_path_list_new_from_file_info_list(files);
	data->dc_job = fm_deep_count_job_new(paths);
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

	if( G_UNLIKELY( 0 == data_id ) )
		data_id = g_quark_from_static_string("FmFileProp");
	g_object_set_qdata(dlg, data_id, data);

	GET_WIDGET(name);
	GET_WIDGET(dir);
	GET_WIDGET(target);
	GET_WIDGET(target_label);
	GET_WIDGET(type);
	GET_WIDGET(open_with);
	GET_WIDGET(total_size);
	GET_WIDGET(mtime);
	GET_WIDGET(atime);

	g_object_unref(builder);

	g_object_set_qdata(data->dc_job, data_id, data);
	data->timeout = g_timeout_add(500, (GSourceFunc)on_timeout, g_object_ref(data->dc_job));
	g_signal_connect(dlg, "response", G_CALLBACK(on_response), data);
	g_signal_connect_swapped(dlg, "destroy", G_CALLBACK(fm_file_prop_data_free), data);
	g_signal_connect(data->dc_job, "finished", on_finished, data);

	fm_job_run(data->dc_job);
	return dlg;
}

gboolean fm_show_file_properties(FmFileInfoList* files)
{
	GtkWidget* dlg = fm_file_properties_widget_new(files, TRUE);
	gtk_widget_show(dlg);
	return TRUE;
}

