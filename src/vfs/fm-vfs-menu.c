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
#include "fm-xml-file.h"

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

static GFileInfo *_g_file_info_from_menu_cache_item(MenuCacheItem *item,
                                                    guint32 de_flag)
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
        char *path = menu_cache_item_get_file_path(item);
        g_file_info_set_file_type(fileinfo, G_FILE_TYPE_SHORTCUT);
        g_file_info_set_attribute_string(fileinfo,
                                         G_FILE_ATTRIBUTE_STANDARD_TARGET_URI,
                                         path);
        g_free(path);
        g_file_info_set_content_type(fileinfo, "application/x-desktop");
        g_file_info_set_is_hidden(fileinfo,
                                  !menu_cache_app_get_is_visible(MENU_CACHE_APP(item),
                                                                 de_flag));
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
    union
    {
        FmMenuVFile *destination;
//        const char *attributes;
    };
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
        if(!item || menu_cache_item_get_type(item) == MENU_CACHE_TYPE_SEP ||
           menu_cache_item_get_type(item) == MENU_CACHE_TYPE_NONE)
            continue;
#if 0
        /* also hide menu items which should be hidden in current DE. */
        if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP
           && !menu_cache_app_get_is_visible(MENU_CACHE_APP(item), enu->de_flag))
            continue;
#endif

        init->result = _g_file_info_from_menu_cache_item(item, enu->de_flag);
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

static MenuCache *_get_menu_cache(GError **error)
{
    MenuCache *mc;
    static gboolean environment_tested = FALSE;
    static gboolean requires_prefix = FALSE;

    /* do it in compatibility with lxpanel */
    if(!environment_tested)
    {
        requires_prefix = (g_getenv("XDG_MENU_PREFIX") == NULL);
        environment_tested = TRUE;
    }
    mc = menu_cache_lookup_sync(requires_prefix ? "lxde-applications.menu" : "applications.menu");
    /* FIXME: may be it is reasonable to set XDG_MENU_PREFIX ? */

    if(mc == NULL) /* initialization failed */
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Menu cache error"));
    return mc;
}

static gboolean _fm_vfs_menu_enumerator_new_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    FmVfsMenuEnumerator *enumerator;
    MenuCache* mc;
    const char *de_name;
    MenuCacheItem *dir;

    mc = _get_menu_cache(init->error);

    if(mc == NULL) /* initialization failed */
        return FALSE;

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
        return g_uri_unescape_string(&remainder[1], NULL);
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
    {
        /* relative_path is the most probably unescaped string (at least GFVS
           works such way) so we have to escape invalid chars here. */
        char *escaped = g_uri_escape_string(relative_path,
                                            G_URI_RESERVED_CHARS_ALLOWED_IN_PATH,
                                            TRUE);
        new_item->path = g_strconcat(path, "/", relative_path, NULL);
        g_free(escaped);
    }
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
    mc = _get_menu_cache(init->error);
    if(mc == NULL)
        goto _mc_failed;

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
    {
        const char *de_name = g_getenv("XDG_CURRENT_DESKTOP");

        if(de_name)
            init->result = _g_file_info_from_menu_cache_item(dir,
                                menu_cache_get_desktop_env_flag(mc, de_name));
        else
            init->result = _g_file_info_from_menu_cache_item(dir, (guint32)-1);
    }
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
    char *basename, *id;
    FmVfsMenuMainThreadData enu;

    matcher = g_file_attribute_matcher_new(attributes);

    if(g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_TYPE) ||
       g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_ICON) ||
       g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI) ||
       g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE) ||
       g_file_attribute_matcher_matches(matcher, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
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
            id = g_uri_unescape_string(basename, NULL);
            g_free(basename);
            g_file_info_set_name(info, id);
            g_free(id);
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
    /* FIXME: renaming should be supported for both directory and application */
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

static inline GFile *_g_file_new_for_id(const char *id)
{
    char *file_path;
    GFile *file;

    file_path = g_build_filename(g_get_user_data_dir(), "applications", id, NULL);
    /* we can try to guess file path and make directories but it
       hardly worth the efforts so it's easier to just make new file
       by its ID since ID is unique thru all the menu */
    if (file_path == NULL)
        return NULL;
    file = g_file_new_for_path(file_path);
    g_free(file_path);
    return file;
}

static gboolean _fm_vfs_menu_read_fn_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    MenuCache *mc;
    MenuCacheItem *item = NULL;
    gboolean is_invalid = TRUE;

    init->result = NULL;
    mc = _get_menu_cache(init->error);
    if(mc == NULL)
        goto _mc_failed;

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

    /* g_debug("_fm_vfs_menu_read_fn %s", item->path); */
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


/* ---- applications.menu manipulations ---- */
typedef struct _FmMenuMenuTree          FmMenuMenuTree;

struct _FmMenuMenuTree
{
    FmXmlFile *menu; /* composite tree to analyze */
    char *file_path; /* current file */
    GCancellable *cancellable;
    gint line, pos; /* we remember position in deepest file */
};

G_LOCK_DEFINE(menuTree); /* locks all .menu file access data below */
static FmXmlFileTag menuTag_Menu = 0; /* tags that are supported */
static FmXmlFileTag menuTag_Include = 0;
static FmXmlFileTag menuTag_Exclude = 0;
static FmXmlFileTag menuTag_Filename = 0;
static FmXmlFileTag menuTag_Or = 0;
static FmXmlFileTag menuTag_And = 0;
static FmXmlFileTag menuTag_Not = 0;
static FmXmlFileTag menuTag_Category = 0;
static FmXmlFileTag menuTag_MergeFile = 0;
static FmXmlFileTag menuTag_MergeDir = 0;
static FmXmlFileTag menuTag_DefaultMergeDirs = 0;
//static FmXmlFileTag menuTag_Directory = 0;
static FmXmlFileTag menuTag_Name = 0;
//static FmXmlFileTag menuTag_Deleted = 0;
//static FmXmlFileTag menuTag_NotDeleted = 0;

/* this handler does nothing, used just to remember its id */
static gboolean _menu_xml_handler_pass(FmXmlFileItem *item, GList *children,
                                       char * const *attribute_names,
                                       char * const *attribute_values,
                                       guint n_attributes, gint line, gint pos,
                                       GError **error, gpointer user_data)
{
    FmMenuMenuTree *data = user_data;
    return !g_cancellable_set_error_if_cancelled(data->cancellable, error);
}

/* checks the tag */
static gboolean _menu_xml_handler_Name(FmXmlFileItem *item, GList *children,
                                       char * const *attribute_names,
                                       char * const *attribute_values,
                                       guint n_attributes, gint line, gint pos,
                                       GError **error, gpointer user_data)
{
    FmMenuMenuTree *data = user_data;

    if (g_cancellable_set_error_if_cancelled(data->cancellable, error))
        return FALSE;
    item = fm_xml_file_item_find_child(item, FM_XML_FILE_TEXT);
    if (item == NULL || fm_xml_file_item_get_data(item, NULL) == NULL) /* empty tag */
    {
        g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                            _("empty <Name> tag"));
        return FALSE;
    }
    return TRUE;
}

