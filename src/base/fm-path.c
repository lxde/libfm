/*
 *      fm-path.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-path.h"
#include "fm-file-info.h"
#include <string.h>
#include <limits.h>
#include <glib/gi18n-lib.h>

static FmPath* root_path = NULL;

static char* home_dir = NULL;
static int home_len = 0;
static FmPath* home_path = NULL;

static FmPath* desktop_path = NULL;
static char* desktop_dir = NULL;
static int desktop_len = 0;

static FmPath* trash_root_path = NULL;
/*defined but not used
static FmPath* network_root = NULL;*/

static FmPath* apps_root_path = NULL;

static FmPath* _fm_path_alloc(FmPath* parent, int name_len, int flags)
{
    FmPath* path;
    path = (FmPath*)g_malloc(sizeof(FmPath) + name_len);
    path->n_ref = 1;
    path->flags = flags;
    path->parent = parent ? fm_path_ref(parent) : NULL;
    path->flags = flags;
    return path;
}

static inline FmPath* _fm_path_new_internal(FmPath* parent, const char* name, int name_len, int flags)
{
    FmPath* path = _fm_path_alloc(parent, name_len, flags);
    memcpy(path->name, name, name_len);
    path->name[name_len] = '\0';
    return path;
}

/**
 * _fm_path_new_uri_root
 * @uri: the uri in the form scheme://user@host/remaining/path
 * @len: length of the uri
 * @remaining: retrive the remaining path
 *
 * This function create a root FmpPath element for scheme://user@host/,
 * and return the remaining part in @remaining.
 */
static FmPath* _fm_path_new_uri_root(const char* uri, int len, const char** remaining, gboolean need_unescape)
{
    FmPath* path;
    char* buf;
    const char* uri_end = uri + len;
    const char* host;
    const char* host_end;
    char* unescaped_host = NULL;
    int scheme_len, host_len;
    int flags;

    /* A generic URI: scheme://user@host/remaining/path */
    if(!g_ascii_isalpha(uri[0])) /* invalid */
        goto on_error;

    for(scheme_len = 1; scheme_len < len && uri[scheme_len] != ':';) /* find the : after scheme */
    {
        char ch = uri[scheme_len];
        if(g_ascii_isalnum(ch) || ch == '+' || ch == '-' || ch == '.') /* valid scheme characters */
            ++scheme_len;
        else /* this is not a valid URI */
            goto on_error;
    }

    if(scheme_len == len) /* there is no : in the URI, it's invalid */
        goto on_error;

    /* now there should be // following :, or no slashes at all, such as mailto:someone@somewhere.net */
    host = uri + scheme_len + 1;
    while(*host == '/' && host < uri_end) /* skip the slashes */
        ++host;

    flags = 0;
    host_len = 0;
    if(scheme_len == 4 && g_ascii_strncasecmp(uri, "file", 4) == 0) /* handles file:/// */
    {
        if(remaining)
            *remaining = host;
        return fm_path_ref(root_path);
    }
    /* special handling for some known schemes */
    else if(scheme_len == 5 && g_ascii_strncasecmp(uri, "trash", 5) == 0) /* trashed files are on local filesystems */
    {
        if(remaining)
            *remaining = host;
        return fm_path_ref(trash_root_path);
    }
    else if(scheme_len == 8 && g_ascii_strncasecmp(uri, "computer", 8) == 0)
    {
        flags |= FM_PATH_IS_VIRTUAL;
        host_end = host;
    }
    else if(scheme_len == 7 && g_ascii_strncasecmp(uri, "network", 7) == 0)
    {
        flags |= FM_PATH_IS_VIRTUAL;
        host_end = host;
    }
    else if(scheme_len == 6 && g_ascii_strncasecmp(uri, "mailto", 6) == 0)
    {
        /* is any special handling needed? */
        if(remaining)
            *remaining = uri_end;
        if(need_unescape)
        {
            unescaped_host = g_uri_unescape_string(uri, NULL);
            path = _fm_path_new_internal(NULL, unescaped_host, strlen(unescaped_host), 0);
            g_free(unescaped_host);
        }
        else
            path = _fm_path_new_internal(NULL, uri, len, 0);
        return path;
    }
    else /* it's a normal remote URI */
    {
        /* now we're at username@hostname, the authenticaion part */
        host_end = host;
        while(host_end < uri_end && *host_end != '/') /* find the end of host name */
            ++host_end;
        host_len = (host_end - host);
        if(scheme_len == 4 && g_ascii_strncasecmp(uri, "menu", 4) == 0)
        {
            if(host_len == 0) /* fallback to applications */
            {
                host = "applications";
                host_len = 12;
                if(remaining)
                    *remaining = uri_end;
                return fm_path_ref(apps_root_path);
            }
            else if(host_len == 12 && strncmp(host, "applications", 12) == 0)
            {
                if(remaining)
                    *remaining = host_end;
                return fm_path_ref(apps_root_path);
            }
            flags |= (FM_PATH_IS_VIRTUAL|FM_PATH_IS_XDG_MENU);
        }

        if(need_unescape)
        {
            unescaped_host = g_uri_unescape_segment(host, host_end, NULL);
            host = unescaped_host;
            host_len = strlen(host);
        }
    }

    if(remaining)
        *remaining = host_end;

    /* it's reasonable to have double slashes :// for URIs other than mailto: */
    len = scheme_len + 3 + host_len + 1;
    path = _fm_path_alloc(NULL, len, flags);
    buf = path->name;
    memcpy(buf, uri, scheme_len); /* the scheme */
    buf += scheme_len;
    memcpy(buf, "://", 3); /* :// */
    buf += 3;
    if(host_len > 0) /* host name */
    {
        memcpy(buf, host, host_len);
        buf += host_len;
    }
    buf[0] = '/'; /* the trailing / */
    buf[1] = '\0';

    g_free(unescaped_host);

    return path;

on_error: /* this is not a valid URI */
    /* FIXME: should we return root or NULL? */
    if(remaining)
        *remaining = uri + len;
    return fm_path_ref(root_path);
}

