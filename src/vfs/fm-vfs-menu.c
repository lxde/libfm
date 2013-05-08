/*
 *      fm-vfs-menu.c
 *      VFS for "menu://applications/" path using menu-cache library.
 *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-vfs-menu.h"
#include "glib-compat.h"

#include <glib/gi18n-lib.h>
#include <menu-cache/menu-cache.h>
#include "fm-utils.h"

/* support for libmenu-cache 0.4.x */
#ifndef MENU_CACHE_CHECK_VERSION
# ifdef HAVE_MENU_CACHE_DIR_LIST_CHILDREN
#  define MENU_CACHE_CHECK_VERSION(_a,_b,_c) (_a == 0 && _b < 5) /* < 0.5.0 */
# else
#  define MENU_CACHE_CHECK_VERSION(_a,_b,_c) 0 /* not even 0.4.0 */
# endif
#endif

/* beforehand declarations */
static GFile *_fm_vfs_menu_new_for_uri(const char *uri);


/* ---- FmMenuVFile class ---- */
#define FM_TYPE_MENU_VFILE             (fm_vfs_menu_file_get_type())
#define FM_MENU_VFILE(o)               (G_TYPE_CHECK_INSTANCE_CAST((o), \
                                        FM_TYPE_MENU_VFILE, FmMenuVFile))

typedef struct _FmMenuVFile             FmMenuVFile;
typedef struct _FmMenuVFileClass        FmMenuVFileClass;

static GType fm_vfs_menu_file_get_type  (void);

struct _FmMenuVFile
{
    GObject parent_object;

    char *path;
};

struct _FmMenuVFileClass
{
  GObjectClass parent_class;
};

static void fm_menu_g_file_init(GFileIface *iface);
static void fm_menu_fm_file_init(FmFileInterface *iface);

G_DEFINE_TYPE_WITH_CODE(FmMenuVFile, fm_vfs_menu_file, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(G_TYPE_FILE, fm_menu_g_file_init)
                        G_IMPLEMENT_INTERFACE(FM_TYPE_FILE, fm_menu_fm_file_init))

static void fm_vfs_menu_file_finalize(GObject *object)
{
    FmMenuVFile *item = FM_MENU_VFILE(object);

    g_free(item->path);

    G_OBJECT_CLASS(fm_vfs_menu_file_parent_class)->finalize(object);
}

static void fm_vfs_menu_file_class_init(FmMenuVFileClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = fm_vfs_menu_file_finalize;
}

static void fm_vfs_menu_file_init(FmMenuVFile *item)
{
    /* nothing */
}

static FmMenuVFile *_fm_menu_vfile_new(void)
{
    return (FmMenuVFile*)g_object_new(FM_TYPE_MENU_VFILE, NULL);
}


/* ---- menu enumerator class ---- */
#define FM_TYPE_VFS_MENU_ENUMERATOR        (fm_vfs_menu_enumerator_get_type())
#define FM_VFS_MENU_ENUMERATOR(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), \
                            FM_TYPE_VFS_MENU_ENUMERATOR, FmVfsMenuEnumerator))

typedef struct _FmVfsMenuEnumerator         FmVfsMenuEnumerator;
typedef struct _FmVfsMenuEnumeratorClass    FmVfsMenuEnumeratorClass;

struct _FmVfsMenuEnumerator
{
    GFileEnumerator parent;

    MenuCache *mc;
    GSList *child;
    guint32 de_flag;
};

struct _FmVfsMenuEnumeratorClass
{
    GFileEnumeratorClass parent_class;
};

static GType fm_vfs_menu_enumerator_get_type   (void);

G_DEFINE_TYPE(FmVfsMenuEnumerator, fm_vfs_menu_enumerator, G_TYPE_FILE_ENUMERATOR)

static void _fm_vfs_menu_enumerator_dispose(GObject *object)
{
    FmVfsMenuEnumerator *enu = FM_VFS_MENU_ENUMERATOR(object);

    if(enu->mc)
    {
        menu_cache_unref(enu->mc);
        enu->mc = NULL;
    }

    G_OBJECT_CLASS(fm_vfs_menu_enumerator_parent_class)->dispose(object);
}

static GFileInfo *_g_file_info_from_menu_cache_item(MenuCacheItem *item)
{
    GFileInfo *fileinfo = g_file_info_new();
    const char *icon_name;
    GIcon* icon;

    /* FIXME: use g_uri_escape_string() for item name */
    g_file_info_set_name(fileinfo, menu_cache_item_get_id(item));
    if(menu_cache_item_get_name(item) != NULL)
        g_file_info_set_display_name(fileinfo, menu_cache_item_get_name(item));

    /* the setup below was in fm_file_info_set_from_menu_cache_item()
       so this setup makes latter API deprecated */
    icon_name = menu_cache_item_get_icon(item);
    if(icon_name)
    {
        if(icon_name[0] != '/') /* this is a icon name, not a full path to icon file. */
        {
            char *dot = strrchr(icon_name, '.'), *tmp = NULL;

            /* remove file extension, this is a hack to fix non-standard desktop entry files */
            if(G_UNLIKELY(dot))
            {
                ++dot;
                if(strcmp(dot, "png") == 0 ||
                   strcmp(dot, "svg") == 0 ||
                   strcmp(dot, "xpm") == 0)
                {
                    tmp = g_strndup(icon_name, dot - icon_name - 1);
                    icon_name = tmp;
                }
            }
            icon = g_themed_icon_new(icon_name);

            if(G_UNLIKELY(tmp))
                g_free(tmp);
        }
        /* this part is from fm_icon_from_name */
        else /* absolute path */
        {
            GFile* gicon_file = g_file_new_for_path(icon_name);
            icon = g_file_icon_new(gicon_file);
            g_object_unref(gicon_file);
        }
        if(G_LIKELY(icon))
        {
            g_file_info_set_icon(fileinfo, icon);
            g_object_unref(icon);
        }
    }
    if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_DIR)
        g_file_info_set_file_type(fileinfo, G_FILE_TYPE_DIRECTORY);
    else /* MENU_CACHE_TYPE_APP */
    {
        g_file_info_set_file_type(fileinfo, G_FILE_TYPE_SHORTCUT);
        g_file_info_set_attribute_string(fileinfo,
                                         G_FILE_ATTRIBUTE_STANDARD_TARGET_URI,
                                         menu_cache_item_get_file_path(item));
        //g_file_info_set_content_type(fileinfo, "application/x-desktop");
    }
    return fileinfo;
}