static gboolean _menu_xml_handler_Not(FmXmlFileItem *item, GList *children,
                                      char * const *attribute_names,
                                      char * const *attribute_values,
                                      guint n_attributes, gint line, gint pos,
                                      GError **error, gpointer user_data)
{
    FmMenuMenuTree *data = user_data;
    FmXmlFileTag tag;

    if (g_cancellable_set_error_if_cancelled(data->cancellable, error))
        return FALSE;
    if (children == NULL || children->next != NULL)
    {
        g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                            _("tag <Not> should have exactly one children"));
        g_list_free(children);
        return FALSE;
    }
    tag = fm_xml_file_item_get_tag(children->data);
    if (tag == menuTag_And || tag == menuTag_Or || tag == menuTag_Filename ||
        tag == menuTag_Category)
        return TRUE;
    g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                        _("tag <Not> may contain only <And>, <Or>, <Filename>"
                          " or <Category> tag"));
    return FALSE;
}

static gboolean _merge_xml_file(FmMenuMenuTree *data, FmXmlFileItem *item,
                                const char *path, GError **error)
{
    FmXmlFile *menu = NULL;
    GList *xml = NULL, *it; /* loaded list */
    GFile *gf;
    char *save_path, *contents;
    gsize len;
    gboolean ok;

    save_path = data->file_path;
    if (path[0] == '/') /* absolute path */
        data->file_path = g_strdup(path);
    else
    {
        char *dir = g_path_get_dirname(save_path);
        data->file_path = g_build_filename(dir, path, NULL);
        g_free(dir);
    }
    g_debug("merging the XML file '%s'", data->file_path);
    gf = g_file_new_for_path(data->file_path);
    ok = g_file_load_contents(gf, data->cancellable, &contents, &len, NULL, error);
    g_object_unref(gf);
    if (!ok)
    {
        g_free(save_path); /* replace the path with failed one */
        return FALSE;
    }
    menu = fm_xml_file_new(data->menu);
    /* g_debug("merging FmXmlFile %p into %p", menu, data->menu); */
    ok = fm_xml_file_parse_data(menu, contents, len, error, data);
    g_free(contents);
    if (ok)
        xml = fm_xml_file_finish_parse(menu, error);
    if (xml == NULL) /* error is set by failed function */
    {
        /* g_debug("freeing FmXmlFile %p (failed)", menu); */
        /* only this handler does recursion, therefore it is safe to set and
           and do check of data->line here */
        if (data->line == -1)
            data->line = fm_xml_file_get_current_line(menu, &data->pos);
        /* we do a little trick here - we don't restore previous fule but
           leave data->file_path for diagnostics in _update_categories() */
        g_free(save_path);
        g_object_unref(menu);
        return FALSE;
    }
    g_free(data->file_path);
    data->file_path = save_path;
    /* insert all children but Name before item */
    for (it = xml; it; it = it->next)
    {
        GList *xml_sub, *it_sub;

        if (fm_xml_file_item_get_tag(it->data) != menuTag_Menu)
        {
            g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                        _("merging file may contain only <Menu> top level tag,"
                          " got <%s>"), fm_xml_file_item_get_tag_name(it->data));
            /* FIXME: it will show error not for merged file but current */
            break;
        }
        xml_sub = fm_xml_file_item_get_children(it->data);
        for (it_sub = xml_sub; it_sub; it_sub = it_sub->next)
        {
            /* g_debug("merge: trying to insert %p into %p", it_sub->data,
                    fm_xml_file_item_get_parent(item)); */
            if (fm_xml_file_item_get_tag(it_sub->data) != menuTag_Name &&
                !fm_xml_file_insert_before(item, it_sub->data))
            {
                g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                            _("failed to insert tag <%s> from merging file"),
                            fm_xml_file_item_get_tag_name(it_sub->data));
                /* FIXME: it will show error not for merged file but current */
                break;
            }
        }
        g_list_free(xml_sub);
        if (it_sub) /* failed above */
            break;
    }
    g_list_free(xml);
    ok = (it == NULL);
    /* g_debug("freeing FmXmlFile %p (success=%d)", menu, (int)ok); */
    g_object_unref(menu);
    return ok;
}

static gboolean _merge_menu_directory(FmMenuMenuTree *data, FmXmlFileItem *item,
                                      const char *path, GError **error,
                                      gboolean ignore_not_exist)
{
    char *full_path, *child;
    GFile *gf;
    GFileEnumerator *fe;
    GFileInfo *fi;
    GError *err = NULL;
    gboolean ok = TRUE;

    if (path[0] == '/') /* absolute path */
        full_path = g_strdup(path);
    else
    {
        char *dir = g_path_get_dirname(data->file_path);
        full_path = g_build_filename(dir, path, NULL);
        g_free(dir);
    }
    g_debug("merging the XML directory '%s'", full_path);
    gf = g_file_new_for_path(full_path);
    fe = g_file_enumerate_children(gf, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                   G_FILE_QUERY_INFO_NONE, data->cancellable,
                                   &err);
    g_object_unref(gf);
    if (fe)
    {
        while ((fi = g_file_enumerator_next_file(fe, data->cancellable, NULL)))
        {
            const char *name = g_file_info_get_name(fi);
            if (strlen(name) <= 5 || !g_str_has_suffix(name, ".menu"))
            {
                /* skip files that aren't *.menu */
                g_object_unref(fi);
                continue;
            }
            child = g_build_filename(full_path, name, NULL);
            g_object_unref(fi);
            ok = _merge_xml_file(data, item, child, &err);
            if (!ok)
            {
                /*
                if (err->domain == G_IO_ERROR && err->code == G_IO_ERROR_PERMISSION_DENIED)
                {
                    g_warning("cannot merge XML file %s: no access", child);
                    g_clear_error(&err);
                    g_free(child);
                    ok = TRUE;
                    continue;
                }
                */
                g_free(child);
                g_propagate_error(error, err);
                err = NULL;
                break;
            }
            g_free(child);
        }
        /* FIXME: handle enumerator errors */
        g_object_unref(fe);
    }
    else if (ignore_not_exist && err->domain == G_IO_ERROR &&
             err->code == G_IO_ERROR_NOT_FOUND)
        g_error_free(err);
    else
    {
        g_propagate_error(error, err);
        ok = FALSE;
    }
    g_free(full_path);
    return ok;
}

/* adds .menu file contents next to current item */
static gboolean _menu_xml_handler_MergeFile(FmXmlFileItem *item, GList *children,
                                            char * const *attribute_names,
                                            char * const *attribute_values,
                                            guint n_attributes, gint line, gint pos,
                                            GError **error, gpointer user_data)
{
    FmMenuMenuTree *data = user_data;
    const char *path;
    gboolean ok;

    if (g_cancellable_set_error_if_cancelled(data->cancellable, error))
        return FALSE;
    /* find and load .menu */
    if (children == NULL ||
        fm_xml_file_item_get_tag(children->data) != FM_XML_FILE_TEXT ||
        fm_xml_file_item_get_data(children->data, NULL) == NULL)
    {
        g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                            _("invalid <MergeFile> tag"));
        return FALSE;
    }
    /* NOTE: this implementation ignores optional 'type' attribute */
    path = fm_xml_file_item_get_data(children->data, NULL);
    ok = _merge_xml_file(data, item, path, error);
    if (ok) /* no errors */
        /* destroy item -- we replaced it already */
        fm_xml_file_item_destroy(item);
    return ok;
}

