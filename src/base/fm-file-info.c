/*
 *      fm-file-info.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <menu-cache.h>
#include "fm-file-info.h"
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <grp.h> /* Query group name */
#include <pwd.h> /* Query user name */
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "fm-utils.h"

#define COLLATE_USING_DISPLAY_NAME    ((char*)-1)

static gboolean use_si_prefix = TRUE;
static FmMimeType* desktop_entry_type = NULL;

struct _FmFileInfo
{
    FmPath* path; /* path of the file */

    mode_t mode;
    union {
        const char* fs_id;
        dev_t dev;
    };
    uid_t uid;
    gid_t gid;
    goffset size;
    time_t mtime;
    time_t atime;

    gulong blksize;
    goffset blocks;

    char* disp_name;  /* displayed name (in UTF-8) */

    /* FIXME: caching the collate key can greatly speed up sorting.
     *        However, memory usage is greatly increased!.
     *        Is there a better alternative solution?
     */
    char* collate_key; /* used to sort files by name */
    char* disp_size;  /* displayed human-readable file size */
    char* disp_mtime; /* displayed last modification time */
    FmMimeType* mime_type;
    FmIcon* icon;

    char* target; /* target of shortcut or mountable. */

    /*<private>*/
    int n_ref;
};

struct _FmFileInfoList
{
    FmList list;
};

/* intialize the file info system */
void _fm_file_info_init(void)
{
    desktop_entry_type = fm_mime_type_from_name("application/x-desktop");
}

void _fm_file_info_finalize()
{
    fm_mime_type_unref(desktop_entry_type);
}

/**
 * fm_file_info_new:
 *
 * Returns: a new FmFileInfo struct which needs to be freed with
 * fm_file_info_unref() when it's no more needed.
 */
FmFileInfo* fm_file_info_new ()
{
    FmFileInfo * fi = g_slice_new0(FmFileInfo);
    fi->n_ref = 1;
    return fi;
}

/**
 * fm_file_info_set_from_native_file:
 * @fi:  A FmFileInfo struct
 * @path:  full path of the file
 * @err: a GError** to retrive errors
 *
 * Get file info of the specified native file and store it in
 * the FmFileInfo struct.
 *
 * Returns: TRUE if no error happens.
 */
gboolean fm_file_info_set_from_native_file(FmFileInfo* fi, const char* path, GError** err)
{
    struct stat st;
    if(lstat(path, &st) == 0)
    {
        /* By default we use the real file base name for display.
         * FIXME: if the base name is not in UTF-8 encoding, we
         * need to convert it to UTF-8 for display and save its
         * UTF-8 version in fi->display_name */
        fi->disp_name = NULL;
        fi->mode = st.st_mode;
        fi->mtime = st.st_mtime;
        fi->atime = st.st_atime;
        fi->size = st.st_size;
        fi->dev = st.st_dev;
        fi->uid = st.st_uid;
        fi->gid = st.st_gid;

        /* FIXME: handle symlinks */
        if(S_ISLNK(st.st_mode))
        {
            stat(path, &st);
            fi->target = g_file_read_link(path, NULL);
        }

        fi->mime_type = fm_mime_type_from_native_file(path, fm_file_info_get_disp_name(fi), &st);

        /* special handling for desktop entry files */
        if(G_UNLIKELY(fm_file_info_is_desktop_entry(fi)))
        {
            char* fpath = fm_path_to_str(fi->path);
            GKeyFile* kf = g_key_file_new();
            FmIcon* icon = NULL;
            if(g_key_file_load_from_file(kf, fpath, 0, NULL))
            {
                char* icon_name = g_key_file_get_locale_string(kf, "Desktop Entry", "Icon", NULL, NULL);
                char* title = g_key_file_get_locale_string(kf, "Desktop Entry", "Name", NULL, NULL);
                if(icon_name)
                {
                    if(icon_name[0] != '/') /* this is a icon name, not a full path to icon file. */
                    {
                        char* dot = strrchr(icon_name, '.');
                        /* remove file extension */
                        if(dot)
                        {
                            ++dot;
                            if(strcmp(dot, "png") == 0 ||
                               strcmp(dot, "svg") == 0 ||
                               strcmp(dot, "xpm") == 0)
                               *(dot-1) = '\0';
                        }
                    }
                    icon = fm_icon_from_name(icon_name);
                    g_free(icon_name);
                }
                if(title) /* Use title of the desktop entry for display */
                    fi->disp_name = title;
            }
            if(icon)
                fi->icon = icon;
            else
                fi->icon = fm_icon_ref(fi->mime_type->icon);
            g_key_file_free(kf);
            g_free(fpath);
        }
        else
            fi->icon = fm_icon_ref(fi->mime_type->icon);
    }
    else
    {
        g_set_error(err, G_IO_ERROR, g_io_error_from_errno(errno), "%s", g_strerror(errno));
        return FALSE;
    }
    return TRUE;
}