typedef struct
{
    union
    {
        FmVfsMenuEnumerator *enumerator;
        const char *path_str;
    };
//    const char *attributes;
//    GFileQueryInfoFlags flags;
    union
    {
        GCancellable *cancellable;
        GFile *file;
    };
    GError **error;
    gpointer result;
} FmVfsMenuMainThreadData;

static gboolean _fm_vfs_menu_enumerator_next_file_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    FmVfsMenuEnumerator *enu = init->enumerator;
    GSList *child = enu->child;
    MenuCacheItem *item;

    init->result = NULL;

    if(child == NULL)
        goto done;

    for(; child; child = child->next)
    {
        if(g_cancellable_set_error_if_cancelled(init->cancellable, init->error))
            break;
        item = MENU_CACHE_ITEM(child->data);
        /* also hide menu items which should be hidden in current DE. */
        if(!item || menu_cache_item_get_type(item) == MENU_CACHE_TYPE_SEP ||
           menu_cache_item_get_type(item) == MENU_CACHE_TYPE_NONE)
            continue;
        if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP
           && !menu_cache_app_get_is_visible(MENU_CACHE_APP(item), enu->de_flag))
            continue;

        init->result = _g_file_info_from_menu_cache_item(item);
        child = child->next;
        break;
    }
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    while(enu->child != child) /* free skipped/used elements */
    {
        GSList *ch = enu->child;
        enu->child = ch->next;
        menu_cache_item_unref(ch->data);
        g_slist_free_1(ch);
    }
#else
    enu->child = child;
#endif

done:
    return FALSE;
}

static GFileInfo *_fm_vfs_menu_enumerator_next_file(GFileEnumerator *enumerator,
                                                    GCancellable *cancellable,
                                                    GError **error)
{
    FmVfsMenuMainThreadData init;

    init.enumerator = FM_VFS_MENU_ENUMERATOR(enumerator);
    init.cancellable = cancellable;
    init.error = error;
    fm_run_in_default_main_context(_fm_vfs_menu_enumerator_next_file_real, &init);
    return init.result;
}

static gboolean _fm_vfs_menu_enumerator_close(GFileEnumerator *enumerator,
                                              GCancellable *cancellable,
                                              GError **error)
{
    FmVfsMenuEnumerator *enu = FM_VFS_MENU_ENUMERATOR(enumerator);

    if(enu->mc)
    {
        menu_cache_unref(enu->mc);
        enu->mc = NULL;
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
        g_slist_free_full(enu->child, (GDestroyNotify)menu_cache_item_unref);
#endif
        enu->child = NULL;
    }
    return TRUE;
}

static void fm_vfs_menu_enumerator_class_init(FmVfsMenuEnumeratorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GFileEnumeratorClass *enumerator_class = G_FILE_ENUMERATOR_CLASS(klass);

  gobject_class->dispose = _fm_vfs_menu_enumerator_dispose;

  enumerator_class->next_file = _fm_vfs_menu_enumerator_next_file;
  enumerator_class->close_fn = _fm_vfs_menu_enumerator_close;
}

static void fm_vfs_menu_enumerator_init(FmVfsMenuEnumerator *enumerator)
{
    /* nothing */
}

static MenuCacheItem *_vfile_path_to_menu_cache_item(MenuCache* mc, const char *path)
{
    MenuCacheItem *dir;
    char *unescaped, *tmp = NULL;

    unescaped = g_uri_unescape_string(path, NULL);
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    dir = MENU_CACHE_ITEM(menu_cache_dup_root_dir(mc));
    if(dir)
    {
        tmp = g_strconcat("/", menu_cache_item_get_id(dir), "/", unescaped, NULL);
        menu_cache_item_unref(dir);
        dir = menu_cache_item_from_path(mc, tmp);
    }
#else
    dir = MENU_CACHE_ITEM(menu_cache_get_root_dir(mc));
    if(dir)
    {
        const char *id;
        tmp = g_strconcat("/", menu_cache_item_get_id(dir), "/", unescaped, NULL);
        /* FIXME: how to access not dir? */
        dir = MENU_CACHE_ITEM(menu_cache_get_dir_from_path(mc, tmp));
        /* The menu-cache is buggy and returns parent for invalid path
           instead of failure so we check what we got here.
           Unfortunately we cannot detect if requested name is the same
           as its parent and menu-cache returned the parent. */
        id = strrchr(unescaped, '/');
        if(id)
            id++;
        else
            id = unescaped;
        if(dir != NULL && strcmp(id, menu_cache_item_get_id(dir)) != 0)
            dir = NULL;
    }
#endif
    g_free(unescaped);
    g_free(tmp);
    /* NOTE: returned value is referenced for >= 0.4.0 only */
    return dir;
}

static gboolean _fm_vfs_menu_enumerator_new_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    FmVfsMenuEnumerator *enumerator;
    MenuCache* mc;
    const char *de_name;
    MenuCacheItem *dir;

    mc = menu_cache_lookup_sync("applications.menu");
    /* ensure that the menu cache is loaded */
    if(mc == NULL) /* if it's not loaded */
    {
        /* try to set $XDG_MENU_PREFIX to "lxde-" for lxmenu-data */
        const char* menu_prefix = g_getenv("XDG_MENU_PREFIX");
        if(g_strcmp0(menu_prefix, "lxde-")) /* if current value is not lxde- */
        {
            char* old_prefix = g_strdup(menu_prefix);
            g_setenv("XDG_MENU_PREFIX", "lxde-", TRUE);
            mc = menu_cache_lookup_sync("applications.menu");
            /* restore original environment variable */
            if(old_prefix)
            {
                g_setenv("XDG_MENU_PREFIX", old_prefix, TRUE);
                g_free(old_prefix);
            }
            else
                g_unsetenv("XDG_MENU_PREFIX");
        }
    }

    if(mc == NULL) /* initialization failed */
    {
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Menu cache error"));
        return FALSE;
    }

    enumerator = g_object_new(FM_TYPE_VFS_MENU_ENUMERATOR, "container",
                              init->file, NULL);
    enumerator->mc = mc;
    de_name = g_getenv("XDG_CURRENT_DESKTOP");

    if(de_name)
        enumerator->de_flag = menu_cache_get_desktop_env_flag(mc, de_name);
    else
        enumerator->de_flag = (guint32)-1;

    /* the menu should be loaded now */
    if(init->path_str)
        dir = _vfile_path_to_menu_cache_item(mc, init->path_str);
    else
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
        dir = MENU_CACHE_ITEM(menu_cache_dup_root_dir(mc));
#else
        dir = MENU_CACHE_ITEM(menu_cache_get_root_dir(mc));