/* adds all .menu files in directory */
static gboolean _menu_xml_handler_MergeDir(FmXmlFileItem *item, GList *children,
                                           char * const *attribute_names,
                                           char * const *attribute_values,
                                           guint n_attributes, gint line, gint pos,
                                           GError **error, gpointer user_data)
{
    FmMenuMenuTree *data = user_data;
    const char *path;
    gboolean ok;

    if (g_cancellable_set_error_if_cancelled(data->cancellable, error))
        return FALSE;
    /* get text from the tag */
    if (children == NULL ||
        fm_xml_file_item_get_tag(children->data) != FM_XML_FILE_TEXT ||
        fm_xml_file_item_get_data(children->data, NULL) == NULL)
    {
        g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                            _("invalid <MergeDir> tag"));
        return FALSE;
    }
    path = fm_xml_file_item_get_data(children->data, NULL);
    ok = _merge_menu_directory(data, item, path, error, FALSE);
    if (ok) /* no errors */
        /* destroy item -- we replaced it already */
        fm_xml_file_item_destroy(item);
    return ok;
}

static gboolean _menu_xml_handler_DefaultMergeDirs(FmXmlFileItem *item, GList *children,
                                                   char * const *attribute_names,
                                                   char * const *attribute_values,
                                                   guint n_attributes, gint line, gint pos,
                                                   GError **error, gpointer user_data)
{
    FmMenuMenuTree *data = user_data;
    const gchar * const *dirs = g_get_system_config_dirs();
    char *path;
    int i = g_strv_length((gchar**)dirs);
    gboolean ok;

    /* scan in reverse order - see XDG menu specification */
    while (--i >= 0)
    {
        path = g_build_filename(dirs[i], "menus", "applications-merged", NULL);
        ok = _merge_menu_directory(data, item, path, error, TRUE);
        g_free(path);
        if (!ok)
            return FALSE; /* failed to merge */
    }
    path = g_build_filename(g_get_user_config_dir(), "menus", "applications-merged", NULL);
    ok = _merge_menu_directory(data, item, path, error, TRUE);
    g_free(path);
    if (ok) /* no errors */
        /* destroy item -- we replaced it already */
        fm_xml_file_item_destroy(item);
    return ok;
}

/* FIXME: handle <Move><Old>...</Old><New>...</New></Move> */

static inline const char *_get_menu_name(FmXmlFileItem *item)
{
    if (fm_xml_file_item_get_tag(item) != menuTag_Menu) /* skip not menu */
        return NULL;
    item = fm_xml_file_item_find_child(item, menuTag_Name);
    if (item == NULL) /* no Name tag? */
        return NULL;
    item = fm_xml_file_item_find_child(item, FM_XML_FILE_TEXT);
    if (item == NULL) /* empty Name tag? */
        return NULL;
    return fm_xml_file_item_get_data(item, NULL);
}

/* merges subitems - consumes list */
static void _merge_tree(GList *first)
{
    while (first)
    {
        if (first->data) /* we might merge this one already */
        {
            if (first->next)
            {
                /* merge this item with identical ones */
                const char *name = _get_menu_name(first->data);
                GList *next;

                if (name) /* not a menu tag */
                {
                    for (next = first->next; next; next = next->next)
                    {
                        if (next->data == NULL) /* already merged */
                            continue;
                        if (g_strcmp0(name, _get_menu_name(next->data)) == 0)
                        {
                            GList *children = fm_xml_file_item_get_children(next->data);
                            GList *l;

                            g_debug("found two identical Menu '%s', merge them", name);
                            for (l = children; l; l = l->next) /* merge all but Name */
                                if (fm_xml_file_item_get_tag(l->data) != menuTag_Name)
                                    fm_xml_file_item_append_child(first->data, l->data);
                            g_list_free(children);
                            fm_xml_file_item_destroy(next->data);
                            next->data = NULL; /* we merged it so no data */
                        }
                    }
                }
            }
            /* merge children */
            _merge_tree(fm_xml_file_item_get_children(first->data));
        }
        /* go to next item */
        first = g_list_delete_link(first, first);
    }
}

static FmXmlFileItem *_find_in_children(GList *list, const char *path)
{
    const char *ptr;
    char *_ptr;

    if (list == NULL)
        return NULL;
    g_debug("menu tree: searching for '%s'", path);
    ptr = strchr(path, '/');
    if (ptr == NULL)
    {
        ptr = path;
        path = _ptr = NULL;
    }
    else
    {
        _ptr = g_strndup(path, ptr - path);
        path = ptr + 1;
        ptr = _ptr;
    }
    while (list)
    {
        const char *elem_name = _get_menu_name(list->data);
        /* g_debug("got child %d: %s", fm_xml_file_item_get_tag(list->data), elem_name); */
        if (g_strcmp0(elem_name, ptr) == 0)
            break;
        else
            list = list->next;
    }
    g_free(_ptr);
    if (list && path)
    {
        FmXmlFileItem *item;

        list = fm_xml_file_item_get_children(list->data);
        item = _find_in_children(list, path);
        g_list_free(list);
        return item;
    }
    return list ? list->data : NULL;
}

static inline GList *_find_by_text(GList *list, const char *name)
{
    while (list)
        if (strcmp(list->data, name) == 0)
            return list;
        else
            list = list->next;
    return NULL;
}

/* returns TRUE if item categories add/del meet conditions in item */
static gboolean _is_satisfying(FmXmlFileItem *item, GList *cur, GList *add, GList *del)
{
    FmXmlFileTag tag = fm_xml_file_item_get_tag(item);
    GList *children = fm_xml_file_item_get_children(item), *l;
    gboolean ok = FALSE;

    if (tag == menuTag_Category)
    {
        const char *name;

        item = fm_xml_file_item_find_child(item, FM_XML_FILE_TEXT);
        if (item && (name = fm_xml_file_item_get_data(item, NULL)))
        {
            if (!_find_by_text(del, name) && /* is it in negative list? */
                (_find_by_text(cur, name) || _find_by_text(add, name)))
                ok = TRUE;
        }
        /* else no Category name, it's broken */
    }
    else if (tag == menuTag_Not)
    {
        ok = !_is_satisfying(children->data, cur, add, del);
    }
    else if (tag == menuTag_And)
    {
        for (l = children; l; l = l->next)
            if (!_is_satisfying(l->data, cur, add, del))
                break;
        ok = (l == NULL);
    }
    else if (tag == menuTag_Or)
    {
        for (l = children; l; l = l->next)
            if (_is_satisfying(l->data, cur, add, del))
                break; /* condition satisfied */
        ok = (l != NULL);
    }
    g_list_free(children);
    return ok;
}

static gboolean _unsatisfy_item(FmXmlFileItem *item, GList *cur, GList **add, GList **del);

/* tests if positive meets conditions in item, adds missing to positive
   or to negative */