/**
 * fm_file_info_set_from_gfileinfo:
 * @fi:  A FmFileInfo struct
 * @inf: a GFileInfo object
 *
 * Get file info from the GFileInfo object and store it in
 * the FmFileInfo struct.
 */
void fm_file_info_set_from_gfileinfo(FmFileInfo* fi, GFileInfo* inf)
{
    const char* tmp;
    GIcon* gicon;
    GFileType type;

    g_return_if_fail(fi->path);

    /* if display name is the same as its name, just use it. */
    tmp = g_file_info_get_display_name(inf);
    if(strcmp(tmp, fi->path->name) == 0)
        fi->disp_name = NULL;
    else
        fi->disp_name = g_strdup(tmp);

    fi->size = g_file_info_get_size(inf);

    tmp = g_file_info_get_content_type(inf);
    if(tmp)
        fi->mime_type = fm_mime_type_from_name(tmp);

    fi->mode = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_MODE);

    fi->uid = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_UID);
    fi->gid = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_GID);

    type = g_file_info_get_file_type(inf);
    if(0 == fi->mode) /* if UNIX file mode is not available, compose a fake one. */
    {
        switch(type)
        {
        case G_FILE_TYPE_REGULAR:
            fi->mode |= S_IFREG;
            break;
        case G_FILE_TYPE_DIRECTORY:
            fi->mode |= S_IFDIR;
            break;
        case G_FILE_TYPE_SYMBOLIC_LINK:
            fi->mode |= S_IFLNK;
            break;
        case G_FILE_TYPE_SHORTCUT:
            break;
        case G_FILE_TYPE_MOUNTABLE:
            break;
        case G_FILE_TYPE_SPECIAL:
            if(fi->mode)
                break;
        /* if it's a special file but it doesn't have UNIX mode, compose a fake one. */
            if(strcmp(tmp, "inode/chardevice")==0)
                fi->mode |= S_IFCHR;
            else if(strcmp(tmp, "inode/blockdevice")==0)
                fi->mode |= S_IFBLK;
            else if(strcmp(tmp, "inode/fifo")==0)
                fi->mode |= S_IFIFO;
        #ifdef S_IFSOCK
            else if(strcmp(tmp, "inode/socket")==0)
                fi->mode |= S_IFSOCK;
        #endif
            break;
        case G_FILE_TYPE_UNKNOWN:
            ;
        }
    }

    /* set file icon according to mime-type */
    if(!fi->mime_type || !fi->mime_type->icon)
    {
        gicon = g_file_info_get_icon(inf);
        fi->icon = fm_icon_from_gicon(gicon);
        /* g_object_unref(gicon); this is not needed since
         * g_file_info_get_icon didn't increase ref_count.
         * the object returned by g_file_info_get_icon is
         * owned by GFileInfo. */
    }
    else
        fi->icon = fm_icon_ref(fi->mime_type->icon);

    if(type == G_FILE_TYPE_MOUNTABLE || G_FILE_TYPE_SHORTCUT)
    {
        const char* uri = g_file_info_get_attribute_string(inf, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
        if(uri)
        {
            if(g_str_has_prefix(uri, "file:/"))
                fi->target = g_filename_from_uri(uri, NULL, NULL);
            else
                fi->target = g_strdup(uri);
        }

        if(!fi->mime_type)
        {
            /* FIXME: is this appropriate? */
            if(type == G_FILE_TYPE_SHORTCUT)
                fi->mime_type = fm_mime_type_ref(_fm_mime_type_get_inode_x_shortcut());
            else
                fi->mime_type = fm_mime_type_ref(_fm_mime_type_get_inode_x_mountable());
        }
        /* FIXME: how about target of symlinks? */
    }

    if(fm_path_is_native(fi->path))
    {
        fi->dev = g_file_info_get_attribute_uint32(inf, G_FILE_ATTRIBUTE_UNIX_DEVICE);
    }
    else
    {
        tmp = g_file_info_get_attribute_string(inf, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
        fi->fs_id = g_intern_string(tmp);
    }

    fi->mtime = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    fi->atime = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_TIME_ACCESS);
}


