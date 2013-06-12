/*
 *      fm-utils.c
 *
 *      Copyright 2009 - 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *      Copyright 2012-2013 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

/**
 * SECTION:fm-utils
 * @short_description: Common utility functions used by libfm and libfm-gtk.
 * @title: Common Libfm utilities.
 *
 * @include: libfm/fm-utils.h
 *
 * This scope contains common data parsing utilities.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <libintl.h>
#include <gio/gdesktopappinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fm-utils.h"
#include "fm-file-info-job.h"

#define BI_KiB  ((gdouble)1024.0)
#define BI_MiB  ((gdouble)1024.0 * 1024.0)
#define BI_GiB  ((gdouble)1024.0 * 1024.0 * 1024.0)
#define BI_TiB  ((gdouble)1024.0 * 1024.0 * 1024.0 * 1024.0)

#define SI_KB   ((gdouble)1000.0)
#define SI_MB   ((gdouble)1000.0 * 1000.0)
#define SI_GB   ((gdouble)1000.0 * 1000.0 * 1000.0)
#define SI_TB   ((gdouble)1000.0 * 1000.0 * 1000.0 * 1000.0)

/**
 * fm_file_size_to_str
 * @buf: pointer to array to make a string
 * @buf_size: size of @buf
 * @size: number to convert
 * @si_prefix: %TRUE to convert in SI units, %FALSE to convert in IEC units
 *
 * Converts @size into text representation of form "21.4 kiB" for example.
 *
 * Returns: @buf.
 *
 * Since: 0.1.0
 */
char* fm_file_size_to_str( char* buf, size_t buf_size, goffset size, gboolean si_prefix )
{
    const char * unit;
    gdouble val;

    if( si_prefix ) /* 1000 based SI units */
    {
        if(size < (goffset)SI_KB)
        {
            snprintf(buf, buf_size,
                     dngettext(GETTEXT_PACKAGE, "%u byte", "%u bytes", (gulong)size),
                     (guint)size);
            return buf;
        }
        val = (gdouble)size;
        if(val < SI_MB)
        {
            val /= SI_KB;
            unit = _("KB");
        }
        else if(val < SI_GB)
        {
            val /= SI_MB;
            unit = _("MB");
        }
        else if(val < SI_TB)
        {
            val /= SI_GB;
            unit = _("GB");
        }
        else
        {
            val /= SI_TB;
            unit = _("TB");
        }
    }
    else /* 1024-based binary prefix */
    {
        if(size < (goffset)BI_KiB)
        {
            snprintf(buf, buf_size,
                     dngettext(GETTEXT_PACKAGE, "%u byte", "%u bytes", (gulong)size),
                     (guint)size);
            return buf;
        }
        val = (gdouble)size;
        if(val < BI_MiB)
        {
            val /= BI_KiB;
            unit = _("KiB");
        }
        else if(val < BI_GiB)
        {
            val /= BI_MiB;
            unit = _("MiB");
        }
        else if(val < BI_TiB)
        {
            val /= BI_GiB;
            unit = _("GiB");
        }
        else
        {
            val /= BI_TiB;
            unit = _("TiB");
        }
    }
    snprintf( buf, buf_size, "%.1f %s", val, unit );
    return buf;
}

/**
 * fm_key_file_get_int
 * @kf: a key file
 * @grp: group to lookup key
 * @key: a key to lookup
 * @val: location to store value
 *
 * Lookups @key in @kf and stores found value in @val if the @key was found.
 *
 * Returns: %TRUE if @key was found.
 *
 * Since: 0.1.0
 */
gboolean fm_key_file_get_int(GKeyFile* kf, const char* grp, const char* key, int* val)
{
    char* str = g_key_file_get_value(kf, grp, key, NULL);
    if(G_LIKELY(str))
    {
        *val = atoi(str);
        g_free(str);
    }
    return str != NULL;
}

/**
 * fm_key_file_get_bool
 * @kf: a key file
 * @grp: group to lookup key
 * @key: a key to lookup
 * @val: location to store value
 *
 * Lookups @key in @kf and stores found value in @val if the @key was found.
 *
 * Returns: %TRUE if @key was found.
 *
 * Since: 0.1.0
 */
gboolean fm_key_file_get_bool(GKeyFile* kf, const char* grp, const char* key, gboolean* val)
{
    char* str = g_key_file_get_value(kf, grp, key, NULL);
    if(G_LIKELY(str))
    {
        *val = (str[0] == '1' || str[0] == 't');
        g_free(str);
    }
    return str != NULL;
}

