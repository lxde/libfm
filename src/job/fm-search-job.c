/*
 * fm-search-job.c
 * 
 * Copyright 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 * Copyright 2010 Shae Smittle <starfall87@gmail.com>
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

#include "fm-search-job.h"
#include "fm-path.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define _GNU_SOURCE /* for FNM_CASEFOLD in fnmatch.h, a GNU extension */
#include <fnmatch.h>

extern const char gfile_info_query_attribs[]; /* defined in fm-file-info-job.c */

struct _FmSearchJobPrivate
{
    GSList* target_folders;
    char** name_patterns;
    GRegex* name_regex;
    char* content_pattern;
    GRegex* content_regex;
    char** mime_types;
    guint64 date1;
    guint64 date2;
    guint64 min_size;
    guint64 max_size;
    gboolean name_case_insensitive : 1;
    gboolean content_case_insensitive : 1;
    gboolean recursive : 1;
    gboolean show_hidden : 1;
};

G_DEFINE_TYPE(FmSearchJob, fm_search_job, FM_TYPE_DIR_LIST_JOB)

static gboolean fm_search_job_run(FmJob* job);
static void fm_search_job_dispose(GObject* object);

static void fm_search_job_match_folder(FmSearchJob * job, GFile * folder_path);
static gboolean fm_search_job_match_file(FmSearchJob * job, GFileInfo * info, GFile * parent);

static gboolean fm_search_job_match_filename(FmSearchJob* job, GFileInfo* info);
static gboolean fm_search_job_match_file_type(FmSearchJob* job, GFileInfo* info);
static gboolean fm_search_job_match_size(FmSearchJob* job, GFileInfo* info);
static gboolean fm_search_job_match_mtime(FmSearchJob* job, GFileInfo* info);
static gboolean fm_search_job_match_content(FmSearchJob* job, GFileInfo* info, GFile* parent);

static void fm_search_job_class_init(FmSearchJobClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    FmJobClass* job_class = FM_JOB_CLASS(klass);
    g_type_class_add_private((gpointer)klass, sizeof(FmSearchJobPrivate));

    object_class->dispose = fm_search_job_dispose;
    job_class->run = fm_search_job_run;
}


static void fm_search_job_init(FmSearchJob *job)
{
    job->priv = G_TYPE_INSTANCE_GET_PRIVATE(job, FM_TYPE_SEARCH_JOB, FmSearchJobPrivate);
    
    fm_job_init_cancellable(FM_JOB(job));
}

static void fm_search_job_dispose(GObject* object)
{
    FmSearchJobPrivate* priv = FM_SEARCH_JOB(object)->priv;
    if(priv->target_folders)
    {
        g_slist_foreach(priv->target_folders, (GFunc)fm_path_unref, NULL);
        g_slist_free(priv->target_folders);
        priv->target_folders = NULL;
    }

    if(priv->name_patterns)
    {
        g_strfreev(priv->name_patterns);
        priv->name_patterns = NULL;
    }

    if(priv->name_regex)
    {
        g_regex_unref(priv->name_regex);
        priv->name_regex = NULL;
    }
    
    if(priv->content_pattern)
    {
        g_free(priv->content_pattern);
        priv->content_pattern = NULL;
    }
    
    if(priv->content_regex)
    {
        g_regex_unref(priv->content_regex);
        priv->content_regex = NULL;
    }

    if(priv->mime_types)
    {
        g_strfreev(priv->mime_types);
        priv->mime_types = NULL;
    }
}

static gboolean fm_search_job_run(FmJob* job)
{
    FmSearchJobPrivate* priv = FM_SEARCH_JOB(job)->priv;
    GSList* l;
    for(l = priv->target_folders; l; l = l->next)
    {
        FmPath* folder_path = FM_PATH(l->data);
        GFile* gf = fm_path_to_gfile(folder_path);
        fm_search_job_match_folder(job, gf);
        g_object_unref(gf);
    }
    return TRUE;
}