/**
 * fm_file_info_new_from_gfileinfo:
 * @path:  FmPath of a file
 * @inf: a GFileInfo object
 *
 * Create a new FmFileInfo for file pointed by @path based on
 * information stored in the GFileInfo object.
 *
 * Returns: A new FmFileInfo struct which should be freed with
 * fm_file_info_unref() when no longer needed.
 */
FmFileInfo* fm_file_info_new_from_gfileinfo(FmPath* path, GFileInfo* inf)
{
    FmFileInfo* fi = fm_file_info_new();
    fi->path = fm_path_ref(path);
    fm_file_info_set_from_gfileinfo(fi, inf);
    return fi;
}

void fm_file_info_set_from_menu_cache_item(FmFileInfo* fi, MenuCacheItem* item)
{
    const char* icon_name = menu_cache_item_get_icon(item);
    fi->disp_name = g_strdup(menu_cache_item_get_name(item));
    if(icon_name)
    {
        char* tmp_name = NULL;
        if(icon_name[0] != '/') /* this is a icon name, not a full path to icon file. */
        {
            char* dot = strrchr(icon_name, '.');
            /* remove file extension, this is a hack to fix non-standard desktop entry files */
            if(G_UNLIKELY(dot))
            {
                ++dot;
                if(strcmp(dot, "png") == 0 ||
                   strcmp(dot, "svg") == 0 ||
                   strcmp(dot, "xpm") == 0)
                {
                    tmp_name = g_strndup(icon_name, dot - icon_name - 1);
                    icon_name = tmp_name;
                }
            }
        }
        fi->icon = fm_icon_from_name(icon_name);
        if(G_UNLIKELY(tmp_name))
            g_free(tmp_name);
    }
    if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_DIR)
    {
        fi->mode |= S_IFDIR;
    }
    else if(menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP)
    {
        fi->mode |= S_IFREG;
        fi->target = menu_cache_item_get_file_path(item);
    }
    fi->mime_type = fm_mime_type_ref(_fm_mime_type_get_inode_x_shortcut());
}

FmFileInfo* fm_file_info_new_from_menu_cache_item(FmPath* path, MenuCacheItem* item)
{
    FmFileInfo* fi = fm_file_info_new();
    fi->path = fm_path_ref(path);
    fm_file_info_set_from_menu_cache_item(fi, item);
    return fi;
}

static void fm_file_info_clear(FmFileInfo* fi)
{
    if(fi->collate_key)
    {
        if(fi->collate_key != COLLATE_USING_DISPLAY_NAME)
            g_free(fi->collate_key);
        fi->collate_key = NULL;
    }

    if(G_LIKELY(fi->path))
    {
        fm_path_unref(fi->path);
        fi->path = NULL;
    }

    if(G_UNLIKELY(fi->disp_name))
    {
        g_free(fi->disp_name);
        fi->disp_name = NULL;
    }

    if(G_LIKELY(fi->disp_size))
    {
        g_free(fi->disp_size);
        fi->disp_size = NULL;
    }

    if(G_UNLIKELY(fi->disp_mtime))
    {
        g_free(fi->disp_mtime);
        fi->disp_mtime = NULL;
    }

    if(G_UNLIKELY(fi->target))
    {
        g_free(fi->target);
        fi->target = NULL;
    }

    if(G_LIKELY(fi->mime_type))
    {
        fm_mime_type_unref(fi->mime_type);
        fi->mime_type = NULL;
    }
    if(G_LIKELY(fi->icon))
    {
        fm_icon_unref(fi->icon);
        fi->icon = NULL;
    }
}