/**
 * fm_canonicalize_filename
 * @filename: a filename
 * @cwd: current work directory path
 *
 * Makes a canonical name with full path from @filename. Returned string
 * should be freed by caller after usage.
 *
 * Returns: (transfer full): a canonical name.
 *
 * Since: 0.1.0
 */
char* fm_canonicalize_filename(const char* filename, const char* cwd)
{
    char* _cwd = NULL;
    int len = strlen(filename);
    int i = 0;
    char* ret = g_malloc(len + 1), *p = ret;
    if(!cwd)
        cwd = _cwd = g_get_current_dir();
    for(; i < len; )
    {
        if(filename[i] == '.')
        {
            if(filename[i+1] == '.' && (filename[i+2] == '/' || filename[i+2] == '\0') ) /* .. */
            {
                if(i == 0) /* .. is first element */
                {
                    int cwd_len;
                    const char* sep;

                    sep = strrchr(cwd, '/');
                    if(sep && sep != cwd)
                        cwd_len = (sep - cwd);
                    else
                        cwd_len = strlen(cwd);
                    ret = g_realloc(ret, len - 2 + cwd_len + 1);
                    memcpy(ret, cwd, cwd_len);
                    p = ret + cwd_len;
                }
                else /* other .. in the path */
                {
                    --p;
                    if(p > ret && *p == '/') /* strip trailing / if it's not root */
                        --p;
                    while(p > ret && *p != '/') /* strip basename */
                        --p;
                    if(*p != '/' || p == ret) /* strip trailing / if it's not root */
                        ++p;
                }
                i += 2;
                continue;
            }
            else if(filename[i+1] == '/' || filename[i+1] == '\0' ) /* . */
            {
                if(i == 0) /* first element */
                {
                    int cwd_len;
                    cwd_len = strlen(cwd);
                    ret = g_realloc(ret, len - 1 + cwd_len + 1);
                    memcpy(ret, cwd, cwd_len);
                    p = ret + cwd_len;
                }
                ++i;
                continue;
            }
        }
        else if(i == 0 && filename[0] != '/') /* relative path without ./ */
        {
            int cwd_len = strlen(cwd);
            ret = g_realloc(ret, len + 1 + cwd_len + 1);
            memcpy(ret, cwd, cwd_len);
            p = ret + cwd_len;
            *p++ = '/';
        }
        for(; i < len; ++p)
        {
            /* prevent duplicated / */
            if(filename[i] == '/' && (p > ret && *(p-1) == '/'))
            {
                ++i;
                break;
            }
            *p = filename[i];
            ++i;
            if(*p == '/')
            {
                ++p;
                break;
            }
        }
    }
    if((p-1) > ret && *(p-1) == '/') /* strip trailing / */
        --p;
    *p = 0;
    if(_cwd)
        g_free(_cwd);
    return ret;
}

/**
 * fm_strdup_replace
 * @str: a string
 * @old_str: substring to replace
 * @new_str: string to replace @old_str
 *
 * Replaces every occurence of @old_str in @str with @new_str and returns
 * resulted string. Returned string should be freed with g_free() after
 * usage.
 *
 * Before 1.0.0 this API had name fm_str_replace.
 *
 * Returns: (transfer full): resulted string.
 *
 * Since: 0.1.16
 */
char* fm_strdup_replace(char* str, char* old_str, char* new_str)
{
    int len = strlen(str);
    char* found;
    GString* buf = g_string_sized_new(len);
    int old_str_len = strlen(old_str);
    while((found = strstr(str, old_str)))
    {
        g_string_append_len(buf, str, (found - str));
        g_string_append(buf, new_str);
        str = found + old_str_len;
    }
    g_string_append(buf, str);
    return g_string_free(buf, FALSE);
}

/**
 * fm_app_command_parse
 * @cmd:        line to parse
 * @opts:       plain list of possible options
 * @ret:        (out) (transfer full): pointer for resulting string, string should be freed by caller
 * @user_data:  caller data to pass to callback
 *
 * This function parses line that contains some %&lt;char&gt; commands and does
 * substitutions on them using callbacks provided by caller.
 *
 * Returns: number of valid options found in @cmd.
 *
 * Since: 1.0.0
 */