#endif
    if(dir)
    {
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
        enumerator->child = menu_cache_dir_list_children(MENU_CACHE_DIR(dir));
        menu_cache_item_unref(dir);
#else
        enumerator->child = menu_cache_dir_get_children(MENU_CACHE_DIR(dir));
#endif
    }
    /* FIXME: do something with attributes and flags */

    init->result = enumerator;
    return FALSE;
}

static GFileEnumerator *_fm_vfs_menu_enumerator_new(GFile *file,
                                                    const char *path_str,
                                                    const char *attributes,
                                                    GFileQueryInfoFlags flags,
                                                    GError **error)
{
    FmVfsMenuMainThreadData enu;

    enu.path_str = path_str;
//    enu.attributes = attributes;
//    enu.flags = flags;
    enu.file = file;
    enu.error = error;
    enu.result = NULL;
    fm_run_in_default_main_context(_fm_vfs_menu_enumerator_new_real, &enu);
    return enu.result;
}


/* ---- GFile implementation ---- */
#define ERROR_UNSUPPORTED(err) g_set_error_literal(err, G_IO_ERROR, \
                        G_IO_ERROR_NOT_SUPPORTED, _("Operation not supported"))

static GFile *_fm_vfs_menu_dup(GFile *file)
{
    FmMenuVFile *item, *new_item;

    item = FM_MENU_VFILE(file);
    new_item = _fm_menu_vfile_new();
    if(item->path)
        new_item->path = g_strdup(item->path);
    return (GFile*)new_item;
}

static guint _fm_vfs_menu_hash(GFile *file)
{
    return g_str_hash(FM_MENU_VFILE(file)->path ? FM_MENU_VFILE(file)->path : "/");
}

static gboolean _fm_vfs_menu_equal(GFile *file1, GFile *file2)
{
    char *path1 = FM_MENU_VFILE(file1)->path;
    char *path2 = FM_MENU_VFILE(file2)->path;

    return g_strcmp0(path1, path2) == 0;
}

static gboolean _fm_vfs_menu_is_native(GFile *file)
{
    return FALSE;
}

static gboolean _fm_vfs_menu_has_uri_scheme(GFile *file, const char *uri_scheme)
{
    return g_ascii_strcasecmp(uri_scheme, "menu") == 0;
}

static char *_fm_vfs_menu_get_uri_scheme(GFile *file)
{
    return g_strdup("menu");
}

static char *_fm_vfs_menu_get_basename(GFile *file)
{
    if(FM_MENU_VFILE(file)->path == NULL)
        return g_strdup("/");
    return g_path_get_basename(FM_MENU_VFILE(file)->path);
}

static char *_fm_vfs_menu_get_path(GFile *file)
{
    return NULL;
}

static char *_fm_vfs_menu_get_uri(GFile *file)
{
    return g_strconcat("menu://applications/", FM_MENU_VFILE(file)->path, NULL);
}

static char *_fm_vfs_menu_get_parse_name(GFile *file)
{
    char *unescaped, *path;

    unescaped = g_uri_unescape_string(FM_MENU_VFILE(file)->path, NULL);
    path = g_strconcat("menu://applications/", unescaped, NULL);
    g_free(unescaped);
    return path;
}

static GFile *_fm_vfs_menu_get_parent(GFile *file)
{
    char *path = FM_MENU_VFILE(file)->path;
    char *dirname;
    GFile *parent;

    if(path)
    {
        dirname = g_path_get_dirname(path);
        if(strcmp(dirname, ".") == 0)
            g_free(dirname);
        else
            path = dirname;
    }
    parent = _fm_vfs_menu_new_for_uri(path);
    if(path)
        g_free(path);
    return parent;
}

/* this function is taken from GLocalFile implementation */
static const char *match_prefix (const char *path, const char *prefix)
{
  int prefix_len;

  prefix_len = strlen (prefix);
  if (strncmp (path, prefix, prefix_len) != 0)
    return NULL;

  if (prefix_len > 0 && (prefix[prefix_len-1]) == '/')
    prefix_len--;

  return path + prefix_len;
}

static gboolean _fm_vfs_menu_prefix_matches(GFile *prefix, GFile *file)
{
    const char *path = FM_MENU_VFILE(file)->path;
    const char *pp = FM_MENU_VFILE(prefix)->path;
    const char *remainder;

    if(pp == NULL)
        return TRUE;
    if(path == NULL)
        return FALSE;
    remainder = match_prefix(path, pp);
    if(remainder != NULL && *remainder == '/')
        return TRUE;
    return FALSE;
}

static char *_fm_vfs_menu_get_relative_path(GFile *parent, GFile *descendant)
{
    const char *path = FM_MENU_VFILE(descendant)->path;
    const char *pp = FM_MENU_VFILE(parent)->path;
    const char *remainder;

    if(pp == NULL)
        return g_strdup(path);
    if(path == NULL)
        return NULL;
    remainder = match_prefix(path, pp);
    if(remainder != NULL && *remainder == '/')
        return g_strdup(&remainder[1]);
    return NULL;
}

static GFile *_fm_vfs_menu_resolve_relative_path(GFile *file, const char *relative_path)
{
    const char *path = FM_MENU_VFILE(file)->path;
    FmMenuVFile *new_item = _fm_menu_vfile_new();

    /* FIXME: handle if relative_path is invalid */
    if(relative_path == NULL || *relative_path == '\0')
        new_item->path = g_strdup(path);
    else if(path == NULL)
        new_item->path = g_strdup(relative_path);
    else
        new_item->path = g_strconcat(path, "/", relative_path, NULL);
    return (GFile*)new_item;
}

/* this is taken from GLocalFile implementation */
static GFile *_fm_vfs_menu_get_child_for_display_name(GFile *file,
                                                      const char *display_name,
                                                      GError **error)
{
#if 1
    /* FIXME: is it really need to be implemented? */
    ERROR_UNSUPPORTED(error);
    return NULL;
#else
    /* Unfortunately this will never work since there is no correlation
       between display name and item id.
       The only way is to iterate all children and compare display names. */
  GFile *new_file;
  char *basename;

  basename = g_filename_from_utf8 (display_name, -1, NULL, NULL, NULL);
  if (basename == NULL)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_FILENAME,
                   _("Invalid filename %s"), display_name);
      return NULL;
    }

  new_file = g_file_get_child (file, basename);
  g_free (basename);

  return new_file;
#endif
}

static GFileEnumerator *_fm_vfs_menu_enumerate_children(GFile *file,
                                                        const char *attributes,
                                                        GFileQueryInfoFlags flags,
                                                        GCancellable *cancellable,
                                                        GError **error)
{
    const char *path = FM_MENU_VFILE(file)->path;

    return _fm_vfs_menu_enumerator_new(file, path, attributes, flags, error);
}