/**
 * fm_file_info_ref:
 * @fi:  A FmFileInfo struct
 *
 * Increase reference count of the FmFileInfo struct.
 *
 * Returns: the FmFileInfo struct itself
 */
FmFileInfo* fm_file_info_ref(FmFileInfo* fi)
{
    g_return_val_if_fail(fi != NULL, NULL);
    g_atomic_int_inc(&fi->n_ref);
    return fi;
}

/**
 * fm_file_info_unref:
 * @fi:  A FmFileInfo struct
 *
 * Decrease reference count of the FmFileInfo struct.
 * When the last reference to the struct is released,
 * the FmFileInfo struct is freed.
 */
void fm_file_info_unref(FmFileInfo* fi)
{
    g_return_if_fail(fi != NULL);
    /* g_debug("unref file info: %d", fi->n_ref); */
    if (g_atomic_int_dec_and_test(&fi->n_ref))
    {
        fm_file_info_clear(fi);
        g_slice_free(FmFileInfo, fi);
    }
}

/**
 * fm_file_info_update:
 * @fi:  A FmFileInfo struct
 * @src: another FmFileInfo struct
 * 
 * Update the content of @fi by copying file info
 * stored in @src to @fi.
 */
void fm_file_info_update(FmFileInfo* fi, FmFileInfo* src)
{
    FmPath* tmp_path = fm_path_ref(src->path);
    FmMimeType* tmp_type = fm_mime_type_ref(src->mime_type);
    FmIcon* tmp_icon = fm_icon_ref(src->icon);
    /* NOTE: we need to ref source first. Otherwise,
     * if path, mime_type, and icon are identical in src
     * and fi, calling fm_file_info_clear() first on fi
     * might unref that. */
    fm_file_info_clear(fi);
    fi->path = tmp_path;
    fi->mime_type = tmp_type;
    fi->icon = tmp_icon;

    fi->mode = src->mode;
    if(fm_path_is_native(fi->path))
        fi->dev = src->dev;
    else
        fi->fs_id = src->fs_id;
    fi->uid = src->uid;
    fi->gid = src->gid;
    fi->size = src->size;
    fi->mtime = src->mtime;
    fi->atime = src->atime;

    fi->blksize = src->blksize;
    fi->blocks = src->blocks;

    fi->disp_name = g_strdup(src->disp_name); /* disp_name might be NULL */

    if(src->collate_key == COLLATE_USING_DISPLAY_NAME)
        fi->collate_key = COLLATE_USING_DISPLAY_NAME;
    else
        fi->collate_key = g_strdup(src->collate_key);
    fi->disp_size = g_strdup(src->disp_size);
    fi->disp_mtime = g_strdup(src->disp_mtime);
}

/**
 * fm_file_info_get_icon:
 * @fi:  A FmFileInfo struct
 *
 * Get the icon used to show the file in the file manager.
 *
 * Returns: a FmIcon struct. The returned FmIcon struct is
 * owned by FmFileInfo and should not be freed.
 * If you need to keep it, use fm_icon_ref() to obtain a 
 * reference.
 */
FmIcon* fm_file_info_get_icon(FmFileInfo* fi)
{
    return fi->icon;
}

/**
 * fm_file_info_get_path:
 * @fi:  A FmFileInfo struct
 *
 * Get the path of the file
 * 
 * Returns: a FmPath struct. The returned FmPath struct is
 * owned by FmFileInfo and should not be freed.
 * If you need to keep it, use fm_path_ref() to obtain a 
 * reference.
 */
FmPath* fm_file_info_get_path(FmFileInfo* fi)
{
    return fi->path;
}

/**
 * fm_file_info_get_name:
 * @fi:  A FmFileInfo struct
 *
 * Get the base name of the file in filesystem encoding.
 *
 * Returns: a const string owned by FmFileInfo which should
 * not be freed.
 */
const char* fm_file_info_get_name(FmFileInfo* fi)
{
    return fi->path->name;
}

/**
 * fm_file_info_get_disp_name:
 * @fi:  A FmFileInfo struct
 *
 * Get the display name used to show the file in the file 
 * manager UI. The display name is guaranteed to be UTF-8
 * and may be different from the real file name on the 
 * filesystem.
 *
 * Returns: a const strin owned by FmFileInfo which should
 * not be freed.
 */
