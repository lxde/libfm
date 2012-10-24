/*
 *      fm-vfs-menu.c
 *      VFS for "menu://applications/" path using menu-cache library.
 *
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

#include "fm-vfs-menu.h"

#include <glib/gi18n-lib.h>
#include <menu-cache/menu-cache.h>
#include "fm-utils.h"

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
    enu->child = child;

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

static gboolean _fm_vfs_menu_enumerator_new_real(gpointer data)
{
    FmVfsMenuMainThreadData *init = data;
    FmVfsMenuEnumerator *enumerator;
    MenuCache* mc;
    const char *de_name;
    MenuCacheDir *dir;

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
    {
        char *unescaped, *tmp;
        unescaped = g_uri_unescape_string(init->path_str, NULL);
        tmp = g_strconcat("/", menu_cache_item_get_id(MENU_CACHE_ITEM(menu_cache_get_root_dir(mc))),
                          "/", unescaped, NULL);
        g_free(unescaped);
        dir = menu_cache_get_dir_from_path(mc, tmp);
        /* FIXME: test if path is valid since menu-cache is buggy */
        g_free(tmp);
    }
    else
        dir = menu_cache_get_root_dir(mc);
    if(dir)
        enumerator->child = menu_cache_dir_get_children(dir);
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
    MenuCacheDir *dir;
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
        char *unescaped, *tmp;
        const char *id;

        unescaped = g_uri_unescape_string(init->path_str, NULL);
        tmp = g_strconcat("/", menu_cache_item_get_id(MENU_CACHE_ITEM(menu_cache_get_root_dir(mc))),
                          "/", unescaped, NULL);
        /* FIXME: how to access not dir? */
        dir = menu_cache_get_dir_from_path(mc, tmp);
        /* The menu-cache is buggy and returns parent for invalid path
           instead of failure so we check what we got here.
           Unfortunately we cannot detect if requested name is the same
           as its parent and menu-cache returned the parent. */
        id = strrchr(unescaped, '/');
        if(id)
            id++;
        else
            id = unescaped;
        if(dir == NULL ||
           strcmp(id, menu_cache_item_get_id(MENU_CACHE_ITEM(dir))) != 0)
            is_invalid = TRUE;
        g_free(unescaped);
        g_free(tmp);
    }
    else
        dir = menu_cache_get_root_dir(mc);
    if(is_invalid)
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                            _("Invalid menu directory"));
    else if(dir)
        init->result = _g_file_info_from_menu_cache_item(MENU_CACHE_ITEM(dir));
    else /* menu_cache_get_root_dir failed */
        g_set_error_literal(init->error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Menu cache error"));

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
    char *basename;
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
            g_file_info_set_name(info, basename);
            g_free(basename);
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

static GFileInputStream *_fm_vfs_menu_read_fn(GFile *file,
                                              GCancellable *cancellable,
                                              GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static GFileOutputStream *_fm_vfs_menu_append_to(GFile *file,
                                                 GFileCreateFlags flags,
                                                 GCancellable *cancellable,
                                                 GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static GFileOutputStream *_fm_vfs_menu_create(GFile *file,
                                              GFileCreateFlags flags,
                                              GCancellable *cancellable,
                                              GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static GFileOutputStream *_fm_vfs_menu_replace(GFile *file,
                                               const char *etag,
                                               gboolean make_backup,
                                               GFileCreateFlags flags,
                                               GCancellable *cancellable,
                                               GError **error)
{
    ERROR_UNSUPPORTED(error);
    return NULL;
}

static gboolean _fm_vfs_menu_delete_file(GFile *file,
                                         GCancellable *cancellable,
                                         GError **error)
{
    ERROR_UNSUPPORTED(error);
    return FALSE;
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

static GFileMonitor *_fm_vfs_menu_monitor_dir(GFile *file,
                                              GFileMonitorFlags flags,
                                              GCancellable *cancellable,
                                              GError **error)
{
    ERROR_UNSUPPORTED(error);
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