/*
 * name: parse_date_str
 * @str: a string in YYYYMMDD format
 * Return: a time_t value
 */
static time_t parse_date_str(const char* str)
{
    int len = strlen(str);
    if(G_LIKELY(len >= 8))
    {
        struct tm timeinfo = {0};
        if(sscanf(str, "%4d%2d%2d", &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday) == 3)
            return mktime(&timeinfo);
    }
    return 0;
}

/*
 * parse_search_uri
 * @job
 * @uri: a search uri
 * 
 * Format of a search URI is similar to that of an http URI:
 * 
 * search://<folder1>:<folder2>:<folder...>?<parameter1=value1>&<parameter2=value2>&...
 * The optional parameter key/value pairs are:
 * show_hidden=<0 or 1>: whether to search for hidden files
 * recursive=<0 or 1>: whether to search sub folders recursively
 * name=<patterns>: patterns of filenames, separated by comma
 * name_regex=<regular expression>: regular expression
 * name_case_sensitive=<0 or 1>
 * content=<content pattern>: search for files containing the pattern
 * content_regex=<regular expression>: regular expression
 * content_case_sensitive=<0 or 1>
 * types=<mime-types>: mime-types to search for, separated by ;
 * min_size=<bytes>
 * max_size=<bytes>
 * date1=YYYYMMDD
 * date2=YYMMDD
 * 
 * An example to search all *.desktop files in /usr/share and /usr/local/share
 * can be written like this:
 * 
 * search://usr/share:/usr/local/share?recursive=1&show_hidden=0&name=*.desktop&name_ci=0
 * 
 * If the folder paths and parameters contain invalid characters for a
 * URI, they should be escaped.
 * 
 */
static void parse_search_uri(FmSearchJob* job, FmPath* uri)
{
    FmSearchJobPrivate* priv = job->priv;
    if(fm_path_is_search(uri))
    {
        char* uri_str = fm_path_to_str(uri);
        char* p = uri_str + strlen("search:/"); /* skip scheme part */
        char* params = strchr(p, '?');
        char* name_regex = NULL;
        char* content_regex = NULL;

        if(params)
        {
            *params = '\0';
            ++params;
            g_printf("params: %s\n", params);
        }

        /* add folder paths */
        for(; *p; ++p)
        {
            FmPath* path;
            char* sep = strchr(p, ':'); /* use : to separate multiple paths */
            if(sep)
                *sep = '\0';
            path = fm_path_new_for_str(p);
            g_print("target folder path: %s\n", p);
            /* add the path to target folders */
            priv->target_folders = g_slist_prepend(priv->target_folders, path);

            if(sep)
            {
                p = sep;
                if(params && p >= params)
                    break;
            }
            else
                break;
        }

        /* priv->target_folders = g_slist_reverse(priv->target_folders); */

        /* decode parameters */
        if(params)
        {
            while(*params)
            {
                /* parameters are in name=value pairs */
                char* name = params;
                char* value = strchr(name, '=');
                char* sep;

                if(value)
                {
                    *value = '\0';
                    ++value;
                    params = value;
                }
                sep = strchr(params, '&'); /* delimeter of parameters is & */
                if(sep)
                    *sep = '\0';

                g_printf("parameter name/value: %s = %s\n", name, value);

                if(strcmp(name, "show_hidden") == 0)
                    priv->show_hidden = (value[0] == '1') ? TRUE : FALSE;
                else if(strcmp(name, "recursive") == 0)
                    priv->recursive = (value[0] == '1') ? TRUE : FALSE;
                else if(strcmp(name, "name") == 0)
                    priv->name_patterns = g_strsplit(value, ",", 0);
                else if(strcmp(name, "name_regex") == 0)
                    name_regex = value;
                else if(strcmp(name, "name_ci") == 0)
                    priv->name_case_insensitive = (value[0] == '1') ? TRUE : FALSE;
                else if(strcmp(name, "content") == 0)
                    priv->content_pattern = g_uri_unescape_string(value, NULL);
                else if(strcmp(name, "content_regex") == 0)
                    content_regex = value;
                else if(strcmp(name, "content_ci") == 0)
                    priv->content_case_insensitive = (value[0] == '1') ? TRUE : FALSE;
                else if(strcmp(name, "types") == 0)
                    priv->mime_types = g_strsplit(value, ";", -1);
                else if(strcmp(name, "min_size") == 0)
                    priv->min_size = atoll(value);
                else if(strcmp(name, "max_size") == 0)
                    priv->max_size = atoll(value);
                else if(strcmp(name, "date1") == 0)
                    priv->date1 = (guint64)parse_date_str(value);
                else if(strcmp(name, "date2") == 0)
                    priv->date2 = (guint64)parse_date_str(value);

                /* continue with the next param=value pair */
                if(sep)
                    params = sep + 1;
                else
                    break;
            }
            
            if(name_regex)
            {
                GRegexCompileFlags flags = 0;
                if(priv->name_case_insensitive)
                    flags |= G_REGEX_CASELESS;
                priv->name_regex = g_regex_new(name_regex, flags, 0, NULL);
            }
            
            if(content_regex)
            {
                GRegexCompileFlags flags = 0;
                if(priv->content_case_insensitive)
                    flags |= G_REGEX_CASELESS;
                priv->content_regex = g_regex_new(content_regex, flags, 0, NULL);
            }
        }
        g_free(uri_str);
    }
}