#if 0
static inline FmPath* _fm_path_reuse_existing_paths(FmPath* parent, const char* basename, int name_len)
{
    FmPath* current;
    /* This is a way to reuse cached FmPath objects created for $HOME and desktop dir.
     * Since most of the files a user may use are under $HOME, reusing this can
     * more or less reduce memory usage. However, this may slow things down a little. */
    for(current = desktop_path; current; current = current->parent)
    {
        if(fm_path_equal(current->parent, parent))
        {
            if(strncmp(basename, current->name, name_len) == 0 && current->name[name_len] == '\0')
                return fm_path_ref(current);
            break;
        }
    }
    return NULL;
}
#endif

/**
 * fm_path_new_child_len
 * @parent: a parent path
 * @basename: basename of a direct child of @parent directory in glib
 * filename encoding. (can be non-UTF-8).
 * @len: length of basename
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
FmPath* fm_path_new_child_len(FmPath* parent, const char* basename, int name_len)
{
    FmPath* path;
    gboolean append_slash = FALSE;
    int flags;

    /* skip empty basename */
    if(G_UNLIKELY(!basename || name_len == 0))
        return parent ? fm_path_ref(path) : NULL;

    if(G_LIKELY(parent)) /* remove slashes if needed. */
    {
        #if 0 /* This saves some memory, but it's too inefficient. */
        /* Optimization: reuse existing FmPaths */
        if(fm_path_is_native(parent))
        {
            path = _fm_path_reuse_existing_paths(parent, basename, name_len);
            if(G_UNLIKELY(path))
                return path;
        }
        #endif

        flags = parent->flags; /* inherit flags of parent */
        while(basename[0] == '/')
        {
            ++basename;
            --name_len;
        }
        while(name_len > 0 && basename[name_len-1] == '/')
            --name_len;

        /* special case for . and .. */
        if(basename[0] == '.' && (name_len == 1 || (name_len == 2 && basename[1] == '.')))
        {
            if(name_len == 1) /* . */
                return parent ? fm_path_ref(parent) : NULL;
            else /* .. */
            {
                if(parent)
                    return parent->parent ? fm_path_ref(parent->parent) : fm_path_ref(parent);
                else
                    return NULL;
            }
        }
    }
    else /* this basename is root of the fs (no parent), it can be "/" or something like "ftp://user@host/" */
    {
        /* remove duplicated leading slashes and treat them as one / */
        if(G_UNLIKELY(basename[0] == '/')) /* this is a posix path */
            return fm_path_ref(root_path);
        else /* This is something like: trash:///, computer:/// sftp://user@host/ */
            return _fm_path_new_uri_root(basename, name_len, NULL, FALSE);
    }

    /* remove tailing slashes */
    while(name_len > 0 && basename[name_len-1] == '/')
        --name_len;

    if(name_len == 0)
        return parent ? fm_path_ref(parent) : NULL;

    path = _fm_path_alloc(parent, (G_UNLIKELY(append_slash) ? name_len + 1 : name_len), flags);
    memcpy(path->name, basename, name_len);
    if(G_UNLIKELY(append_slash))
    {
        path->name[name_len] = '/';
        path->name[name_len + 1] = '\0';
    }
    else
        path->name[name_len] = '\0';
    return path;
}