int fm_app_command_parse(const char* cmd, const FmAppCommandParseOption* opts,
                         char** ret, gpointer user_data)
{
    const char* ptr = cmd, *c, *ins;
    GString* buf = g_string_sized_new(256);
    const FmAppCommandParseOption* opt;
    int hits = 0;

    for(c = ptr; *c; c++)
    {
        if(*c == '%')
        {
            if(c[1] == '\0')
                break;
            if(c != ptr)
                g_string_append_len(buf, ptr, c - ptr);
            ++c;
            ptr = c + 1;
            if(*c == '%') /* subst "%%" as "%" */
            {
                g_string_append_c(buf, '%');
                continue;
            }
            if(!opts) /* no options available? */
                continue;
            for(opt = opts; opt->opt; opt++)
            {
                if(opt->opt == *c)
                {
                    hits++;
                    if(opt->callback)
                    {
                        ins = opt->callback(*c, user_data);
                        if(ins && *ins)
                            g_string_append(buf, ins);
                        /* FIXME: add support for uri and escaping */
                    }
                    break;
                }
            }
            /* FIXME: should invalid options be passed 'as is' or ignored? */
        }
    }
    if(c != ptr)
        g_string_append_len(buf, ptr, c - ptr);
    *ret = g_string_free(buf, FALSE);
    return hits;
}


/* ---- run menu cache in main context ---- */
/* Lock for main context run. */
#if GLIB_CHECK_VERSION(2, 32, 0)
static GMutex main_loop_run_mutex;
static GCond main_loop_run_cond;
#else
G_LOCK_DEFINE(main_loop_run_mutex);
static GMutex *main_loop_run_mutex = NULL;
static GCond *main_loop_run_cond = NULL;
#endif

typedef struct
{
    gboolean done;
    GSourceFunc func;
    gpointer data;
    gboolean result;
} _main_context_data;

static gboolean _fm_run_in_default_main_context_real(gpointer user_data)
{
    _main_context_data *data = user_data;
    data->result = data->func(data->data);
#if GLIB_CHECK_VERSION(2, 32, 0)
    g_mutex_lock(&main_loop_run_mutex);
    data->done = TRUE;
    g_cond_broadcast(&main_loop_run_cond);
    g_mutex_unlock(&main_loop_run_mutex);
#else
    g_mutex_lock(main_loop_run_mutex);
    data->done = TRUE;
    g_cond_broadcast(main_loop_run_cond);
    g_mutex_unlock(main_loop_run_mutex);
#endif
    return FALSE;
}

/**
 * fm_run_in_default_main_context
 * @func: function to run
 * @data: data supplied for @func
 *
 * Runs @func once in global main loop with supplied @data.
 *
 * Returns: output of @func.
 *
 * Since: 1.0.2
 */
gboolean fm_run_in_default_main_context(GSourceFunc func, gpointer data)
{
    _main_context_data md;

#if GLIB_CHECK_VERSION(2, 32, 0)
    md.done = FALSE;
    md.func = func;
    md.data = data;
    g_main_context_invoke(NULL, _fm_run_in_default_main_context_real, &md);
    g_mutex_lock(&main_loop_run_mutex);
    while(!md.done)
        g_cond_wait(&main_loop_run_cond, &main_loop_run_mutex);
    g_mutex_unlock(&main_loop_run_mutex);
#else
    /* if we already in main loop then just run it */
    if(g_main_context_is_owner(g_main_context_default()))
        md.result = func(data);
    /* if we can acquire context then do it */
    else if(g_main_context_acquire(g_main_context_default()))
    {
        md.result = func(data);
        g_main_context_release(g_main_context_default());
    }
    /* else add idle source and wait for return */
    else
    {
        G_LOCK(main_loop_run_mutex);
        if(!main_loop_run_mutex)
            main_loop_run_mutex = g_mutex_new();
        if(!main_loop_run_cond)
            main_loop_run_cond = g_cond_new();
        G_UNLOCK(main_loop_run_mutex);
        md.done = FALSE;
        md.func = func;
        md.data = data;
        g_idle_add(_fm_run_in_default_main_context_real, &md);
        g_mutex_lock(main_loop_run_mutex);
        while(!md.done)
            g_cond_wait(main_loop_run_cond, main_loop_run_mutex);
        g_mutex_unlock(main_loop_run_mutex);
    }
#endif
    return md.result;
}

/**
 * fm_get_home_dir
 *
 * Retrieves valid path to home dir of user.
 *
 * Returns: path string.
 *
 * Since: 1.0.2
 */
const char *fm_get_home_dir(void)
{
    /* From GLib docs for g_get_home_dir */
    const char *homedir = g_getenv("HOME");
    if(!homedir)
        homedir = g_get_home_dir();
    return homedir;
}