static gboolean _satisfy_item(FmXmlFileItem *item, GList *cur, GList **add, GList **del)
{
    FmXmlFileTag tag = fm_xml_file_item_get_tag(item);
    GList *children = fm_xml_file_item_get_children(item), *l;
    gboolean ok = FALSE;

    if (tag == menuTag_Category)
    {
        const char *name;

        item = fm_xml_file_item_find_child(item, FM_XML_FILE_TEXT);
        if (item && (name = fm_xml_file_item_get_data(item, NULL)))
        {
            if (!_find_by_text(*del, name)) /* it's not in negative list */
            {
                ok = TRUE;
                g_debug("menu test: category required: %s", name);
                if (!_find_by_text(cur, name) &&
                    !_find_by_text(*add, name)) /* if not in list then add it */
                    *add = g_list_prepend(*add, g_strdup(name));
            }
        }
        /* else no Category name, it's broken */
    }
    else if (tag == menuTag_Not)
    {
        ok = _unsatisfy_item(children->data, cur, add, del);
    }
    else if (tag == menuTag_And)
    {
        GList *temp_p = *add, *temp_n = *del;

        for (l = children; l; l = l->next)
            if (!_satisfy_item(l->data, cur, add, del))
            {
                g_debug("menu test: could not satisfy <And> condition");
                /* restore the list */
                for (l = *add; l && l != temp_p; )
                {
                    g_free(l->data);
                    l = g_list_delete_link(l, l);
                }
                *add = l;
                for (l = *del; l && l != temp_n; )
                {
                    g_free(l->data);
                    l = g_list_delete_link(l, l);
                }
                *del = l;
                break;
            }
        ok = (l == NULL);
    }
    else if (tag == menuTag_Or)
    {
        /* test if it meet condition already */
        for (l = children; l; l = l->next)
            if (_is_satisfying(l->data, cur, *add, *del))
                break; /* condition satisfied */
        if (l == NULL) /* not satisfied yet */
            for (l = children; l; l = l->next)
                if (_satisfy_item(l->data, cur, add, del)) /* try to do */
                    break;
        else
            g_debug("menu test: condition <Or> already satisfied");
        ok = (l != NULL);
    }
    g_list_free(children);
    return ok;
}

/* tests if positive not meets conditions in item, adds missing to positive
   or to negative */
static gboolean _unsatisfy_item(FmXmlFileItem *item, GList *cur, GList **add, GList **del)
{
    FmXmlFileTag tag = fm_xml_file_item_get_tag(item);
    GList *children = fm_xml_file_item_get_children(item), *l;
    gboolean ok = TRUE;

    if (tag == menuTag_Category)
    {
        const char *name;

        item = fm_xml_file_item_find_child(item, FM_XML_FILE_TEXT);
        if (item && (name = fm_xml_file_item_get_data(item, NULL)))
        {
            g_debug("menu test: category unwanted: '%s'", name);
            if (_find_by_text(*add, name)) /* ouch, it's in positive list */
            {
                g_debug("menu test: category disable failed: '%s' is marked to add", name);
                ok = FALSE;
            }
            else if (!_find_by_text(*del, name)) /* if not in list then add it */
                *del = g_list_prepend(*del, g_strdup(name));
        }
        /* else no Category name, it's broken */
    }
    else if (tag == menuTag_Not)
    {
        ok = _satisfy_item(children->data, cur, add, del);
    }
    else if (tag == menuTag_And)
    {
        for (l = children; l; l = l->next)
            if (!_is_satisfying(l->data, cur, *add, *del))
                break; /* condition satisfied */
        if (l == NULL) /* not satisfied yet */
            for (l = children; l; l = l->next)
                if (_unsatisfy_item(l->data, cur, add, del)) /* try to do */
                    break;
        g_debug("menu test: condition <And> to disable, already satisfied");
        ok = (l != NULL);
    }
    else if (tag == menuTag_Or)
    {
        GList *temp_p = *add, *temp_n = *del;

        for (l = children; l; l = l->next)
            if (!_unsatisfy_item(l->data, cur, add, del))
            {
                g_debug("menu test: could not satisfy negative <Or> condition");
                /* restore the list */
                for (l = *add; l && l != temp_p; )
                {
                    g_free(l->data);
                    l = g_list_delete_link(l, l);
                }
                *add = l;
                for (l = *del; l && l != temp_n; )
                {
                    g_free(l->data);
                    l = g_list_delete_link(l, l);
                }
                *del = l;
            }
        ok = (l == NULL);
    }
    g_list_free(children);
    return ok;
}

/* returns 0 if src_directory and dst_directory are equal
   returns -1 in case of error and sets error
   otherwise replaces categories with new list
   src_directory may be NULL
   may remove fileid from .menu XML file
   may return errors G_MARKUP_ERROR, G_IO_ERROR, G_FILE_ERROR */
static int _update_categories(gchar ***categories, const char *src_directory,
                              const char *dst_directory, const char *fileid,
                              GCancellable *cancellable, GError **error)
{
    const char *xdg_menu_prefix;
    GList *lcats, *cats_to_add, *cats_to_del;
    FmMenuMenuTree data;
    GFile *gf;
    char *contents;
    gsize len;
    GList *xml = NULL, *it, *l2, *it2;
    FmXmlFileItem *item;
    gchar **cats, **new_cats;
    int n_cats = 0;
    gboolean ok;

    /* do it in compatibility with lxpanel */
    xdg_menu_prefix = g_getenv("XDG_MENU_PREFIX");
    contents = g_strdup_printf("%sapplications.menu",
                               xdg_menu_prefix ? xdg_menu_prefix : "lxde-");
    /* find first appliable file */
    data.file_path = g_build_filename(g_get_user_config_dir(), "menus", contents, NULL);
    gf = g_file_new_for_path(data.file_path);
    if (!g_file_query_exists(gf, cancellable))
    {
        const gchar * const *dirs = g_get_system_config_dirs();
        g_debug("user Menu file '%s' does not exist", data.file_path);
        while (dirs && dirs[0])
        {
            g_free(data.file_path);
            g_object_unref(gf);
            data.file_path = g_build_filename(dirs[0], "menus", contents, NULL);
            gf = g_file_new_for_path(data.file_path);
            dirs++;
            if (g_file_query_exists(gf, cancellable))
                break;
        }
        g_debug("trying system Menu file: %s", data.file_path);
    }
    g_free(contents); /* we used it temporarily */
    contents = NULL;
    ok = g_file_load_contents(gf, cancellable, &contents, &len, NULL, error);
    g_object_unref(gf);
    if (!ok)
    {
        g_free(data.file_path);
        return -1;
    }
    /* prepare structure */
    G_LOCK(menuTree);
    data.menu = fm_xml_file_new(NULL);
    data.line = data.pos = -1;
    /* g_debug("new FmXmlFile %p", data.menu); */
    menuTag_Menu = fm_xml_file_set_handler(data.menu, "Menu",
                                           &_menu_xml_handler_pass, error);
    menuTag_Include = fm_xml_file_set_handler(data.menu, "Include",
                                              &_menu_xml_handler_pass, error);
    menuTag_Exclude = fm_xml_file_set_handler(data.menu, "Exclude",
                                              &_menu_xml_handler_pass, error);
    menuTag_Filename = fm_xml_file_set_handler(data.menu, "Filename",
                                               &_menu_xml_handler_pass, error);
    menuTag_Or = fm_xml_file_set_handler(data.menu, "Or",
                                         &_menu_xml_handler_pass, error);
    menuTag_And = fm_xml_file_set_handler(data.menu, "And",
                                          &_menu_xml_handler_pass, error);
    menuTag_Not = fm_xml_file_set_handler(data.menu, "Not",
                                          &_menu_xml_handler_Not, error);
    menuTag_Category = fm_xml_file_set_handler(data.menu, "Category",
                                               &_menu_xml_handler_pass, error);
    menuTag_MergeFile = fm_xml_file_set_handler(data.menu, "MergeFile",
                                                &_menu_xml_handler_MergeFile, error);
    menuTag_MergeDir = fm_xml_file_set_handler(data.menu, "MergeDir",
                                               &_menu_xml_handler_MergeDir, error);
    menuTag_DefaultMergeDirs = fm_xml_file_set_handler(data.menu, "DefaultMergeDirs",
                                                       &_menu_xml_handler_DefaultMergeDirs,
                                                       error);
    menuTag_Name = fm_xml_file_set_handler(data.menu, "Name",
                                           &_menu_xml_handler_Name, error);
    data.cancellable = cancellable;
    /* do parsing */
    ok = fm_xml_file_parse_data(data.menu, contents, len, error, &data);
    g_free(contents);
    if (ok)
        xml = fm_xml_file_finish_parse(data.menu, error);
    if (xml == NULL) /* error is set by failed function */
    {
        if (data.line == -1)
            data.line = fm_xml_file_get_current_line(data.menu, &data.pos);
        g_prefix_error(error, _("XML file %s error (%d:%d): "), data.file_path,
                       data.line, data.pos);
        goto _return_error;
    }
    /* contents = fm_xml_file_to_data(data.menu, NULL, NULL);
    g_debug("pre-merge: %s", contents);
    g_free(contents); */
    /* merge menus and get merged list again */
    _merge_tree(xml); /* it will free the list */
    /* contents = fm_xml_file_to_data(data.menu, NULL, NULL);
    g_debug("post-merge: %s", contents);
    g_free(contents); */
    xml = fm_xml_file_finish_parse(data.menu, NULL);
    /* descent into 'Applications' menu */
    item = _find_in_children(xml, "Applications");
    g_list_free(xml);
    if (item == NULL) /* invalid .menu file! */
    {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                            _("XML file doesn't contain Applications root"));
        goto _return_error;
    }
    xml = fm_xml_file_item_get_children(item);
    /* validate dst_directory first */
    item = _find_in_children(xml, dst_directory);
    if (item == NULL) /* no such menu path in XML! */
    {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                    _("cannot find path %s in XML definitions"), dst_directory);
        g_list_free(xml);