FmSearchJob* fm_search_job_new(FmPath* search_uri)
{
    FmSearchJob* job = (FmSearchJob*)g_object_new(FM_TYPE_SEARCH_JOB, NULL);
    FmFileInfo* info = fm_file_info_new();
    fm_file_info_set_path(info, search_uri);
    fm_dir_list_job_set_emit_files_found(FM_DIR_LIST_JOB(job), TRUE);
    fm_dir_list_job_set_dir_path(FM_DIR_LIST_JOB(job), search_uri);
    fm_dir_list_job_set_dir_info(FM_DIR_LIST_JOB(job), info);
    fm_file_info_unref(info);

    parse_search_uri(job, search_uri);

    return job;
}


static void fm_search_job_match_folder(FmSearchJob * job, GFile * folder_path)
{
    FmSearchJobPrivate* priv = job->priv;
    GError * error = NULL;
    FmJobErrorAction action = FM_JOB_CONTINUE;
    GFileEnumerator * enumerator = g_file_enumerate_children(folder_path, gfile_info_query_attribs, G_FILE_QUERY_INFO_NONE, fm_job_get_cancellable(job), &error);
    if(enumerator == NULL)
        action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_MILD);
    else /* enumerator opened correctly */
    {
        while(!fm_job_is_cancelled(FM_JOB(job)))
        {
            GFileInfo * file_info = g_file_enumerator_next_file(enumerator, fm_job_get_cancellable(job), &error);
            if(file_info)
            {
                if(fm_search_job_match_file(job, file_info, folder_path))
                {
                    const char * name = g_file_info_get_name(file_info); /* does not need to be freed */
                    GFile *file = g_file_get_child(folder_path, name);
                    FmPath * path = fm_path_new_for_gfile(file);
                    FmFileInfo * info = fm_file_info_new_from_gfileinfo(path, file_info);
                    g_object_unref(file);

                    fm_dir_list_job_add_found_file(FM_DIR_LIST_JOB(job), info);

                    fm_file_info_unref(info);
                    fm_path_unref(path);
                }

                /* recurse upon each directory */
                if(priv->recursive && g_file_info_get_file_type(file_info) == G_FILE_TYPE_DIRECTORY)
                {
                    const char * name = g_file_info_get_name(file_info);
                    GFile * file = g_file_get_child(folder_path, name);
                    fm_search_job_match_folder(job, file);
                    g_object_unref(file);
                }
                g_object_unref(file_info);
            }
            else /* file_info == NULL */
            {
                if(error != NULL) /* this should catch g_file_enumerator_next_file errors if they pop up */
                {
                    action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_MILD);
                    g_error_free(error);
                    error = NULL;
                    if(action == FM_JOB_ABORT)
                        break;
                }
                else /* end of file list */
                    break;
            }
        }
        g_file_enumerator_close(enumerator, fm_job_get_cancellable(FM_JOB(job)), NULL);
        g_object_unref(enumerator);
    }

    if(action == FM_JOB_ABORT )
        fm_job_cancel(FM_JOB(job));
}