/**
 * fm_path_new_child
 * @parent: a parent path
 * @basename: basename of a direct child of @parent directory in glib
 * filename encoding. (can be non-UTF-8).
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
FmPath* fm_path_new_child(FmPath* parent, const char* basename)
{
    if(G_LIKELY(basename && *basename))
    {
        int baselen = strlen(basename);
        return fm_path_new_child_len(parent, basename, baselen);
    }
    return G_LIKELY(parent) ? fm_path_ref(parent) : NULL;
}

/**
 * fm_path_new_for_gfile
 * @gf: a GFile object
 *
 * This function converts a GFile object to FmPath.
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
FmPath* fm_path_new_for_gfile(GFile* gf)
{
    FmPath* path;
    char* str;
    if( g_file_is_native(gf) )
    {
        str = g_file_get_path(gf);
        path = fm_path_new_for_path(str);
    }
    else
    {
        str = g_file_get_uri(gf);
        path = fm_path_new_for_uri(str);
    }
    g_free(str);
    return path;
}

/**
 * fm_path_new_relative
 * @parent: a parent path
 * @rel: a path relative to @parent in glib filename encoding. (can be
 * non-UTF-8). However this should not be a escaped ASCII string used in
 * URI. If you're building a relative path for a URI, and the relative
 * path is escaped, you have to unescape it first.
 *
 * For example, if @parent is "http://wiki.lxde.org/" and @rel is
 * "zh/%E9%A6%96%E9%A0%81", you have to unescape the relative path
 * prior to passing it to fm_path_new_relative().
 *
 * If @parent is NULL, this works the same as fm_path_new_for_str(@rel)
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
FmPath* fm_path_new_relative(FmPath* parent, const char* rel)
{
    FmPath* path;
    if(G_UNLIKELY(!rel || !*rel)) /* relative path is empty */
        return parent ? fm_path_ref(parent) : fm_path_ref(root_path); /* return parent */

    if(G_LIKELY(parent))
    {
        char* sep;
        /* remove leading slashes */
        while(*rel == '/')
            ++rel;
        if(!*rel)
            path = fm_path_ref(parent);
        else
        {
#if 0       /* FIXME: Let's optimize this later. Make things working first is more important. */
            /* use some pre-defined paths when possible */
            if(G_UNLIKELY(parent == root_path))
            {
                if(strcmp(home_dir + 1, rel) == 0)
                    return fm_path_ref(home_path);
                if(strcmp(desktop_dir + 1, rel) == 0)
                    return fm_path_ref(desktop_dir);
            }
#endif

            sep = strchr(rel, '/');
            if(sep)
            {
                FmPath* new_parent = fm_path_new_child_len(parent, rel, sep - rel);
                path = fm_path_new_relative(new_parent, sep + 1);
                fm_path_unref(new_parent);
            }
            else
            {
                path = fm_path_new_child(parent, rel);
            }
        }
    }
    else /* this is actaully a full path */
        path = fm_path_new_for_str(rel);
    return path;
}

/**
 * fm_path_new_for_path
 * @path_name: a POSIX path.
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
FmPath* fm_path_new_for_path(const char* path_name)
{
    FmPath* path;
    if(!path_name || !*path_name)
        return fm_path_ref(root_path);

    /* some special cases */
    if(G_LIKELY(path_name[0] == '/'))
    {
        if(G_UNLIKELY(path_name[1] == '\0')) /* pathname is / */
            path = fm_path_ref(root_path);
        else
            path = fm_path_new_relative(root_path, path_name + 1);
    }
    else /* pathname should be absolute path. otherwise its invalid */
        path = fm_path_ref(root_path); /* return root */
    return path;
}