/* Get displayed name encoded in UTF-8 */
const char* fm_file_info_get_disp_name(FmFileInfo* fi)
{
    return G_LIKELY(!fi->disp_name) ? fi->path->name : fi->disp_name;
}

/**
 * fm_file_info_set_path:
 * @fi:  A FmFileInfo struct
 * @path: a FmPath struct
 *
 * Change the path of the FmFileInfo.
 */
void fm_file_info_set_path(FmFileInfo* fi, FmPath* path)
{
    if(fi->path)
        fm_path_unref(fi->path);

    if(path)
        fi->path = fm_path_ref(path);
    else
        fi->path = NULL;
}

/**
 * fm_file_info_set_disp_name:
 * @fi:  A FmFileInfo struct
 * @name: A UTF-8 display name. (can be NULL).
 *
 * Set the display name used to show the file in the 
 * file manager UI. If NULL is passed for @name,
 * the original display will be freed and the real base name
 * will be used for display.
 */
/* if disp name is set to NULL, we use the real filename for display. */
void fm_file_info_set_disp_name(FmFileInfo* fi, const char* name)
{
    g_free(fi->disp_name);
    fi->disp_name = g_strdup(name);
}

/**
 * fm_file_info_get_size:
 * @fi:  A FmFileInfo struct
 *
 * Returns: the size of the file in bytes.
 */
goffset fm_file_info_get_size(FmFileInfo* fi)
{
    return fi->size;
}

/**
 * fm_file_info_get_disp_size:
 * @fi:  A FmFileInfo struct
 *
 * Get the size of the file as a human-readable string.
 * It's convinient for show the file size to the user.
 *
 * Returns: a const string owned by FmFileInfo which should
 * not be freed. (non-NULL)
 */
const char* fm_file_info_get_disp_size(FmFileInfo* fi)
{
    if (G_UNLIKELY(!fi->disp_size))
    {
        if(S_ISREG(fi->mode))
        {
            char buf[ 64 ];
            fm_file_size_to_str(buf, sizeof(buf), fi->size, use_si_prefix);
            fi->disp_size = g_strdup(buf);
        }
    }
    return fi->disp_size;
}

/**
 * fm_file_info_get_blocks
 * @fi:  A FmFileInfo struct
 *
 * Returns: how many filesystem blocks used by the file.
 */
goffset fm_file_info_get_blocks(FmFileInfo* fi)
{
    return fi->blocks;
}

/**
 * fm_file_info_get_mime_type:
 * @fi:  A FmFileInfo struct
 *
 * Get the mime-type of the file.
 *
 * Returns: a FmMimeType struct owned by FmFileInfo which
 * should not be freed.
 * If you need to keep it, use fm_mime_type_ref() to obtain a 
 * reference.
 */
FmMimeType* fm_file_info_get_mime_type(FmFileInfo* fi)
{
    return fi->mime_type;
}

/**
 * fm_file_info_get_mode:
 * @fi:  A FmFileInfo struct
 *
 * Get the mode of the file. For detail about the meaning of
 * mode, see manpage of stat() and the st_mode struct field.
 *
 * Returns: mode_t value of the file as defined in POSIX struct stat.
 */
mode_t fm_file_info_get_mode(FmFileInfo* fi)
{
    return fi->mode;
}

/**
 * fm_file_info_get_is_native:
 * @fi:  A FmFileInfo struct
 *
 * Check if the file is a native UNIX file.
 * 
 * Returns: TRUE for native UNIX files, FALSE for
 * remote filesystems or other URIs, such as 
 * trahs:///, computer:///, ...etc.
 */
gboolean fm_file_info_is_native(FmFileInfo* fi)
{
	return fm_path_is_native(fi->path);
}

/**
 * fm_file_info_get_is_dir:
 * @fi:  A FmFileInfo struct
 *
 * Returns: TRUE if the file is a directory.
 */
gboolean fm_file_info_is_dir(FmFileInfo* fi)
{
    return (S_ISDIR(fi->mode) ||
        (S_ISLNK(fi->mode) && (0 == strcmp(fi->mime_type->type, "inode/directory"))));
}