static gboolean _fm_vfs_menu_query_info_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    MenuCache *mc;
    MenuCacheItem *dir;
    gboolean is_invalid = FALSE;

    init->result = NULL;
    mc = menu_cache_lookup_sync("applications.menu");
    if(mc == NULL)
    {
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Menu cache error"));
        goto _mc_failed;
    }

    if(init->path_str)
    {
        dir = _vfile_path_to_menu_cache_item(mc, init->path_str);
        if(dir == NULL)
            is_invalid = TRUE;
    }
    else
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
        dir = MENU_CACHE_ITEM(menu_cache_dup_root_dir(mc));
#else
        dir = MENU_CACHE_ITEM(menu_cache_get_root_dir(mc));
#endif
    if(is_invalid)
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                            _("Invalid menu directory"));
    else if(dir)
        init->result = _g_file_info_from_menu_cache_item(dir);
    else /* menu_cache_get_root_dir failed */
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Menu cache error"));

#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    if(dir)
        menu_cache_item_unref(dir);
#endif
    menu_cache_unref(mc);

_mc_failed:
    return FALSE;
}

static GFileInfo *_fm_vfs_menu_query_info(GFile *file,
                                          const char *attributes,
                                          GFileQueryInfoFlags flags,
                                          GCancellable *cancellable,
                                          GError **error)
{
    FmMenuVFile *item = FM_MENU_VFILE(file);
    GFileInfo *info;
    GFileAttributeMatcher *matcher;
    char *basename, *unescaped;
    FmVfsMenuMainThreadData enu;

    matcher = g_file_attribute_matcher_new(attributes);

    if(g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_TYPE) ||
       g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_ICON) ||
       g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI) ||
       //g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE) ||
       g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME))
    {
        /* retrieve matching attributes from menu-cache */
        enu.path_str = item->path;
//        enu.attributes = attributes;
//        enu.flags = flags;
        enu.cancellable = cancellable;
        enu.error = error;
        fm_run_in_default_main_context(_fm_vfs_menu_query_info_real, &enu);
        info = enu.result;
    }
    else
    {
        info = g_file_info_new();
        if(g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_NAME))
        {
            if(item->path == NULL)
                basename = g_strdup("/");
            else
                basename = g_path_get_basename(item->path);
            unescaped = g_uri_unescape_string(basename, NULL);
            g_free(basename);
            g_file_info_set_name(info, unescaped);
            g_free(unescaped);
        }
    }

    g_file_attribute_matcher_unref (matcher);

    return info;
}