/**
 * fm_path_new_for_uri
 * @path_name: a URI with special characters escaped.
 * Encoded URI such as http://wiki.lxde.org/zh/%E9%A6%96%E9%A0%81
 * will be unescaped.
 *
 * You can call fm_path_to_uri() to convert a FmPath to a escaped URI
 * string.
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
static FmPath* _fm_path_new_for_uri_internal(const char* uri, gboolean need_unescape)
{
    FmPath* path, *root;
    char* rel_path;
    if(!uri || !*uri)
        return fm_path_ref(root_path);

    root = _fm_path_new_uri_root(uri, strlen(uri), &rel_path, need_unescape);
    if(*rel_path)
    {
        if(need_unescape)
            rel_path = g_uri_unescape_string(rel_path, NULL);
        path = fm_path_new_relative(root, rel_path);
        fm_path_unref(root);
        if(need_unescape)
            g_free((char*)rel_path);
    }
    else
        path = root;
    return path;
}

/**
 * fm_path_new_for_uri
 * @path_name: a URI with special characters escaped.
 * Encoded URI such as http://wiki.lxde.org/zh/%E9%A6%96%E9%A0%81
 * will be unescaped.
 *
 * You can call fm_path_to_uri() to convert a FmPath to a escaped URI
 * string.
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
FmPath* fm_path_new_for_uri(const char* uri)
{
    return _fm_path_new_for_uri_internal(uri, TRUE);
}

/**
 * fm_path_new_for_display_name
 * @path_name: a UTF-8 encoded display name for the path
 * It can either be a POSIX path in UTF-8 encoding, or an unescaped URI
 * (can contain non-ASCII characters and spaces)
 *
 * You can call fm_path_display_name() to convert a FmPath to a
 * UTF-8 encoded name ready for being displayed in the GUI.
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
FmPath* fm_path_new_for_display_name(const char* path_name)
{
    FmPath* path;
    if(!path_name || !*path_name || (path_name[0]=='/' && path_name[1] == '\0') )
        return fm_path_ref(root_path);
    if(path_name[0] == '/') /* native path */
    {
        char* filename = g_filename_from_utf8(path_name, -1, NULL, NULL, NULL);
        if(filename) /* convert from utf-8 to local encoding */
        {
            path = fm_path_new_for_path(filename);
            g_free(filename);
        }
        else
            path = fm_path_ref(root_path);
    }
    else /* this is an URI */
    {
        /* UTF-8 should be allowed, I think. */
        path = _fm_path_new_for_uri_internal(path_name, FALSE);
    }
    return path;
}

/**
 * fm_path_new_for_str
 * @path_str: a string representing the file path in its native
 * encoding (can be non-UTF-8). It can either be a native path or an
 * unescaped URI (can contain non-ASCII characters and spaces).
 * The function will try to figure out what to do.
 *
 * You can call fm_path_to_str() to convert a FmPath back to its string
 * presentation.
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
FmPath* fm_path_new_for_str(const char* path_str)
{
    if(!path_str || !*path_str)
        return fm_path_ref(root_path);
    if(path_str[0] == '/')
        return fm_path_new_for_path(path_str);
    /* UTF-8 should be allowed, I think. */
    return _fm_path_new_for_uri_internal(path_str, FALSE);
}

/**
 * fm_path_new_for_commandline_arg
 * @arg: a file path passed in command line argv to the program. The @arg
 * can be a POSIX path in glib filename encoding (can be non-UTTF-8) and
 * can be a URI with non-ASCII characters escaped, like
 * http://wiki.lxde.org/zh/%E9%A6%96%E9%A0%81.
 *
 * Returns: a newly created FmPath for the path. You have to call
 * fm_path_unref() when it's no longer needed.
 */
FmPath* fm_path_new_for_commandline_arg(const char* arg)
{
    if(!arg || !*arg || (arg[0]=='/' && arg[1] == '\0'))
        return fm_path_ref(root_path);
    if(arg[0] == '/')
        return fm_path_new_for_path(arg);
    return _fm_path_new_for_uri_internal(arg, TRUE);
}


FmPath* fm_path_ref(FmPath* path)
{
    g_atomic_int_inc(&path->n_ref);
    return path;
}