static gboolean fm_search_job_match_file(FmSearchJob * job, GFileInfo * info, GFile * parent)
{
    FmSearchJobPrivate* priv = job->priv;

    if(!priv->show_hidden && g_file_info_get_is_hidden(info))
        return FALSE;

    if(!fm_search_job_match_filename(job, info))
        return FALSE;

    if(!fm_search_job_match_file_type(job, info))
        return FALSE;

    if(!fm_search_job_match_size(job, info))
        return FALSE;

    if(!fm_search_job_match_mtime(job, info))
        return FALSE;

    if(!fm_search_job_match_content(job, info, parent))
        return FALSE;

    return TRUE;
}


/* functions for content search rule */

static gboolean strstr_nocase(char * haystack, char * needle)
{
    gboolean ret = FALSE;

    char * haystack_nocase = g_utf8_strdown(haystack, -1);
    char * needle_nocase = g_utf8_strdown(needle, -1);

    if(strstr(haystack_nocase, needle_nocase) != NULL)
        ret = TRUE;

    g_free(haystack_nocase);
    g_free(needle_nocase);

    return ret;
}

gboolean fm_search_job_match_filename(FmSearchJob* job, GFileInfo* info)
{
    FmSearchJobPrivate* priv = job->priv;
    gboolean ret;

    if(priv->name_regex)
    {
        const char* name = g_file_info_get_name(info);
        ret = g_regex_match(priv->name_regex, name, 0, NULL);
    }
    else if(priv->name_patterns)
    {
        ret = FALSE;
        const char* name = g_file_info_get_name(info);
        const char** ppattern;
        for(ppattern = priv->name_patterns; *ppattern; ++ppattern)
        {
            const char* pattern = *ppattern;
            /* FIXME: FNM_CASEFOLD is a GNU extension */
            int flags = FNM_PERIOD;
            if(priv->name_case_insensitive)
                flags |= FNM_CASEFOLD;
            if(fnmatch(pattern, name, flags) == 0)
                ret = TRUE;
        }
    }
    else
        ret = TRUE;
    return ret;
}

static gboolean fm_search_job_match_content_regex(FmSearchJob* job, GFileInfo* info, GInputStream* stream)
{
    FmSearchJobPrivate* priv = job->priv;
    gboolean ret = FALSE;
    GString* line = g_string_sized_new(4096);
    char buf[4096];
    GCancellable* cancellable = fm_job_get_cancellable(FM_JOB(job));
    /* TODO: finish the regexp based matching */
    for(;;)
    {
        /* regular expression search like grep is line-based */
        char* eol;
        gssize size = g_input_stream_read(stream, buf, sizeof(buf) - 1, cancellable, NULL);
        if(size <=0) /* error or EOF */
            break;
        buf[size] = '\0';
        eol = strchr(buf, '\n');
        if(eol)
        {
        }
    }
    g_string_free(line, TRUE);
    return ret;
}