static GFileInfo *_fm_vfs_menu_query_filesystem_info(GFile *file,
                                                     const char *attributes,
                                                     GCancellable *cancellable,
                                                     GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static GMount *_fm_vfs_menu_find_enclosing_mount(GFile *file,
                                                 GCancellable *cancellable,
                                                 GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static GFile *_fm_vfs_menu_set_display_name(GFile *file,
                                            const char *display_name,
                                            GCancellable *cancellable,
                                            GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static GFileAttributeInfoList *_fm_vfs_menu_query_settable_attributes(GFile *file,
                                                                      GCancellable *cancellable,
                                                                      GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static GFileAttributeInfoList *_fm_vfs_menu_query_writable_namespaces(GFile *file,
                                                                      GCancellable *cancellable,
                                                                      GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static gboolean _fm_vfs_menu_set_attribute(GFile *file,
                                           const char *attribute,
                                           GFileAttributeType type,
                                           gpointer value_p,
                                           GFileQueryInfoFlags flags,
                                           GCancellable *cancellable,
                                           GError **error)
{
    ERROR_UNSUPPORTED(error);
    return FALSE;
}

static gboolean _fm_vfs_menu_set_attributes_from_info(GFile *file,
                                                      GFileInfo *info,
                                                      GFileQueryInfoFlags flags,
                                                      GCancellable *cancellable,
                                                      GError **error)
{
    ERROR_UNSUPPORTED(error);
    return FALSE;
}

static gboolean _fm_vfs_menu_read_fn_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    MenuCache *mc;
    MenuCacheItem *item = NULL;
    gboolean is_invalid = TRUE;

    init->result = NULL;
    mc = menu_cache_lookup_sync("applications.menu");
    if(mc == NULL)
    {
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Menu cache error"));
        goto _mc_failed;
    }

    if(init->path_str)
    {
        item = _vfile_path_to_menu_cache_item(mc, init->path_str);
        /* If item wasn't found or isn't a file then we cannot read it. */
        if(item != NULL && menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP)
            is_invalid = FALSE;
    }

    if(is_invalid)
        g_set_error(init->error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                    _("The \"%s\" isn't a menu item"),
                    init->path_str ? init->path_str : "/");
    else
    {
        char *file_path;
        GFile *gf;

        file_path = menu_cache_item_get_file_path(item);
        if (file_path)
        {
            gf = g_file_new_for_path(file_path);
            g_free(file_path);
            if (gf)
            {
                init->result = g_file_read(gf, init->cancellable, init->error);
                g_object_unref(gf);
            }
        }
    }

#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    if(item)
        menu_cache_item_unref(item);
#endif
    menu_cache_unref(mc);

_mc_failed:
    return FALSE;
}

static GFileInputStream *_fm_vfs_menu_read_fn(GFile *file,
                                              GCancellable *cancellable,
                                              GError **error)
{
    FmMenuVFile *item = FM_MENU_VFILE(file);
    FmVfsMenuMainThreadData enu;

    enu.path_str = item->path;
    enu.cancellable = cancellable;
    enu.error = error;
    fm_run_in_default_main_context(_fm_vfs_menu_read_fn_real, &enu);
    return enu.result;
}

static GFileOutputStream *_fm_vfs_menu_append_to(GFile *file,
                                                 GFileCreateFlags flags,
                                                 GCancellable *cancellable,
                                                 GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static gboolean _fm_vfs_menu_create_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    MenuCache *mc;
    char *unescaped = NULL, *basename, *subdir;
    gsize tmp_len;
    gboolean is_invalid = TRUE;

    init->result = NULL;
    if(init->path_str)
    {
        MenuCacheItem *item;

        mc = menu_cache_lookup_sync("applications.menu");
        if(mc == NULL)
        {
            g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                _("Menu cache error"));
            goto _mc_failed;
        }
        unescaped = g_uri_unescape_string(init->path_str, NULL);
        /* ensure new menu item has suffix .desktop */
        if (!g_str_has_suffix(unescaped, ".desktop"))
        {
            basename = unescaped;
            unescaped = g_strconcat(unescaped, ".desktop", NULL);
            g_free(basename);
        }
        basename = strrchr(unescaped, '/');
        if (basename)
        {
            *basename++ = '\0';
            subdir = strchr(unescaped, '/');
            if(subdir) /* path is "Category/.../File.desktop" */
                *subdir++ = '\0';
#if MENU_CACHE_CHECK_VERSION(0, 5, 0)
            item = menu_cache_find_item_by_id(mc, basename);
            menu_cache_item_unref(item); /* use item simply as marker */
#else
            GSList *list = menu_cache_list_all_apps(mc), *l;
            for (l = list; l; l = l->next)
                if (strcmp(menu_cache_item_get_id(l->data), basename) == 0)
                    break;
            if (l)
                item = l->data;
            else
                item = NULL;
            g_slist_free_full(list, (GDestroyNotify)menu_cache_item_unref);
#endif
            if(item == NULL)
                is_invalid = FALSE;
        }
        g_debug("basename %s, category %s, subdir %s", basename, unescaped, subdir);
        menu_cache_unref(mc);
    }

    if(is_invalid)
        g_set_error(init->error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                    _("Cannot create menu item \"%s\""),
                    init->path_str ? init->path_str : "/");
    else
    {
        char *file_path;
        GFile *gf;
        GOutputStream *fstream;

        file_path = g_build_filename(g_get_user_data_dir(), "applications",
                                     basename, NULL);
        /* FIXME: make subdirectories if it's not directly in category */
        if (file_path)
        {
            gf = g_file_new_for_path(file_path);
            g_free(file_path);
            if (gf)
            {
                g_file_delete(gf, NULL, NULL); /* remove old if there is any */
                fstream = G_OUTPUT_STREAM(g_file_create(gf,
                                                G_FILE_CREATE_REPLACE_DESTINATION,
                                                init->cancellable, init->error));
                if (fstream)
                {
                    /* write simple stub before returning the handle */
                    file_path = g_strdup_printf("[Desktop Entry]\n"
                                                "Type=Application\n"
                                                "Name=\n"
                                                "Exec=\n"
                                                "Categories=%s;\n", unescaped);
                    g_output_stream_write_all(fstream, file_path, strlen(file_path),
                                              &tmp_len, init->cancellable, NULL);
                    g_free(file_path);
                    g_seekable_seek(G_SEEKABLE(fstream), (goffset)0, G_SEEK_SET,
                                    init->cancellable, NULL);
                    /* FIXME: handle cancellation and errors */
                    init->result = fstream;
                }
                g_object_unref(gf);
            }
        }
    }
    g_free(unescaped);

_mc_failed:
    return FALSE;
}

static GFileOutputStream *_fm_vfs_menu_create(GFile *file,
                                              GFileCreateFlags flags,
                                              GCancellable *cancellable,
                                              GError **error)
{
    FmMenuVFile *item = FM_MENU_VFILE(file);
    FmVfsMenuMainThreadData enu;

    enu.path_str = item->path;
    enu.cancellable = cancellable;
    enu.error = error;
    // enu.flags = flags;
    fm_run_in_default_main_context(_fm_vfs_menu_create_real, &enu);
    return enu.result;
}

static gboolean _fm_vfs_menu_replace_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    MenuCache *mc;
    char *unescaped = NULL, *basename;
    gboolean is_invalid = TRUE;

    init->result = NULL;
    if(init->path_str)
    {
        MenuCacheItem *item, *item2;

        mc = menu_cache_lookup_sync("applications.menu");
        if(mc == NULL)
        {
            g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                _("Menu cache error"));
            goto _mc_failed;
        }
        /* prepare basename first */
        unescaped = g_uri_unescape_string(init->path_str, NULL);
        basename = strrchr(unescaped, '/');
        if (basename != NULL)
            *basename++ = '\0';
        /* get existing item */
        item = _vfile_path_to_menu_cache_item(mc, init->path_str);
        /* if not found then check item by id to exclude conflicts */
        if (item != NULL) /* item is there, OK, we'll replace it then */
            is_invalid = FALSE;
        else if (basename != NULL)
        {
#if MENU_CACHE_CHECK_VERSION(0, 5, 0)
            item2 = menu_cache_find_item_by_id(mc, basename);
#else
            GSList *list = menu_cache_list_all_apps(mc), *l;
            for (l = list; l; l = l->next)
                if (strcmp(menu_cache_item_get_id(l->data), basename) == 0)
                    break;
            if (l)
                item2 = menu_cache_item_ref(l->data);
            else
                item2 = NULL;
            g_slist_free_full(list, (GDestroyNotify)menu_cache_item_unref);
#endif
            if(item2 == NULL)
                is_invalid = FALSE;
            else /* item was found in another category */
                menu_cache_item_unref(item2);
        }
        /* if basename is NULL then we trying to create item in root, i.e.
           outside of categories and that should be prohibited */
        menu_cache_unref(mc);
    }

    if(is_invalid)
        g_set_error(init->error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                    _("Cannot create menu item \"%s\""),
                    init->path_str ? init->path_str : "/");
    else
    {
        char *file_path;
        GFile *gf;

        file_path = g_build_filename(g_get_user_data_dir(), "applications",
                                     basename, NULL);
        /* FIXME: make subdirectories if it's not directly in category */
        if (file_path)
        {
            gf = g_file_new_for_path(file_path);
            g_free(file_path);
            if (gf)
            {
                /* FIXME: use flags and make_backup */
                init->result = g_file_replace(gf, NULL, FALSE,
                                              G_FILE_CREATE_REPLACE_DESTINATION,
                                              init->cancellable, init->error);
                /* FIXME: create own handler instead of using g_file_replace() */
                g_object_unref(gf);
            }
        }
    }
    g_free(unescaped);

_mc_failed:
    return FALSE;
}

static GFileOutputStream *_fm_vfs_menu_replace(GFile *file,
                                               const char *etag,
                                               gboolean make_backup,
                                               GFileCreateFlags flags,
                                               GCancellable *cancellable,
                                               GError **error)
{
    FmMenuVFile *item = FM_MENU_VFILE(file);
    FmVfsMenuMainThreadData enu;

    enu.path_str = item->path;
    enu.cancellable = cancellable;
    enu.error = error;
    // enu.flags = flags;
    // enu.make_backup = make_backup;
    fm_run_in_default_main_context(_fm_vfs_menu_replace_real, &enu);
    return enu.result;
}

static gboolean _fm_vfs_menu_delete_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    MenuCache *mc;
    MenuCacheItem *item = NULL;
    char *file_path, *contents;
    GKeyFile *kf;
    GFile *gf;
    GOutputStream *out;
    gsize length, tmp_len;

    init->result = (gpointer)FALSE;
    if(init->path_str == NULL)
    {
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Cannot delete root directory"));
        goto _failed;
    }
    mc = menu_cache_lookup_sync("applications.menu");
    if(mc == NULL)
    {
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Menu cache error"));
        goto _failed;
    }
    item = _vfile_path_to_menu_cache_item(mc, init->path_str);
    if(item == NULL || menu_cache_item_get_type(item) != MENU_CACHE_TYPE_APP)
    {
        g_set_error(init->error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                    _("The \"%s\" isn't a menu item"), init->path_str);
        goto _failed;
    }
    file_path = menu_cache_item_get_file_path(item);
    if (file_path == NULL)
    {
        g_set_error(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    _("Invalid menu item %s"), init->path_str);
        goto _failed;
    }
    kf = g_key_file_new();
    if (!g_key_file_load_from_file(kf, file_path,
                                   G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                                   init->error))
    {
        g_free(file_path);
        g_key_file_free(kf);
        goto _failed;
    }
    g_free(file_path);
    g_key_file_set_boolean(kf, G_KEY_FILE_DESKTOP_GROUP,
                           G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, TRUE);
    contents = g_key_file_to_data(kf, &length, init->error);
    g_key_file_free(kf);
    if (contents == NULL)
        goto _failed;
    file_path = g_build_filename(g_get_user_data_dir(), "applications",
                                 menu_cache_item_get_file_basename(item), NULL);
    /* FIXME: make subdirectories if it's not directly in category */
    gf = g_file_new_for_path(file_path);
    g_free(file_path);
    out = G_OUTPUT_STREAM(g_file_replace(gf, NULL, FALSE,
                                         G_FILE_CREATE_REPLACE_DESTINATION,
                                         init->cancellable, init->error));
    g_object_unref(gf);
    if (out == NULL)
    {
        g_free(contents);
        goto _failed;
    }
    if(!g_output_stream_write_all(out, contents, length, &tmp_len,
                                  init->cancellable, init->error))
    {
        g_free(contents);
        goto _failed;
    }
    g_free(contents);
    g_output_stream_close(out, init->cancellable, init->error);
    g_object_unref(out);
    init->result = (gpointer)TRUE;

_failed:
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    if(item)
        menu_cache_item_unref(item);
#endif
    return FALSE;
}