/**
 * fm_file_info_get_is_symlink:
 * @fi:  A FmFileInfo struct
 *
 * Check if the file is a symlink. Note that for symlinks,
 * all infos stored in FmFileInfo are actually the info of
 * their targets.
 * The only two places you can tell that is a symlink are:
 * 1. fm_file_info_get_is_symlink()
 * 2. fm_file_info_get_target() which returns the target
 * of the symlink.
 * 
 * Returns: TRUE if the file is a symlink
 */
gboolean fm_file_info_is_symlink(FmFileInfo* fi)
{
    return S_ISLNK(fi->mode) ? TRUE : FALSE;
}

/**
 * fm_file_info_get_is_shortcut:
 * @fi:  A FmFileInfo struct
 *
 * Returns: TRUE if the file is a shortcut.
 * For a shortcut, read the value of fm_file_info_get_target()
 * to get the destination the shortut points to.
 * An example of shortcut type FmFileInfo is file info of
 * files in menu://applications/
 */
gboolean fm_file_info_is_shortcut(FmFileInfo* fi)
{
    return fi->mime_type == _fm_mime_type_get_inode_x_shortcut();
}

gboolean fm_file_info_is_mountable(FmFileInfo* fi)
{
    return fi->mime_type == _fm_mime_type_get_inode_x_mountable();
}

/**
 * fm_file_info_get_is_image:
 * @fi:  A FmFileInfo struct
 *
 * Returns: TRUE if the file is a image file (*.jpg, *.png, ...).
 */
gboolean fm_file_info_is_image(FmFileInfo* fi)
{
    /* FIXME: We had better use functions of xdg_mime to check this */
    if (!strncmp("image/", fi->mime_type->type, 6))
        return TRUE;
    return FALSE;
}

/**
 * fm_file_info_get_is_text:
 * @fi:  A FmFileInfo struct
 *
 * Returns: TRUE if the file is a plain text file.
 */
gboolean fm_file_info_is_text(FmFileInfo* fi)
{
    if(g_content_type_is_a(fi->mime_type->type, "text/plain"))
        return TRUE;
    return FALSE;
}

/**
 * fm_file_info_get_is_desktop_entry:
 * @fi:  A FmFileInfo struct
 *
 * Returns: TRUE if the file is a desktop entry file.
 */
gboolean fm_file_info_is_desktop_entry(FmFileInfo* fi)
{
    return fi->mime_type == desktop_entry_type;
}

/**
 * fm_file_info_get_is_unknown_type:
 * @fi:  A FmFileInfo struct
 *
 * Returns: TRUE if the mime type of the file cannot be
 * recognized.
 */
gboolean fm_file_info_is_unknown_type(FmFileInfo* fi)
{
    return g_content_type_is_unknown(fi->mime_type->type);
}

/**
 * fm_file_info_get_is_executable_type:
 * @fi:  A FmFileInfo struct
 *
 * Note that the function only check if the file seems
 * to be an executable file. It does not check if the
 * user really has the permission to execute the file or
 * if the executable bit of the file is set.
 * To check if a file is really executable by the current
 * user, you may need to call POSIX access() or euidaccess().
 * 
 * Returns: TRUE if the file is a kind of executable file,
 * such as shell script, python script, perl script, or 
 * binary executable file.
 */
/* full path of the file is required by this function */
gboolean fm_file_info_is_executable_type(FmFileInfo* fi)
{
    if(strncmp(fm_mime_type_get_type(fi->mime_type), "text/", 5) == 0)
    { /* g_content_type_can_be_executable reports text files as executables too */
        /* We don't execute remote files */
        if(fm_path_is_local(fi->path) && (fi->mode & (S_IXOTH|S_IXGRP|S_IXUSR)))
        { /* it has executable bits so lets check shell-bang */
            char *path = fm_path_to_str(fi->path);
            int fd = open(path, O_RDONLY);
            g_free(path);
            if(fd >= 0)
            {
                char buf[2];
                ssize_t rdlen = read(fd, &buf, 2);
                close(fd);
                if(rdlen == 2 && buf[0] == '#' && buf[1] == '!')
                    return TRUE;
            }
        }
        return FALSE;
    }
    return g_content_type_can_be_executable(fi->mime_type->type);
}