void fm_path_unref(FmPath* path)
{
    /* g_debug("fm_path_unref: %s, n_ref = %d", fm_path_to_str(path), path->n_ref); */
    if(g_atomic_int_dec_and_test(&path->n_ref))
    {
        if(G_LIKELY(path->parent))
            fm_path_unref(path->parent);
        g_free(path);
    }
}

FmPath* fm_path_get_parent(FmPath* path)
{
    return path->parent;
}

const char* fm_path_get_basename(FmPath* path)
{
    return path->name;
}

FmPathFlags fm_path_get_flags(FmPath* path)
{
    return path->flags;
}

/**
 * fm_path_has_prefix
 * @path: a sub path
 * @prefix: a prefix
 *
 * Check if @prefix is a prefix of @path.
 * For example: /usr/share is the prefix of /usr/share/docs/libfm
 * but /etc is not.
 *
 * Returns: TRUE if @prefix is the prefix of @path.
 */
gboolean fm_path_has_prefix(FmPath* path, FmPath* prefix)
{
    for(; path; path = path->parent)
    {
        if(fm_path_equal(path, prefix))
            return TRUE;
    }
    return FALSE;
}

static int fm_path_strlen(FmPath* path)
{
    int len = 0;
    for(;;)
    {
        len += strlen(path->name);
        if(G_UNLIKELY(!path->parent ))
            break;
        if(path->parent->parent)
            ++len; /* add a character for separator */
        path = path->parent;
    }
    return len;
}

/* recursive internal implem. of fm_path_to_str returns end of current
   build string */
static gchar* fm_path_to_str_int(FmPath* path, gchar** ret, gint str_len)
{
    gint name_len = strlen(path->name);
    gchar* pbuf;

    if (!path->parent)
    {
        *ret = g_new0(gchar, str_len + name_len + 1 );
        pbuf = *ret;
    }
    else
    {
        pbuf = fm_path_to_str_int( path->parent, ret, str_len + name_len + 1 );
        if (path->parent->parent) /* if parent dir is not root_path */
            *pbuf++ = G_DIR_SEPARATOR;
    }
    memcpy( pbuf, path->name, name_len );
    return pbuf + name_len;
}

/* FIXME: handle display name and real file name (maybe non-UTF8) issue */
char* fm_path_to_str(FmPath* path)
{
    gchar *ret;
    fm_path_to_str_int( path, &ret, 0 );
    return ret;
}

