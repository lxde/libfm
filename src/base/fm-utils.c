/*
 *      fm-utils.c
 *
 *      Copyright 2009 - 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *      Copyright 2012 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

char* fm_strdup_replace(char* str, char* old, char* new)
{
    int len = strlen(str);
    char* found;
    GString* buf = g_string_sized_new(len);
    while((found = strstr(str, old)))
    {
        g_string_append_len(buf, str, (found - str));
        g_string_append(buf, new);
        str = found + strlen(old);
    }
    g_string_append(buf, str);
    return g_string_free(buf, FALSE);
}

/**
 * fm_app_command_parse
 * @cmd:        line to parse
 * @opts:       plain list of possible options
 * @ret:        pointer for resulting string, string should be freed by caller
 * @user_data:  caller data to pass to callback
 *
 * This function parses line that contains some %&lt;char&gt; commands and does
 * substitutions on them using callbacks provided by caller.
 *
 * Return value: number of valid options found in @cmd
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