static gboolean _fm_vfs_menu_delete_file(GFile *file,
                                         GCancellable *cancellable,
                                         GError **error)
{
    FmMenuVFile *item = FM_MENU_VFILE(file);
    FmVfsMenuMainThreadData enu;

    enu.path_str = item->path;
    enu.cancellable = cancellable;
    enu.error = error;
    fm_run_in_default_main_context(_fm_vfs_menu_delete_real, &enu);
    return (gboolean)enu.result;
}

static gboolean _fm_vfs_menu_trash(GFile *file,
                                   GCancellable *cancellable,
                                   GError **error)
{
    ERROR_UNSUPPORTED(error);
    return FALSE;
}

static gboolean _fm_vfs_menu_make_directory(GFile *file,
                                            GCancellable *cancellable,
                                            GError **error)
{
    ERROR_UNSUPPORTED(error);
    return FALSE;
}

static gboolean _fm_vfs_menu_make_symbolic_link(GFile *file,
                                                const char *symlink_value,
                                                GCancellable *cancellable,
                                                GError **error)
{
    ERROR_UNSUPPORTED(error);
    return FALSE;
}

static gboolean _fm_vfs_menu_copy(GFile *source,
                                  GFile *destination,
                                  GFileCopyFlags flags,
                                  GCancellable *cancellable,
                                  GFileProgressCallback progress_callback,
                                  gpointer progress_callback_data,
                                  GError **error)
{
    ERROR_UNSUPPORTED(error);
    return FALSE;
}

static gboolean _fm_vfs_menu_move(GFile *source,
                                  GFile *destination,
                                  GFileCopyFlags flags,
                                  GCancellable *cancellable,
                                  GFileProgressCallback progress_callback,
                                  gpointer progress_callback_data,
                                  GError **error)
{
    ERROR_UNSUPPORTED(error);
    return FALSE;
}

/* ---- FmMenuVFileMonitor class ---- */
#define FM_TYPE_MENU_VFILE_MONITOR     (fm_vfs_menu_file_monitor_get_type())
#define FM_MENU_VFILE_MONITOR(o)       (G_TYPE_CHECK_INSTANCE_CAST((o), \
                                        FM_TYPE_MENU_VFILE_MONITOR, FmMenuVFileMonitor))

typedef struct _FmMenuVFileMonitor      FmMenuVFileMonitor;
typedef struct _FmMenuVFileMonitorClass FmMenuVFileMonitorClass;

static GType fm_vfs_menu_file_monitor_get_type  (void);

struct _FmMenuVFileMonitor
{
    GFileMonitor parent_object;

    FmMenuVFile *file;
    MenuCache *cache;
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    MenuCacheItem *item;
    MenuCacheNotifyId notifier;
#else
    GSList *items;
    gboolean stopped;
    gpointer notifier;
#endif
};

struct _FmMenuVFileMonitorClass
{
    GFileMonitorClass parent_class;
};

G_DEFINE_TYPE(FmMenuVFileMonitor, fm_vfs_menu_file_monitor, G_TYPE_FILE_MONITOR);

static void fm_vfs_menu_file_monitor_finalize(GObject *object)
{
    FmMenuVFileMonitor *mon = FM_MENU_VFILE_MONITOR(object);

    if(mon->cache)
    {
        if(mon->notifier)
            menu_cache_remove_reload_notify(mon->cache, mon->notifier);
        menu_cache_unref(mon->cache);
    }
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    if(mon->item)
        menu_cache_item_unref(mon->item);
#else
    g_slist_free_full(mon->items, (GDestroyNotify)menu_cache_item_unref);
#endif
    g_object_unref(mon->file);

    G_OBJECT_CLASS(fm_vfs_menu_file_monitor_parent_class)->finalize(object);
}

static gboolean fm_vfs_menu_file_monitor_cancel(GFileMonitor *monitor)
{
    FmMenuVFileMonitor *mon = FM_MENU_VFILE_MONITOR(monitor);

#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    if(mon->item)
        menu_cache_item_unref(mon->item); /* rest will be done in finalizer */
    mon->item = NULL;
#else
    mon->stopped = TRUE;
#endif
    return TRUE;
}