_return_error:
        /* g_debug("destroying FmXmlFile %p", data.menu); */
        g_object_unref(data.menu);
        g_free(data.file_path);
        G_UNLOCK(menuTree);
        return -1;
    }
    g_debug("menu test: destination path '%s' is valid", dst_directory);
    /* convert strv into list for convenience */
    lcats = cats_to_del = cats_to_add = NULL;
    for (cats = *categories; *cats; cats++)
        lcats = g_list_prepend(lcats, *cats);
    lcats = g_list_reverse(lcats);
    /* prepare draft 'definitely add' list to work on it:
       add first satisfying And or Category content from first Include */
    {
        GList *list = fm_xml_file_item_get_children(item);
        for (it = list; it; it = it->next)
            if (fm_xml_file_item_get_tag(it->data) == menuTag_Include)
            {
                l2 = fm_xml_file_item_get_children(it->data);
                for (it2 = l2; it2; it2 = it2->next)
                {
                    FmXmlFileTag tag = fm_xml_file_item_get_tag(it2->data);
                    if ((tag == menuTag_And || tag == menuTag_Category) &&
                        _satisfy_item(it2->data, lcats, &cats_to_add, &cats_to_del))
                        break; /* done */
                }
                g_list_free(l2);
                if (it2 != NULL)
                    break; /* done */
            }
        g_list_free(list);
    }
    /* compose categories add/del to remove from src_directory */
    if (src_directory && (item = _find_in_children(xml, src_directory)))
    {
        GList *list = fm_xml_file_item_get_children(item);
        g_debug("menu test: source path '%s' is valid", src_directory);
        /* unsatisfy all Include tags */
        for (it = list; it; it = it->next)
            if (fm_xml_file_item_get_tag(it->data) == menuTag_Include)
            {
                l2 = fm_xml_file_item_get_children(it->data);
                for (it2 = l2; it2; it2 = it2->next)
                    if (!_unsatisfy_item(it2->data, lcats, &cats_to_add, &cats_to_del))
                        break;
                g_list_free(l2);
                if (it2 != NULL)
                    break;
            }
        /* FIXME: handle if file has Include somewhere - add Exclude to .menu */
        g_list_free(list);
        if (it != NULL)
            goto _satisfy_failed;
    }
    /* compose categories to add/del to add into dst_directory */
    item = _find_in_children(xml, dst_directory);
    g_list_free(xml);
    xml = fm_xml_file_item_get_children(item);
    /* try to satisfy any Include we can find, stop on success */
    for (it = xml; it; it = it->next)
        if (fm_xml_file_item_get_tag(it->data) == menuTag_Include)
        {
            l2 = fm_xml_file_item_get_children(it->data);
            for (it2 = l2; it2; it2 = it2->next)
                if (_satisfy_item(it2->data, lcats, &cats_to_add, &cats_to_del))
                    break; /* done */
            g_list_free(l2);
            if (it2 != NULL)
                break; /* done */
        }
    if (it == NULL) /* could satisfy none of them! */
        goto _satisfy_failed;
    /* try to satisfy all Exclude now */
    for (it = xml; it; it = it->next)
        if (fm_xml_file_item_get_tag(it->data) == menuTag_Exclude)
        {
            l2 = fm_xml_file_item_get_children(it->data);
            for (it2 = l2; it2; it2 = it2->next)
                if (!_unsatisfy_item(it2->data, lcats, &cats_to_add, &cats_to_del))
                    break; /* failed */
            g_list_free(l2);
            if (it2 != NULL)
                break; /* failed */
        }
    if (it != NULL)
    {
_satisfy_failed:
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                            _("category manipulation internal error"));
        g_list_free(xml);
        g_object_unref(data.menu);
        g_free(data.file_path);
        G_UNLOCK(menuTree);
        goto _return_cancelled;
    }
    g_list_free(xml);
    /* FIXME: investigate and handle special combinations later */
    /* FIXME: handle if file has Exclude somewhere - add Include to .menu */
    /* if we found fileid in .menu file then rewrite it */
//    if (cancellable && g_cancellable_is_cancelled(cancellable)) ;
//    else if (wants_exclude || wants_include)
//    {
//        g_object_unref(data.menu);
//        data.menu = fm_xml_file_new(NULL);
        /* we should support just few tags here - Exclude and Filename */