/**
 * fm_file_info_get_is_hidden:
 * @fi:  A FmFileInfo struct
 *
 * Files treated as hidden files are filenames with dot prefix
 * or ~ suffix.
 * 
 * Returns: TRUE if the file is a hidden file.
 */
gboolean fm_file_info_is_hidden(FmFileInfo* fi)
{
    const char* name = fi->path->name;
    /* files with . prefix or ~ suffix are regarded as hidden files.
     * dirs with . prefix are regarded as hidden dirs. */
    /* FIXME: bug #3416724: backup and hidden files should be distinguishable */
    return (name[0] == '.' ||
       (!fm_file_info_is_dir(fi) && g_str_has_suffix(name, "~")));
}

/**
 * fm_file_info_get_can_thumbnail:
 * @fi:  A FmFileInfo struct
 *
 * Returns: TRUE if the the file manager can try to 
 * generate a thumbnail for the file.
 */
gboolean fm_file_info_can_thumbnail(FmFileInfo* fi)
{
    /* We cannot use S_ISREG here as this exclude all symlinks */
    if( fi->size == 0 || /* don't generate thumbnails for empty files */
        !(fi->mode & S_IFREG) ||
        fm_file_info_is_desktop_entry(fi) ||
        fm_file_info_is_unknown_type(fi))
        return FALSE;
    return TRUE;
}


/**
 * fm_file_info_get_collate_key:
 * @fi:  A FmFileInfo struct
 *
 * Get the collate key used for locale-dependent
 * filename sorting. The keys of different files 
 * can be compared with strcmp() directly.
 * 
 * Returns: a const string owned by FmFileInfo which should
 * not be freed.
 */
const char* fm_file_info_get_collate_key(FmFileInfo* fi)
{
    /* create a collate key on demand, if we don't have one */
    if(G_UNLIKELY(!fi->collate_key))
    {
        const char* disp_name = fm_file_info_get_disp_name(fi);
        char* casefold = g_utf8_casefold(disp_name, -1);
        char* collate = g_utf8_collate_key_for_filename(casefold, -1);
        g_free(casefold);
        if(strcmp(collate, disp_name))
            fi->collate_key = collate;
        else
        {
            /* if the collate key is the same as the display name,
             * then there is no need to save it.
             * Just use the display name directly. */
            fi->collate_key = COLLATE_USING_DISPLAY_NAME;
            g_free(collate);
        }
    }

    /* if the collate key is the same as the display name, 
     * just return the display name instead. */
    if(fi->collate_key == COLLATE_USING_DISPLAY_NAME)
        return fm_file_info_get_disp_name(fi);

    return fi->collate_key;
}

/**
 * fm_file_info_get_target:
 * @fi:  A FmFileInfo struct
 *
 * Get the target of a symlink or a shortcut.
 * 
 * Returns: a const string owned by FmFileInfo which should
 * not be freed. NULL if the file is not a symlink or
 * shortcut.
 */
const char* fm_file_info_get_target(FmFileInfo* fi)
{
    return fi->target;
}

/**
 * fm_file_info_get_desc:
 * @fi:  A FmFileInfo struct
 * 
 * Get a human-readable description for the file.
 * 
 * Returns: a const string owned by FmFileInfo which should
 * not be freed.
 */
const char* fm_file_info_get_desc(FmFileInfo* fi)
{
    /* FIXME: how to handle descriptions for virtual files without mime-tyoes? */
    return fi->mime_type ? fm_mime_type_get_desc(fi->mime_type) : NULL;
}

/**
 * fm_file_info_get_disp_mtime:
 * @fi:  A FmFileInfo struct
 * 
 * Get a human-readable string for showing file modification
 * time in the UI.
 * 
 * Returns: a const string owned by FmFileInfo which should
 * not be freed.
 */