static void fm_vfs_menu_file_monitor_class_init(FmMenuVFileMonitorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GFileMonitorClass *gfilemon_class = G_FILE_MONITOR_CLASS (klass);

    gobject_class->finalize = fm_vfs_menu_file_monitor_finalize;
    gfilemon_class->cancel = fm_vfs_menu_file_monitor_cancel;
}

static void fm_vfs_menu_file_monitor_init(FmMenuVFileMonitor *item)
{
    /* nothing */
}

static FmMenuVFileMonitor *_fm_menu_vfile_monitor_new(void)
{
    return (FmMenuVFileMonitor*)g_object_new(FM_TYPE_MENU_VFILE_MONITOR, NULL);
}

static void _reload_notify_handler(MenuCache* cache, gpointer user_data)
{
    FmMenuVFileMonitor *mon = FM_MENU_VFILE_MONITOR(user_data);
    GSList *items, *new_items, *ol, *nl;
    MenuCacheItem *dir;
    GFile *file;
    const char *de_name;
    guint32 de_flag;

#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    if(mon->item == NULL) /* menu folder was destroyed or monitor cancelled */
        return;
    dir = mon->item;
    if(mon->file->path)
        mon->item = _vfile_path_to_menu_cache_item(cache, mon->file->path);
    else
        mon->item = MENU_CACHE_ITEM(menu_cache_dup_root_dir(cache));
    if(mon->item && menu_cache_item_get_type(mon->item) != MENU_CACHE_TYPE_DIR)
    {
        menu_cache_item_unref(mon->item);
        mon->item = NULL;
    }
    if(mon->item == NULL) /* folder was destroyed - emit event and exit */
    {
        menu_cache_item_unref(dir);
        g_file_monitor_emit_event(G_FILE_MONITOR(mon), G_FILE(mon->file), NULL,
                                  G_FILE_MONITOR_EVENT_DELETED);
        return;
    }
    items = menu_cache_dir_list_children(MENU_CACHE_DIR(dir));
    menu_cache_item_unref(dir);
    new_items = menu_cache_dir_list_children(MENU_CACHE_DIR(mon->item));
#else
    if(mon->stopped) /* menu folder was destroyed or monitor cancelled */
        return;
    if(mon->file->path)
        dir = _vfile_path_to_menu_cache_item(cache, mon->file->path);
    else
        dir = MENU_CACHE_ITEM(menu_cache_get_root_dir(cache));
    if(dir == NULL) /* folder was destroyed - emit event and exit */
    {
        mon->stopped = TRUE;
        g_file_monitor_emit_event(G_FILE_MONITOR(mon), G_FILE(mon->file), NULL,
                                  G_FILE_MONITOR_EVENT_DELETED);
        return;
    }
    items = mon->items;
    mon->items = g_slist_copy_deep(menu_cache_dir_get_children(MENU_CACHE_DIR(dir)),
                                   (GCopyFunc)menu_cache_item_ref, NULL);
    new_items = g_slist_copy_deep(mon->items, (GCopyFunc)menu_cache_item_ref, NULL);
#endif
    /* we have two copies of lists now, compare them and emit events */
    ol = items;
    de_name = g_getenv("XDG_CURRENT_DESKTOP");
    if(de_name)
        de_flag = menu_cache_get_desktop_env_flag(cache, de_name);
    else
        de_flag = (guint32)-1;
    while (ol)
    {
        for (nl = new_items; nl; nl = nl->next)
            if (strcmp(menu_cache_item_get_id(ol->data),
                       menu_cache_item_get_id(nl->data)) == 0)
                break; /* the same id found */
        if (nl)
        {
            /* check if any visible attribute of it was changed */
            if (g_strcmp0(menu_cache_item_get_name(ol->data),
                          menu_cache_item_get_name(nl->data)) == 0 ||
                g_strcmp0(menu_cache_item_get_icon(ol->data),
                          menu_cache_item_get_icon(nl->data)) == 0 ||
                menu_cache_app_get_is_visible(ol->data, de_flag) !=
                                menu_cache_app_get_is_visible(nl->data, de_flag))
                /* FIXME: test if it is hidden */
            {
                file = _fm_vfs_menu_resolve_relative_path(G_FILE(mon->file),
                                             menu_cache_item_get_id(nl->data));
                g_file_monitor_emit_event(G_FILE_MONITOR(mon), file, NULL,
                                          G_FILE_MONITOR_EVENT_CHANGED);
                g_object_unref(file);
            }
            /* free both new and old from the list */
            menu_cache_item_unref(nl->data);
            new_items = g_slist_delete_link(new_items, nl);
            nl = ol->next; /* use 'nl' as storage */
            menu_cache_item_unref(ol->data);
            items = g_slist_delete_link(items, ol);
            ol = nl;
        }
        else /* id not found (removed), go to next */
            ol = ol->next;
    }
    /* emit events for removed files */
    while (items)
    {
        file = _fm_vfs_menu_resolve_relative_path(G_FILE(mon->file),
                                             menu_cache_item_get_id(items->data));
        g_file_monitor_emit_event(G_FILE_MONITOR(mon), file, NULL,
                                  G_FILE_MONITOR_EVENT_DELETED);
        g_object_unref(file);
        menu_cache_item_unref(items->data);
        items = g_slist_delete_link(items, items);
    }
    /* emit events for added files */
    while (new_items)
    {
        file = _fm_vfs_menu_resolve_relative_path(G_FILE(mon->file),
                                     menu_cache_item_get_id(new_items->data));
        g_file_monitor_emit_event(G_FILE_MONITOR(mon), file, NULL,
                                  G_FILE_MONITOR_EVENT_CREATED);
        g_object_unref(file);
        menu_cache_item_unref(new_items->data);
        new_items = g_slist_delete_link(new_items, new_items);
    }
}