char* fm_path_to_uri(FmPath* path)
{
    char* uri = NULL;
    char* str = fm_path_to_str(path);
    if( G_LIKELY(str) )
    {
        if(str[0] == '/') /* absolute path */
            uri = g_filename_to_uri(str, NULL, NULL);
        else
        {
            /* FIXME: is this correct? */
            uri = g_uri_escape_string(str, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
        }
        g_free(str);
    }
    return uri;
}

/* FIXME: maybe we can support different encoding for different mount points? */
char* fm_path_display_name(FmPath* path, gboolean human_readable)
{
    char* disp;
    if(human_readable)
    {
        if(G_LIKELY(path->parent))
        {
            char* disp_parent = fm_path_display_name(path->parent, TRUE);
            char* disp_base = fm_path_display_basename(path);
            disp = g_build_filename( disp_parent, disp_base, NULL);
            g_free(disp_parent);
            g_free(disp_base);
        }
        else
            disp = fm_path_display_basename(path);
    }
    else
    {
        char* str = fm_path_to_str(path);
        disp = g_filename_display_name(str);
        g_free(str);
    }
    return disp;
}

/* FIXME: maybe we can support different encoding for different mount points? */
char* fm_path_display_basename(FmPath* path)
{
    if(G_UNLIKELY(!path->parent)) /* root_path element */
    {
        if( !fm_path_is_native(path) && fm_path_is_virtual(path) )
        {
            if(fm_path_is_trash_root(path))
                return g_strdup(_("Trash Can"));
            if(g_str_has_prefix(path->name, "computer:/"))
                return g_strdup(_("My Computer"));
            if(g_str_has_prefix(path->name, "menu:/"))
            {
                /* FIXME: this should be more flexible */
                const char* p = path->name + 5;
                while(p == '/')
                    ++p;
                if(g_str_has_prefix(p, "applications.menu"))
                    return g_strdup(_("Applications"));
            }
            if(g_str_has_prefix(path->name, "network:/"))
                return g_strdup(_("Network"));
        }
    }
    return g_filename_display_name(path->name);
}

GFile* fm_path_to_gfile(FmPath* path)
{
    GFile* gf;
    char* str;
    str = fm_path_to_str(path);
    if(fm_path_is_native(path))
        gf = g_file_new_for_path(str);
    else
        gf = g_file_new_for_uri(str);
    g_free(str);
    return gf;
}

FmPath* fm_path_get_root()
{
    return root_path;
}

FmPath* fm_path_get_home()
{
    return home_path;
}

FmPath* fm_path_get_desktop()
{
    return desktop_path;
}

FmPath* fm_path_get_trash()
{
    return trash_root_path;
}

FmPath* fm_path_get_apps_menu()
{
    return apps_root_path;
}

void _fm_path_init()
{
    const char* sep, *name;
    FmPath* tmp, *parent;

    /* path object of root_path dir */
    root_path = _fm_path_new_internal(NULL, "/", 1, FM_PATH_IS_LOCAL|FM_PATH_IS_NATIVE);
    home_dir = g_get_home_dir();
    home_len = strlen(home_dir);
    while(home_dir[home_len - 1] == '/')
        --home_len;

    /* build path object for home dir */
    name = home_dir + 1; /* skip leading / */
    parent = root_path;
    while( sep = strchr(name, '/') )
    {
        int len = (sep - name);
        if(len > 0)
        {
            /* ref counting is not a problem here since this path component
             * will exist till the termination of the program. So mem leak is ok. */
            tmp = _fm_path_new_internal(parent, name, len, FM_PATH_IS_LOCAL|FM_PATH_IS_NATIVE);
            parent = tmp;
        }
        name = sep + 1;
    }
    home_path = _fm_path_new_internal(parent, name, strlen(name), FM_PATH_IS_LOCAL|FM_PATH_IS_NATIVE);

    desktop_dir = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
    desktop_len = strlen(desktop_dir);
    while(desktop_dir[desktop_len - 1] == '/')
        --desktop_len;

    /* build path object for desktop_path dir */
    name = desktop_dir + home_len + 1; /* skip home_path dir part / */
    parent = home_path;
    while( sep = strchr(name, '/') )
    {
        int len = (sep - name);
        if(len > 0)
        {
            /* ref counting is not a problem here since this path component
             * will exist till the termination of the program. So mem leak is ok. */
            tmp = _fm_path_new_internal(parent, name, len, FM_PATH_IS_LOCAL|FM_PATH_IS_NATIVE);
            parent = tmp;
        }
        name = sep + 1;
    }
    desktop_path = _fm_path_new_internal(parent, name, strlen(name), FM_PATH_IS_LOCAL|FM_PATH_IS_NATIVE);

    /* build path object for trash can */
    /* FIXME: currently there are problems with URIs. using trash:/ here will cause problems. */
    trash_root_path = _fm_path_new_internal(NULL, "trash:///", 9, FM_PATH_IS_TRASH|FM_PATH_IS_VIRTUAL|FM_PATH_IS_LOCAL);
    apps_root_path = _fm_path_new_internal(NULL, "menu://applications/", 20, FM_PATH_IS_VIRTUAL|FM_PATH_IS_XDG_MENU);
}


/* For used in hash tables */

/* FIXME: is this good enough? */
guint fm_path_hash(FmPath* path)
{
    guint hash = g_str_hash(path->name);
    if(path->parent)
    {
        /* this is learned from g_str_hash() of glib. */
        hash = (hash << 5) - hash + '/';
        /* this is learned from g_icon_hash() of gio. */
        hash ^= fm_path_hash(path->parent);
    }
    return hash;
}

gboolean fm_path_equal(FmPath* p1, FmPath* p2)
{
    if(p1 == p2)
        return TRUE;
    if(!p1)
        return !p2 ? TRUE:FALSE;
    if(!p2)
        return !p1 ? TRUE:FALSE;
    if( strcmp(p1->name, p2->name) != 0 )
        return FALSE;
    return fm_path_equal( p1->parent, p2->parent);
}

/* Check if this path contains absolute pathname str*/
gboolean fm_path_equal_str(FmPath *path, const gchar *str, int n)
{
    const gchar *last_part;

    if(G_UNLIKELY(!path))
        return FALSE;

    /* default compare str len */
    if (n == -1)
        n = strlen( str );

    /* end of recursion */
    if ((path->parent == NULL) && g_str_equal ( path->name, "/" ) && n == 0 )
        return TRUE;

    /* must also contain leading slash */
    if (n < (strlen(path->name) + 1))
        return FALSE;

    /* check for current part mismatch */
    last_part  = str + n - strlen(path->name) - 1;
    if ( strncmp( last_part + 1, path->name, strlen(path->name)) != 0 )
        return FALSE;
    if ( *last_part != G_DIR_SEPARATOR )
        return FALSE;

    /* tail-end recursion */
    return fm_path_equal_str( path->parent, str, n - strlen(path->name) - 1 );
}

/* calculate how many elements are in this path. */
int fm_path_depth(FmPath* path)
{
    int depth = 1;
    while(path->parent)
    {
        ++depth;
        path = path->parent;
    }
    return depth;
}


/* path list */

static FmListFuncs funcs =
{
    fm_path_ref,
    fm_path_unref
};

FmPathList* fm_path_list_new()
{
    return (FmPathList*)fm_list_new(&funcs);
}

gboolean fm_list_is_path_list(FmList* list)
{
    return list->funcs == &funcs;
}

FmPathList* fm_path_list_new_from_uris(const char** uris)
{
    const char** uri;
    FmPathList* pl = fm_path_list_new();
    for(uri = uris; *uri; ++uri)
    {
        const char* puri = *uri;
        if(puri[0] != '\0') /* ensure that it's not an empty string */
        {
            FmPath* path;
            if(puri[0] == '/')
                path = fm_path_new_for_path(puri);
            else if(strstr(puri, "://"))
                path = fm_path_new_for_uri(puri);
            else /* it's not a valid path or URI */
                continue;
            fm_list_push_tail_noref(pl, path);
        }
    }
    return pl;
}

FmPathList* fm_path_list_new_from_uri_list(const char* uri_list)
{
    char** uris = g_strsplit(uri_list, "\r\n", -1);
    FmPathList* pl = fm_path_list_new_from_uris((const char **)uris);
    g_strfreev(uris);
    return pl;
}

char* fm_path_list_to_uri_list(FmPathList* pl)
{
    GString* buf = g_string_sized_new(4096);
    fm_path_list_write_uri_list(pl, buf);
    return g_string_free(buf, FALSE);
}

/*
char** fm_path_list_to_uris(FmPathList* pl)
{
    if( G_LIKELY(!fm_list_is_empty(pl)) )
    {
        GList* l = fm_list_peek_head_link(pl);
        char** uris = g_new0(char*, fm_list_get_length(pl) + 1);
        for(i=0; l; ++i, l=l->next)
        {
            FmFileInfo* fi = (FmFileInfo*)l->data;
            FmPath* path = fi->path;
            char* uri = fm_path_to_uri(path);
            uris[i] = uri;
        }
    }
    return NULL;
}
*/

FmPathList* fm_path_list_new_from_file_info_list(FmFileInfoList* fis)
{
    FmPathList* list = fm_path_list_new();
    GList* l;
    for(l=fm_list_peek_head_link(fis);l;l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_list_push_tail(list, fi->path);
    }
    return list;
}

FmPathList* fm_path_list_new_from_file_info_glist(GList* fis)
{
    FmPathList* list = fm_path_list_new();
    GList* l;
    for(l=fis;l;l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_list_push_tail(list, fi->path);
    }
    return list;
}

FmPathList* fm_path_list_new_from_file_info_gslist(GSList* fis)
{
    FmPathList* list = fm_path_list_new();
    GSList* l;
    for(l=fis;l;l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_list_push_tail(list, fi->path);
    }
    return list;
}

void fm_path_list_write_uri_list(FmPathList* pl, GString* buf)
{
    GList* l;
    for(l = fm_list_peek_head_link(pl); l; l=l->next)
    {
        FmPath* path = (FmPath*)l->data;
        char* uri = fm_path_to_uri(path);
        g_string_append(buf, uri);
        g_free(uri);
        if(l->next)
            g_string_append(buf, "\r\n");
    }
}
