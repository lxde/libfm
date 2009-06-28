/*
 *      fm-progress-dlg.c
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
#include "fm-progress-dlg.h"

typedef struct _FmProgressDlgData FmProgressData;
struct _FmProgressDlgData
{
	GtkWidget* dlg;
	GtkWidget* act;
	GtkWidget* src;
	GtkWidget* dest;
	GtkWidget* current;
	GtkWidget* progress;
	FmFileOpsJob* job;
};

static void data_free(GtkWidget* dlg, FmProgressData* data)
{
	if(data->job)
	{
		fm_job_cancel(data->job);
		g_object_unref(data->job);
	}
	g_slice_free(FmProgressData, data);
}

static void on_percent(FmFileOpsJob* job, guint percent, FmProgressData* data)
{
    char percent_text[64];
    g_snprintf(percent_text, 64, "%d %%", percent);
	gtk_progress_bar_set_fraction(data->progress, (gdouble)percent/100);
    gtk_progress_bar_set_text(data->progress, percent_text);
}

static void on_cur_file(FmFileOpsJob* job, const char* cur_file, FmProgressData* data)
{
	gtk_label_set_text(data->current, cur_file);
}

static void on_error(FmFileOpsJob* job)
{
	
}

static void on_ask(FmFileOpsJob* job)
{
	
}

static void on_finished(FmFileOpsJob* job, FmProgressData* data)
{
	g_object_unref(data->job);
	data->job = NULL;
	gtk_widget_destroy(data->dlg);
	g_debug("finished!");
}

GtkWidget* fm_progress_dlg_new(FmFileOpsJob* job)
{
	FmProgressData* data = g_slice_new(FmProgressData);
	GtkBuilder* builder = gtk_builder_new();

	gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/progress.ui", NULL);

	data->dlg = (GtkWidget*)gtk_builder_get_object(builder, "dlg");

	data->job = (FmFileOpsJob*)g_object_ref(job);
	g_signal_connect(data->dlg, "destroy", data_free, data);

	data->act = (GtkWidget*)gtk_builder_get_object(builder, "action");
	data->src = (GtkWidget*)gtk_builder_get_object(builder, "src");
	data->dest = (GtkWidget*)gtk_builder_get_object(builder, "dest");
	data->current = (GtkWidget*)gtk_builder_get_object(builder, "current");
	data->progress = (GtkWidget*)gtk_builder_get_object(builder, "progress");

	g_object_unref(builder);

	g_signal_connect(job, "ask", G_CALLBACK(on_ask), data);
	g_signal_connect(job, "error", G_CALLBACK(on_error), data);
	g_signal_connect(job, "cur-file", G_CALLBACK(on_cur_file), data);
	g_signal_connect(job, "percent", G_CALLBACK(on_percent), data);
	g_signal_connect(job, "finished", G_CALLBACK(on_finished), data);

	return data->dlg;
}

