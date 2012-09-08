/*
 * libfm-file-search-cli-demo.c
 * 
 * Copyright 2010 Shae Smittle <starfall87@gmail.com>
 * Copyright 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include <stdio.h>
#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "fm-gtk.h"

#include <string.h>

static char * target = NULL;
static char * target_contains = NULL;
static char * path_list = NULL;
static char * target_type = NULL;
static gboolean not_recursive = FALSE;
static gboolean show_hidden = FALSE;
static gboolean regex_target = FALSE;
static gboolean regex_content = FALSE;
static gboolean exact_target = FALSE;
static gboolean exact_content = FALSE;
static gboolean case_sensitive_target = FALSE;
static gboolean case_sensitive_content = FALSE;
static gint64 minimum_size = -1;
static gint64 maximum_size = -1;

static GOptionEntry entries[] =
{
	{"target", 't', 0, G_OPTION_ARG_STRING, &target, "phrase to search for in file names", NULL },
	{"contains", 'c', 0, G_OPTION_ARG_STRING, &target_contains, "phrase to search for in file contents", NULL},
	{"paths", 'p', 0, G_OPTION_ARG_STRING, &path_list, "paths to search through i.e. /usr/share/local:/usr/share", NULL},
	{"type", 'y', 0, G_OPTION_ARG_STRING, &target_type, "system string representation of type of files to search", NULL},
	{"norecurse", 'r', 0, G_OPTION_ARG_NONE, &not_recursive, "disables recursively searching directories", NULL},
	{"showhidden", 's', 0, G_OPTION_ARG_NONE, &show_hidden, "enables searching hidden files", NULL},
	{"regextarget", 'e', 0, G_OPTION_ARG_NONE, &regex_target, "enables regex target searching", NULL},
	{"regexcontent", 'g', 0, G_OPTION_ARG_NONE, &regex_content, "enables regex target searching", NULL},
	{"exacttarget", 'x', 0, G_OPTION_ARG_NONE, &exact_target, "enables regex target searching", NULL},
	{"exactcontent", 'a', 0, G_OPTION_ARG_NONE, &exact_content, "enables regex target searching", NULL},
	{"casesensitivetarget", 'n', 0, G_OPTION_ARG_NONE, &case_sensitive_target, "enables case sensitive target searching", NULL},
	{"casesensitivecontent", 'i', 0, G_OPTION_ARG_NONE, &case_sensitive_content, "enables case sensitive content searching", NULL},
	{"minimumsize", 'u', 0,G_OPTION_ARG_INT64, &minimum_size, "minimum size of file that is a match", NULL},
	{"maximumsize", 'w', 0, G_OPTION_ARG_INT64, &maximum_size, "maximum size of file taht is a match", NULL},
	{NULL}
};

int main(int argc, char** argv)
{
	gtk_init(&argc, &argv);

	fm_gtk_init(NULL);

	GOptionContext * context;

	context = g_option_context_new(" - test for libfm file search");
	g_option_context_add_main_entries(context, entries, NULL);
	g_option_context_parse(context, &argc, &argv, NULL);

	FmFileSearch * search;

	char * path_list_token = strtok(path_list, ":");
	FmPathList * target_folders = fm_path_list_new();

	while(path_list_token != NULL)
	{
		FmPath * path = fm_path_new_for_str(path_list_token);
		fm_list_push_tail(target_folders, path);
		path_list_token = strtok(NULL, ":");
	}
	search = fm_file_search_new(target_folders);

	fm_file_search_set_show_hidden(search, show_hidden);
	fm_file_search_set_recursive(search, !not_recursive);

	if(regex_target)
		fm_file_search_set_target_mode(search, FM_FILE_SEARCH_MODE_REGEX);
	else if(exact_target)
		fm_file_search_set_target_mode(search, FM_FILE_SEARCH_MODE_EXACT);

	if(regex_content)
		fm_file_search_set_content_mode(search, FM_FILE_SEARCH_MODE_REGEX);
	else if(exact_content)
		fm_file_search_set_content_mode(search, FM_FILE_SEARCH_MODE_EXACT);

	if(case_sensitive_target)
		fm_file_search_set_case_sensitive_target(search, TRUE);

	if(case_sensitive_content)
		fm_file_search_set_case_sensitive_content(search, TRUE);

	if(minimum_size >= 0)
		fm_file_search_add_search_func(search, fm_file_search_minimum_size_rule, &minimum_size);

	if(maximum_size >= 0)
		fm_file_search_add_search_func(search, fm_file_search_maximum_size_rule, &maximum_size);

	if(target != NULL)
		fm_file_search_add_search_func(search, fm_file_search_target_rule, target);

	if(target_type != NULL)
		fm_file_search_add_search_func(search, fm_file_search_target_type_rule, target_type);

	if(target_contains != NULL)
		fm_file_search_add_search_func(search, fm_file_search_target_contains_rule, target_contains);

	GtkWidget * window;
	FmFolderModel * model;
	GtkWidget * tree;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(window, 400, 300);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	model = fm_folder_model_new(FM_FOLDER(search), TRUE);
	fm_folder_model_set_folder(model, FM_FOLDER(search));
	GtkWidget * view = fm_folder_view_new(FM_FV_LIST_VIEW);
	fm_folder_view_set_model(FM_FOLDER_VIEW(view), model);
	fm_folder_view_set_selection_mode(FM_FOLDER_VIEW(view), GTK_SELECTION_MULTIPLE);
	g_object_unref(model);
	fm_file_search_run(search);
	g_object_unref(search);

	gtk_container_add(GTK_CONTAINER(window), view);
	gtk_widget_show(view);
	gtk_widget_show(window);

	gtk_main();

	return 0;
}