//        .......
//    }
    g_object_unref(data.menu);
    g_free(data.file_path);
    G_UNLOCK(menuTree);
    if (g_cancellable_set_error_if_cancelled(cancellable, error))
    {
_return_cancelled:
        g_list_free_full(cats_to_add, g_free);
        g_list_free_full(cats_to_del, g_free);
        g_list_free(lcats);
        return -1;
    }
    /* do add/delete */
    for (it = lcats; it != NULL && cats_to_del != NULL; )
    {
        /* remove cats_to_del from lcats */
        l2 = it->next;
        for (it2 = cats_to_del; it2; it2 = it2->next)
        {
            if (strcmp(it->data, it2->data) == 0)
            {
                g_debug("deleting category %s", (char *)it->data);
                g_free(it->data);
                lcats = g_list_delete_link(lcats, it);
                g_free(it2->data);
                cats_to_del = g_list_delete_link(cats_to_del, it2);
                break;
            }
        }
        it = l2; /* go to next */
    }
    /* make new list from cats_to_add+lcats */
    lcats = g_list_concat(cats_to_add, lcats);
    n_cats = g_list_length(lcats);
    new_cats = g_new(char *, n_cats + 1);
    for (cats = new_cats, it = lcats; it != NULL; cats++, it = it->next)
        *cats = it->data; /* move strings */
    *cats = NULL; /* terminate the list */
    g_free(*categories); /* members were freed above */
    *categories = new_cats; /* replace the list */
    g_list_free_full(cats_to_del, g_free); /* if there are any left */
    g_list_free(lcats); /* members are used in new_cats */
    return n_cats;
}

#if 0
static void _set_default_contents(FmXmlFile *file, const char *basename)
{
    /* set DTD:
        "Menu PUBLIC '-//freedesktop//DTD Menu 1.0//EN'\n"
        " 'http://www.freedesktop.org/standards/menu-spec/menu-1.0.dtd'"
    */
    /* set content:
        <Menu>
            <Name>Applications</Name>
            <MergeFile type='parent'>%s</MergeFile>
        </Menu>
    */
}

/* changes .menu XML file */
static gboolean _remove_directory(const char *path)
{
    //if file doesn't exist then it should be created with default contents
    //if path is found and has <NotDeleted/> then replace it with <Deleted/>
    //else create path and add <Deleted/> to it
}

/* changes .menu XML file */
static gboolean _add_directory(const char *path, const char *name)
{
    //if file doesn't exist then it should be created with default contents
    //if path is found and has <Deleted/> then just remove that tag
    //else create path and add <Name>... and <Directory>... and <Include><Category>X-...
}

/* changes .menu XML file */
static gboolean _set_directory_category(const char *path)
{
    //if file doesn't exist then it should be created with default contents
    //if path is not found then create it
    //add <Include><Category>X-...
}
#endif


/* ---- FmMenuVFileOutputStream class ---- */
#define FM_TYPE_MENU_VFILE_OUTPUT_STREAM  (fm_vfs_menu_file_output_stream_get_type())
#define FM_MENU_VFILE_OUTPUT_STREAM(o)    (G_TYPE_CHECK_INSTANCE_CAST((o), \
                                           FM_TYPE_MENU_VFILE_OUTPUT_STREAM, \
                                           FmMenuVFileOutputStream))

typedef struct _FmMenuVFileOutputStream      FmMenuVFileOutputStream;
typedef struct _FmMenuVFileOutputStreamClass FmMenuVFileOutputStreamClass;

struct _FmMenuVFileOutputStream
{
    GFileOutputStream parent;
    GOutputStream *real_stream;
    gchar *path; /* base directory in menu */
    GString *content;
    gboolean do_close;
};

struct _FmMenuVFileOutputStreamClass
{
    GFileOutputStreamClass parent_class;
};

static GType fm_vfs_menu_file_output_stream_get_type  (void);

G_DEFINE_TYPE(FmMenuVFileOutputStream, fm_vfs_menu_file_output_stream, G_TYPE_FILE_OUTPUT_STREAM);

static void fm_vfs_menu_file_output_stream_finalize(GObject *object)
{
    FmMenuVFileOutputStream *stream = FM_MENU_VFILE_OUTPUT_STREAM(object);
    if(stream->real_stream)
        g_object_unref(stream->real_stream);
    g_free(stream->path);
    g_string_free(stream->content, TRUE);
    G_OBJECT_CLASS(fm_vfs_menu_file_output_stream_parent_class)->finalize(object);
}

static gssize fm_vfs_menu_file_output_stream_write(GOutputStream *stream,
                                                   const void *buffer, gsize count,
                                                   GCancellable *cancellable,
                                                   GError **error)
{
    if (g_cancellable_set_error_if_cancelled(cancellable, error))
        return -1;
    g_string_append_len(FM_MENU_VFILE_OUTPUT_STREAM(stream)->content, buffer, count);
    return (gssize)count;
}

static gboolean fm_vfs_menu_file_output_stream_close(GOutputStream *gos,
                                                     GCancellable *cancellable,
                                                     GError **error)
{
    FmMenuVFileOutputStream *stream = FM_MENU_VFILE_OUTPUT_STREAM(gos);
    GKeyFile *kf;
    gchar **categories;
    gsize len = 0;
    int i;
    gchar *content;
    gboolean ok;

    if (g_cancellable_set_error_if_cancelled(cancellable, error))
        return FALSE;
    if (!stream->do_close)
        return TRUE;
    kf = g_key_file_new();
    /* parse entered file content first */
    if (stream->content->len > 0)
        g_key_file_load_from_data(kf, stream->content->str, stream->content->len,
                                  G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                                  NULL); /* FIXME: don't ignore some errors? */
    /* correct invalid data in desktop entry file: Name and Exec are mandatory,
       Type must be Application, and Category should include requested one */
    if(!g_key_file_has_key(kf, G_KEY_FILE_DESKTOP_GROUP,
                           G_KEY_FILE_DESKTOP_KEY_NAME, NULL))
        g_key_file_set_string(kf, G_KEY_FILE_DESKTOP_GROUP,
                              G_KEY_FILE_DESKTOP_KEY_NAME, "");
    if(!g_key_file_has_key(kf, G_KEY_FILE_DESKTOP_GROUP,
                           G_KEY_FILE_DESKTOP_KEY_EXEC, NULL))
        g_key_file_set_string(kf, G_KEY_FILE_DESKTOP_GROUP,
                              G_KEY_FILE_DESKTOP_KEY_EXEC, "");
    g_key_file_set_string(kf, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TYPE,
                          G_KEY_FILE_DESKTOP_TYPE_APPLICATION);
    categories = g_key_file_get_string_list(kf, G_KEY_FILE_DESKTOP_GROUP,
                                            G_KEY_FILE_DESKTOP_KEY_CATEGORIES,
                                            &len, NULL);
    if (stream->path)
        i = _update_categories(&categories, NULL, stream->path, NULL, cancellable, error);
    else
        i = 0;
    if (i < 0) /* .menu file error */
    {
        g_key_file_free(kf);
        return FALSE;
    }
    if (i > 0)
        g_key_file_set_string_list(kf, G_KEY_FILE_DESKTOP_GROUP,
                                   G_KEY_FILE_DESKTOP_KEY_CATEGORIES,
                                   (const gchar* const*)categories, i);
    g_strfreev(categories);
    content = g_key_file_to_data(kf, &len, error);
    g_key_file_free(kf);
    if (!content)
        return FALSE;
    ok = g_output_stream_write_all(stream->real_stream, content, len, &len,
                                   cancellable, error);
    g_free(content);
    if (!ok || !g_output_stream_close(stream->real_stream, cancellable, error))
        return FALSE;
    stream->do_close = FALSE;
    return TRUE;
}