static GFileMonitor *_fm_vfs_menu_monitor_dir(GFile *file,
                                              GFileMonitorFlags flags,
                                              GCancellable *cancellable,
                                              GError **error)
{
    FmMenuVFileMonitor *mon;
#if !MENU_CACHE_CHECK_VERSION(0, 4, 0)
    MenuCacheItem *dir;
#endif

    /* open menu cache instance */
    mon = _fm_menu_vfile_monitor_new();
    if(mon == NULL) /* out of memory! */
        return NULL;
    mon->file = FM_MENU_VFILE(g_object_ref(file));
    mon->cache = menu_cache_lookup_sync("applications.menu");
    if(mon->cache == NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Menu cache error"));
        goto _fail;
    }
    /* check if requested path exists within cache */
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    if(mon->file->path)
        mon->item = _vfile_path_to_menu_cache_item(mon->cache, mon->file->path);
    else
        mon->item = MENU_CACHE_ITEM(menu_cache_dup_root_dir(mon->cache));
    if(mon->item == NULL || menu_cache_item_get_type(mon->item) != MENU_CACHE_TYPE_DIR)
#else
    if(mon->file->path)
        dir = _vfile_path_to_menu_cache_item(mon->cache, mon->file->path);
    else
        dir = MENU_CACHE_ITEM(menu_cache_get_root_dir(mon->cache));
    if(dir == NULL)
#endif
    {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    _("FmMenuVFileMonitor: folder %s not found in menu cache"),
                    mon->file->path);
        goto _fail;
    }
#if !MENU_CACHE_CHECK_VERSION(0, 4, 0)
    /* for old libmenu-cache we have no choice but copy all the data right now */
    mon->items = g_slist_copy_deep(menu_cache_dir_get_children(MENU_CACHE_DIR(dir)),
                                   (GCopyFunc)menu_cache_item_ref, NULL);
#endif
    /* current directory contents belong to mon->item now */
    /* attach reload notify handler */
    mon->notifier = menu_cache_add_reload_notify(mon->cache,
                                                 &_reload_notify_handler, mon);
    return (GFileMonitor*)mon;

_fail:
    g_object_unref(mon);
    return NULL;
}

static GFileMonitor *_fm_vfs_menu_monitor_file(GFile *file,
                                               GFileMonitorFlags flags,
                                               GCancellable *cancellable,
                                               GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

#if GLIB_CHECK_VERSION(2, 22, 0)
static GFileIOStream *_fm_vfs_menu_open_readwrite(GFile *file,
                                                  GCancellable *cancellable,
                                                  GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static GFileIOStream *_fm_vfs_menu_create_readwrite(GFile *file,
                                                    GFileCreateFlags flags,
                                                    GCancellable *cancellable,
                                                    GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static GFileIOStream *_fm_vfs_menu_replace_readwrite(GFile *file,
                                                     const char *etag,
                                                     gboolean make_backup,
                                                     GFileCreateFlags flags,
                                                     GCancellable *cancellable,
                                                     GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}
#endif /* Glib >= 2.22 */

static void fm_menu_g_file_init(GFileIface *iface)
{
    iface->dup = _fm_vfs_menu_dup;
    iface->hash = _fm_vfs_menu_hash;
    iface->equal = _fm_vfs_menu_equal;
    iface->is_native = _fm_vfs_menu_is_native;
    iface->has_uri_scheme = _fm_vfs_menu_has_uri_scheme;
    iface->get_uri_scheme = _fm_vfs_menu_get_uri_scheme;
    iface->get_basename = _fm_vfs_menu_get_basename;
    iface->get_path = _fm_vfs_menu_get_path;
    iface->get_uri = _fm_vfs_menu_get_uri;
    iface->get_parse_name = _fm_vfs_menu_get_parse_name;
    iface->get_parent = _fm_vfs_menu_get_parent;
    iface->prefix_matches = _fm_vfs_menu_prefix_matches;
    iface->get_relative_path = _fm_vfs_menu_get_relative_path;
    iface->resolve_relative_path = _fm_vfs_menu_resolve_relative_path;
    iface->get_child_for_display_name = _fm_vfs_menu_get_child_for_display_name;
    iface->enumerate_children = _fm_vfs_menu_enumerate_children;
    iface->query_info = _fm_vfs_menu_query_info;
    iface->query_filesystem_info = _fm_vfs_menu_query_filesystem_info;
    iface->find_enclosing_mount = _fm_vfs_menu_find_enclosing_mount;
    iface->set_display_name = _fm_vfs_menu_set_display_name;
    iface->query_settable_attributes = _fm_vfs_menu_query_settable_attributes;
    iface->query_writable_namespaces = _fm_vfs_menu_query_writable_namespaces;
    iface->set_attribute = _fm_vfs_menu_set_attribute;
    iface->set_attributes_from_info = _fm_vfs_menu_set_attributes_from_info;
    iface->read_fn = _fm_vfs_menu_read_fn;
    iface->append_to = _fm_vfs_menu_append_to;
    iface->create = _fm_vfs_menu_create;
    iface->replace = _fm_vfs_menu_replace;
    iface->delete_file = _fm_vfs_menu_delete_file;
    iface->trash = _fm_vfs_menu_trash;
    iface->make_directory = _fm_vfs_menu_make_directory;
    iface->make_symbolic_link = _fm_vfs_menu_make_symbolic_link;
    iface->copy = _fm_vfs_menu_copy;
    iface->move = _fm_vfs_menu_move;
    iface->monitor_dir = _fm_vfs_menu_monitor_dir;
    iface->monitor_file = _fm_vfs_menu_monitor_file;
#if GLIB_CHECK_VERSION(2, 22, 0)
    iface->open_readwrite = _fm_vfs_menu_open_readwrite;
    iface->create_readwrite = _fm_vfs_menu_create_readwrite;
    iface->replace_readwrite = _fm_vfs_menu_replace_readwrite;
    iface->supports_thread_contexts = TRUE;
#endif /* Glib >= 2.22 */
}


/* ---- FmFile implementation ---- */
static gboolean _fm_vfs_menu_wants_incremental(GFile* file)
{
    return FALSE;
}

static void fm_menu_fm_file_init(FmFileInterface *iface)
{
    iface->wants_incremental = _fm_vfs_menu_wants_incremental;
}


/* ---- interface for loading ---- */
static GFile *_fm_vfs_menu_new_for_uri(const char *uri)
{
    FmMenuVFile *item = _fm_menu_vfile_new();

    if(uri == NULL)
        uri = "";
    /* skip menu:/ */
    if(g_ascii_strncasecmp(uri, "menu:", 5) == 0)
        uri += 5;
    while(*uri == '/')
        uri++;
    /* skip "applications/" or "applications.menu/" */
    if(g_ascii_strncasecmp(uri, "applications", 12) == 0)
    {
        uri += 12;
        if(g_ascii_strncasecmp(uri, ".menu", 5) == 0)
            uri += 5;
    }
    while(*uri == '/')
        uri++;
    /* save the rest of path, NULL means the root path */
    if(*uri)
        item->path = g_strdup(uri);
    return (GFile*)item;
}

FmFileInitTable _fm_vfs_menu_init_table =
{
    .new_for_uri = &_fm_vfs_menu_new_for_uri
};