static gboolean fm_search_job_match_content_exact(FmSearchJob* job, GFileInfo* info, GInputStream* stream)
{
    FmSearchJobPrivate* priv = job->priv;
    gboolean ret = FALSE;
    char* buf;
    gssize size;
    int pattern_len = strlen(priv->content_pattern);
    int buf_len = pattern_len > 4095 ? pattern_len : 4095;
    buf = g_new(char*, buf_len + 1); /* +1 for terminating null char. */
    for(;;)
    {
        char* found;
        size = g_input_stream_read(stream, buf, buf_len, fm_job_get_cancellable(FM_JOB(job)), NULL);
        if(size <=0)
            break;
        buf[size] = '\0';
        /* TODO: only searching the buffer is not enough.
         * it's possible that the content of buffer partially match
         * the pattern and might fully match the pattern if more bytes
         * are read later. */

        found = strstr(buf, priv->content_pattern);
        if(found)
        {
            ret = TRUE;
            break;
        }
    }
    g_free(buf);
    return ret;
}

gboolean fm_search_job_match_content(FmSearchJob* job, GFileInfo* info, GFile* parent)
{
    FmSearchJobPrivate* priv = job->priv;
    gboolean ret;
    if(priv->content_pattern || priv->content_regex)
    {
        ret = FALSE;
        if(g_file_info_get_file_type(info) == G_FILE_TYPE_REGULAR && g_file_info_get_size(info) > 0)
        {
            GError * error = NULL;
            GFile* file = g_file_get_child(parent, g_file_info_get_name(info));
            /* NOTE: I disabled mmap-based search since this could cause
             * unexpected crashes sometimes if the mapped files are
             * removed or changed during the search. */
            GFileInputStream * stream = g_file_read(file, fm_job_get_cancellable(FM_JOB(job)), &error);
            g_object_unref(file);

            if(stream)
            {
                if(priv->content_regex)
                    fm_search_job_match_content_regex(job, info, stream);
                else if(priv->content_pattern)
                    fm_search_job_match_content_exact(job, info, stream);

                g_input_stream_close(stream, NULL, NULL);
                g_object_unref(stream);
            }
            else
            {
                FmJobErrorAction action = FM_JOB_CONTINUE;
                action = fm_job_emit_error(FM_JOB(job), error, FM_JOB_ERROR_MILD);
                g_error_free(error);

                if(action == FM_JOB_ABORT)
                    fm_job_cancel(FM_JOB(job));
            }
        }
    }
    else
        ret = TRUE;
    return ret;
}

gboolean fm_search_job_match_file_type(FmSearchJob* job, GFileInfo* info)
{
    FmSearchJobPrivate* priv = job->priv;
    gboolean ret;
    if(priv->mime_types)
    {
        const file_type = g_file_info_get_content_type(info);
        const char** mime_type;
        ret = FALSE;
        for(mime_type = priv->mime_types; *mime_type; ++mime_type)
        {
            if(g_content_type_is_a(file_type, *mime_type))
            {
                ret = TRUE;
                break;
            }
        }
    }
    else
        ret = TRUE;
    return ret;
}

gboolean fm_search_job_match_size(FmSearchJob* job, GFileInfo* info)
{
    FmSearchJobPrivate* priv = job->priv;
    guint64 size = g_file_info_get_size(info);
    gboolean ret = TRUE;
    if(priv->min_size > 0 && size < priv->min_size)
        ret = FALSE;
    else if(priv->max_size > 0 && size > priv->max_size)
        ret = FALSE;
    return ret;
}

gboolean fm_search_job_match_mtime(FmSearchJob* job, GFileInfo* info)
{
    FmSearchJobPrivate* priv = job->priv;
    gboolean ret = TRUE;
    if(priv->date1 || priv->date2)
    {
        guint64 mtime = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
        if(priv->date1 > 0 && mtime < priv->date1)
            ret = FALSE; /* earlier than date1 */
        else if(priv->date2 > 0 && mtime > priv->date2)
            ret = FALSE; /* later than date2 */
    }
    return ret;
}

/* end of rule functions */