static void fm_vfs_menu_file_output_stream_class_init(FmMenuVFileOutputStreamClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GOutputStreamClass *stream_class = G_OUTPUT_STREAM_CLASS(klass);

    gobject_class->finalize = fm_vfs_menu_file_output_stream_finalize;
    stream_class->write_fn = fm_vfs_menu_file_output_stream_write;
    stream_class->close_fn = fm_vfs_menu_file_output_stream_close;
    /* we don't implement seek/truncate/etag/query so no GFileOutputStream funcs */
}

static void fm_vfs_menu_file_output_stream_init(FmMenuVFileOutputStream *stream)
{
    stream->content = g_string_sized_new(1024);
    stream->do_close = TRUE;
}

static FmMenuVFileOutputStream *_fm_vfs_menu_file_output_stream_new(const gchar *directory)
{
    FmMenuVFileOutputStream *stream;

    stream = g_object_new(FM_TYPE_MENU_VFILE_OUTPUT_STREAM, NULL);
    if (directory)
        stream->path = g_strdup(directory);
    return stream;
}

static GFileOutputStream *_vfile_menu_create(GFile *file,
                                             GFileCreateFlags flags,
                                             GCancellable *cancellable,
                                             GError **error,
                                             const gchar *directory)
{
    FmMenuVFileOutputStream *stream;
    GFileOutputStream *ostream;

    if (g_cancellable_set_error_if_cancelled(cancellable, error))
        return NULL;
//    g_file_delete(file, cancellable, NULL); /* remove old if there is any */
    stream = _fm_vfs_menu_file_output_stream_new(directory);
    ostream = g_file_create(file, flags, cancellable, error);
    if (ostream == NULL)
    {
        g_object_unref(stream);
        return NULL;
    }
    stream->real_stream = G_OUTPUT_STREAM(ostream);
    return (GFileOutputStream*)stream;
}

static GFileOutputStream *_vfile_menu_replace(GFile *file,
                                              const char *etag,
                                              gboolean make_backup,
                                              GFileCreateFlags flags,
                                              GCancellable *cancellable,
                                              GError **error,
                                              const gchar *directory)
{
    FmMenuVFileOutputStream *stream;
    GFileOutputStream *ostream;

    if (g_cancellable_set_error_if_cancelled(cancellable, error))
        return NULL;
    stream = _fm_vfs_menu_file_output_stream_new(directory);
    ostream = g_file_replace(file, etag, make_backup, flags, cancellable, error);
    if (ostream == NULL)
    {
        g_object_unref(stream);
        return NULL;
    }
    stream->real_stream = G_OUTPUT_STREAM(ostream);
    return (GFileOutputStream*)stream;
}