const char* fm_file_info_get_disp_mtime(FmFileInfo* fi)
{
    /* FIXME: This can cause problems if the file really has mtime=0. */
    /*        We'd better hide mtime for virtual files only. */
    if(fi->mtime > 0)
    {
        if (!fi->disp_mtime)
        {
            char buf[ 128 ];
            strftime(buf, sizeof(buf),
                      "%x %R",
                      localtime(&fi->mtime));
            fi->disp_mtime = g_strdup(buf);
        }
    }
    return fi->disp_mtime;
}

/**
 * fm_file_info_get_mtime:
 * @fi:  A FmFileInfo struct
 * 
 * Returns: file modification time.
 */
time_t fm_file_info_get_mtime(FmFileInfo* fi)
{
    return fi->mtime;
}

/**
 * fm_file_info_get_mtime:
 * @fi:  A FmFileInfo struct
 * 
 * Returns: file access time.
 */
time_t fm_file_info_get_atime(FmFileInfo* fi)
{
    return fi->atime;
}

/**
 * fm_file_info_get_uid:
 * @fi:  A FmFileInfo struct
 * 
 * Returns: user id (uid) of the file owner.
 */
uid_t fm_file_info_get_uid(FmFileInfo* fi)
{
    return fi->uid;
}

/**
 * fm_file_info_get_gid:
 * @fi:  A FmFileInfo struct
 * 
 * Returns: group id (gid) of the file owner.
 */
gid_t fm_file_info_get_gid(FmFileInfo* fi)
{
    return fi->gid;
}


/**
 * fm_file_info_get_fs_id:
 * @fi:  A FmFileInfo struct
 * 
 * Get the filesystem id string
 * This is only applicable when the file is on a remote
 * filesystem. e.g. fm_file_info_is_native() returns FALSE.
 * 
 * Returns: a const string owned by FmFileInfo which should
 * not be freed.
 */
const char* fm_file_info_get_fs_id(FmFileInfo* fi)
{
    return fi->fs_id;
}

/**
 * fm_file_info_get_dev:
 * @fi:  A FmFileInfo struct
 * 
 * Get the filesystem device id (POSIX dev_t)
 * This is only applicable when the file is native.
 * e.g. fm_file_info_is_native() returns TRUE.
 * 
 * Returns: device id (POSIX dev_t, st_dev member of 
 * struct stat).
 */
dev_t fm_file_info_get_dev(FmFileInfo* fi)
{
    return fi->dev;
}

static FmListFuncs fm_list_funcs =
{
    .item_ref = (gpointer (*)(gpointer))&fm_file_info_ref,
    .item_unref = (void (*)(gpointer))&fm_file_info_unref
};

FmFileInfoList* fm_file_info_list_new()
{
    return (FmFileInfoList*)fm_list_new(&fm_list_funcs);
}

#if 0
gboolean fm_list_is_file_info_list(FmList* list)
{
    return list->funcs == &fm_list_funcs;
}
#endif

/* return TRUE if all files in the list are of the same type */
gboolean fm_file_info_list_is_same_type(FmFileInfoList* list)
{
    /* FIXME: handle virtual files without mime-types */
    if(!fm_list_is_empty((FmList*)list))
    {
        GList* l = fm_list_peek_head_link((FmList*)list);
        FmFileInfo* fi = (FmFileInfo*)l->data;
        l = l->next;
        for(;l;l=l->next)
        {
            FmFileInfo* fi2 = (FmFileInfo*)l->data;
            if(fi->mime_type != fi2->mime_type)
                return FALSE;
        }
    }
    return TRUE;
}

/* return TRUE if all files in the list are on the same fs */
gboolean fm_file_info_list_is_same_fs(FmFileInfoList* list)
{
    if(!fm_list_is_empty((FmList*)list))
    {
        GList* l = fm_list_peek_head_link((FmList*)list);
        FmFileInfo* fi = (FmFileInfo*)l->data;
        l = l->next;
        for(;l;l=l->next)
        {
            FmFileInfo* fi2 = (FmFileInfo*)l->data;
            gboolean is_native = fm_path_is_native(fi->path);
            if(is_native != fm_path_is_native(fi2->path))
                return FALSE;
            if(is_native)
            {
                if(fi->dev != fi2->dev)
                    return FALSE;
            }
            else
            {
                if(fi->fs_id != fi2->fs_id)
                    return FALSE;
            }
        }
    }
    return TRUE;
}