static gboolean _fm_vfs_menu_create_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    MenuCache *mc;
    char *unescaped = NULL, *id;
    gboolean is_invalid = TRUE;

    init->result = NULL;
    if(init->path_str)
    {
        MenuCacheItem *item;
#if !MENU_CACHE_CHECK_VERSION(0, 5, 0)
        GSList *list, *l;
#endif

        mc = _get_menu_cache(init->error);
        if(mc == NULL)
            goto _mc_failed;
        unescaped = g_uri_unescape_string(init->path_str, NULL);
        /* ensure new menu item has suffix .desktop */
        if (!g_str_has_suffix(unescaped, ".desktop"))
        {
            id = unescaped;
            unescaped = g_strconcat(unescaped, ".desktop", NULL);
            g_free(id);
        }
        id = strrchr(unescaped, '/');
        if (id)
        {
            *id++ = '\0';
#if MENU_CACHE_CHECK_VERSION(0, 5, 0)
            item = menu_cache_find_item_by_id(mc, id);
            menu_cache_item_unref(item); /* use item simply as marker */
#else
            list = menu_cache_list_all_apps(mc);
            for (l = list; l; l = l->next)
                if (strcmp(menu_cache_item_get_id(l->data), id) == 0)
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
        /* g_debug("create id %s, category %s", id, category); */
        menu_cache_unref(mc);
    }

    if(is_invalid)
        g_set_error(init->error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                    _("Cannot create menu item \"%s\""),
                    init->path_str ? init->path_str : "/");
    else
    {
        GFile *gf = _g_file_new_for_id(id);

        if (gf)
        {
            init->result = _vfile_menu_create(gf, G_FILE_CREATE_NONE,
                                              init->cancellable, init->error,
                                              unescaped);
            g_object_unref(gf);
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

    /* g_debug("_fm_vfs_menu_create %s", item->path); */
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
    char *unescaped = NULL, *id, *directory = "";
    gboolean is_invalid = TRUE;

    init->result = NULL;
    if(init->path_str)
    {
        MenuCacheItem *item, *item2;

        mc = _get_menu_cache(init->error);
        if(mc == NULL)
            goto _mc_failed;
        /* prepare id first */
        unescaped = g_uri_unescape_string(init->path_str, NULL);
        id = strrchr(unescaped, '/');
        if (id != NULL)
        {
            *id++ = '\0';
            directory = unescaped;
        }
        /* get existing item */
        item = _vfile_path_to_menu_cache_item(mc, init->path_str);
        /* if not found then check item by id to exclude conflicts */
        if (item != NULL) /* item is there, OK, we'll replace it then */
            is_invalid = FALSE;
        else if (id != NULL)
        {
#if MENU_CACHE_CHECK_VERSION(0, 5, 0)
            item2 = menu_cache_find_item_by_id(mc, id);
#else
            GSList *list = menu_cache_list_all_apps(mc), *l;
            for (l = list; l; l = l->next)
                if (strcmp(menu_cache_item_get_id(l->data), id) == 0)
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
        /* if id is NULL then we trying to create item in root, i.e.
           outside of categories and that should be prohibited */
        menu_cache_unref(mc);
    }

    if(is_invalid)
        g_set_error(init->error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                    _("Cannot create menu item \"%s\""),
                    init->path_str ? init->path_str : "/");
    else
    {
        GFile *gf = _g_file_new_for_id(id);

        if (gf)
        {
            /* FIXME: use flags and make_backup */
            init->result = _vfile_menu_replace(gf, NULL, FALSE,
                                               G_FILE_CREATE_REPLACE_DESTINATION,
                                               init->cancellable, init->error,
                                               directory);
            g_object_unref(gf);
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

    /* g_debug("_fm_vfs_menu_replace %s", item->path); */
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
    gboolean result = FALSE;

    if(init->path_str == NULL)
    {
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Cannot delete root directory"));
        return FALSE;
    }
    mc = _get_menu_cache(init->error);
    if(mc == NULL)
        return FALSE;
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
    gf = _g_file_new_for_id(menu_cache_item_get_id(item));
    out = G_OUTPUT_STREAM(g_file_replace(gf, NULL, FALSE,
                                         G_FILE_CREATE_REPLACE_DESTINATION,
                                         init->cancellable, init->error));
    g_object_unref(gf);
    if (out == NULL)
    {
        g_free(contents);
        goto _failed;
    }
    result = g_output_stream_write_all(out, contents, length, &tmp_len,
                                       init->cancellable, init->error);
    g_free(contents);
    if (result) /* else nothing to close */
        g_output_stream_close(out, init->cancellable, init->error);
    g_object_unref(out);

_failed:
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    if(item)
        menu_cache_item_unref(item);
#endif
    menu_cache_unref(mc);
    return result;
}

static gboolean _fm_vfs_menu_delete_file(GFile *file,
                                         GCancellable *cancellable,
                                         GError **error)
{
    FmMenuVFile *item = FM_MENU_VFILE(file);
    FmVfsMenuMainThreadData enu;

    /* g_debug("_fm_vfs_menu_delete_file %s", item->path); */
    enu.path_str = item->path;
    enu.cancellable = cancellable;
    enu.error = error;
    return fm_run_in_default_main_context(_fm_vfs_menu_delete_real, &enu);
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

static gboolean _fm_vfs_menu_move_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    MenuCache *mc = NULL;
    MenuCacheItem *item = NULL;
    char *src_path, *dst_path;
    char *src_id, *dst_id;
    const char *src_category, *dst_category;
    char *file_path, *contents;
    gchar **categories;
    gsize len = 0;
    int i;
    GKeyFile *kf;
    GFile *gf;
    GOutputStream *out;
    gsize length, tmp_len;
    gboolean result = FALSE;

    dst_path = init->destination->path;
    if (init->path_str == NULL || dst_path == NULL)
    {
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Invalid operation with menu root"));
        return FALSE;
    }
    /* make path strings */
    src_path = g_uri_unescape_string(init->path_str, NULL);
    dst_path = g_uri_unescape_string(dst_path, NULL);
    src_id = strrchr(src_path, '/');
    if (src_id)
    {
        *src_id++ = '\0';
        src_category = src_path;
    }
    else
    {
        src_id = src_path;
        src_category = NULL;
    }
    dst_id = strrchr(dst_path, '/');
    if (dst_id)
    {
        *dst_id++ = '\0';
        dst_category = dst_path;
    }
    else
    {
        dst_id = dst_path;
        dst_category = NULL;
    }
    if (strcmp(src_id, dst_id))
    {
        /* ID change isn't supported now */
        ERROR_UNSUPPORTED(init->error);
        goto _failed;
    }
    if (src_category == NULL || dst_category == NULL)
    {
        /* we cannot move base categories now */
        ERROR_UNSUPPORTED(init->error);
        goto _failed;
    }
    if (strcmp(src_path, dst_path) == 0)
    {
        g_warning("menu: tried to move '%s' into itself", src_path);
        g_free(src_path);
        g_free(dst_path);
        return TRUE; /* nothing was changed */
    }
    /* do actual move */
    mc = _get_menu_cache(init->error);
    if(mc == NULL)
        goto _failed;
    item = _vfile_path_to_menu_cache_item(mc, init->path_str);
    /* TODO: if id changed then check for ID conflicts */
    /* TODO: save updated desktop entry for old ID (if different) */
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
    categories = g_key_file_get_string_list(kf, G_KEY_FILE_DESKTOP_GROUP,
                                            G_KEY_FILE_DESKTOP_KEY_CATEGORIES,
                                            &len, NULL);
    /* remove old category from list */
    i = _update_categories(&categories, src_category, dst_category, src_id,
                           init->cancellable, init->error);
    if (i < 0)
    {
        g_strfreev(categories);
        g_key_file_free(kf);
        goto _failed;
    }
    if (i > 0)
        g_key_file_set_string_list(kf, G_KEY_FILE_DESKTOP_GROUP,
                                   G_KEY_FILE_DESKTOP_KEY_CATEGORIES,
                                   (const gchar* const*)categories, i);
    g_strfreev(categories);
    contents = g_key_file_to_data(kf, &length, init->error);
    g_key_file_free(kf);
    if (contents == NULL)
        goto _failed;
    /* save updated desktop entry for the new ID */
    gf = _g_file_new_for_id(menu_cache_item_get_id(item));
    out = G_OUTPUT_STREAM(_vfile_menu_replace(gf, NULL, FALSE,
                                              G_FILE_CREATE_REPLACE_DESTINATION,
                                              init->cancellable, init->error,
                                              NULL));
    g_object_unref(gf);
    if (out == NULL)
    {
        g_free(contents);
        goto _failed;
    }
    result = g_output_stream_write_all(out, contents, length, &tmp_len,
                                       init->cancellable, init->error);
    g_free(contents);
    if (result) /* else nothing to close */
        g_output_stream_close(out, init->cancellable, init->error);
    g_object_unref(out);

_failed:
#if MENU_CACHE_CHECK_VERSION(0, 4, 0)
    if(item)
        menu_cache_item_unref(item);
#endif
    if(mc)
        menu_cache_unref(mc);
    g_free(src_path);
    g_free(dst_path);
    return result;
}

static gboolean _fm_vfs_menu_move(GFile *source,
                                  GFile *destination,
                                  GFileCopyFlags flags,
                                  GCancellable *cancellable,
                                  GFileProgressCallback progress_callback,
                                  gpointer progress_callback_data,
                                  GError **error)
{
    FmMenuVFile *item = FM_MENU_VFILE(source);
    FmVfsMenuMainThreadData enu;

    /* g_debug("_fm_vfs_menu_move"); */
    if(!FM_IS_FILE(destination))
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            _("Invalid destination"));
        return FALSE;
    }
    enu.path_str = item->path;
    enu.cancellable = cancellable;
    enu.error = error;
    // enu.flags = flags;
    enu.destination = FM_MENU_VFILE(destination);
    /* FIXME: use progress_callback */
    return fm_run_in_default_main_context(_fm_vfs_menu_move_real, &enu);
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
    for (ol = items; ol; ) /* remove all separatorts first */
    {
        nl = ol->next;
        if (menu_cache_item_get_id(ol->data) == NULL)
        {
            menu_cache_item_unref(ol->data);
            items = g_slist_delete_link(items, ol);
        }
        ol = nl;
    }
    for (ol = new_items; ol; )
    {
        nl = ol->next;
        if (menu_cache_item_get_id(ol->data) == NULL)
        {
            menu_cache_item_unref(ol->data);
            new_items = g_slist_delete_link(new_items, ol);
        }
        ol = nl;
    }
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
    mon->cache = _get_menu_cache(error);
    if(mon->cache == NULL)
        goto _fail;
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

#if 0
static gboolean _fm_vfs_menu_set_icon(GFile* file, GIcon *icon)
{
    //change icon should be supported at least for directory
}
#endif

static void fm_menu_fm_file_init(FmFileInterface *iface)
{
    iface->wants_incremental = _fm_vfs_menu_wants_incremental;
    //iface->set_icon = _fm_vfs_menu_set_icon;
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
    while(*uri == '/') /* skip starting slashes */
        uri++;
    /* save the rest of path, NULL means the root path */
    if(*uri)
    {
        char *end;

        item->path = g_strdup(uri);
        for(end = item->path + strlen(item->path); end > item->path; end--)
            if(end[-1] == '/') /* skip trailing slashes */
                end[-1] = '\0';
            else
                break;
    }
    return (GFile*)item;
}

FmFileInitTable _fm_vfs_menu_init_table =
{
    .new_for_uri = &_fm_vfs_menu_new_for_uri
};
