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

#include "fm-path.h"
#include "fm-file-info.h"
#include <string.h>

static FmPath* root = NULL;

static char* home_dir = NULL;
static int home_len = 0;
static FmPath* home = NULL;

static FmPath* desktop = NULL;
static char* desktop_dir = NULL;
static int desktop_len = 0;

static FmPath* trash_root = NULL;
static FmPath* network_root = NULL;

FmPath*	fm_path_new(const char* path)
{
	const char* sep;
	if( path[0] == '/' ) /* is this is a absolute native path */
		return fm_path_new_relative(root, path + 1);
	else
	{
		if( 0 == strncmp(path, "trash:", 6) ) /* in trash */
		{
			path += 6;
			while(*path == '/')
				++path;
			return fm_path_new_relative(trash_root, path);
		}
		else if( sep = strstr(path, ":/") ) /* if it's a URL */
		{
			FmPath* parent = fm_path_new_child_len(NULL, path, (sep - path)+1);
			FmPath* ret;
			/* FIXME: computer: is not remote, but a virtual path */
			parent->flags |= FM_PATH_IS_REMOTE;
			path = sep += 2;
			while(*path == '/')
				++path;			
			ret = fm_path_new_relative(parent, path);
			fm_path_unref(parent);
			return ret;
		}
	}
	return fm_path_new_relative(NULL, path);
}

FmPath*	fm_path_new_child_len(FmPath* parent, const char* basename, int name_len)
{
	FmPath* path;
	path = (FmPath*)g_malloc(sizeof(FmPath) + name_len);
	path->n_ref = 1;
	if(G_LIKELY(parent))
	{
		path->flags = parent->flags;
		path->parent = fm_path_ref(parent);
	}
	else
	{
    	path->flags = 0;
		if(*basename == '/') /* it's a native full path */
			path->flags |= FM_PATH_IS_NATIVE;
		else
		{
			/* FIXME: trash:/, computer:/, network:/ are virtual paths */
			path->flags |= FM_PATH_IS_VIRTUAL;

			/* FIXME: http://, ftp://, smb://, ...etc. are remote paths */
			path->flags |= FM_PATH_IS_REMOTE;
		}
		path->parent = NULL;
	}
	memcpy(path->name, basename, name_len);
	path->name[name_len] = '\0';
	return path;
}

FmPath*	fm_path_new_child(FmPath* parent, const char* basename)
{
	int baselen = strlen(basename);
	return fm_path_new_child_len(parent, basename, baselen);
}

/*
FmPath*	fm_path_new_relative_len(FmPath* parent, const char* relative_path, int len)
{
	FmPath* path;

	return path;
}
*/

FmPath*	fm_path_new_relative(FmPath* parent, const char* relative_path)
{
	FmPath* path;
	const char* sep;
	gsize name_len;

	if(parent == root)
	{
		if( 0 == strncmp(relative_path, home_dir + 1, home_len - 1) ) /* in home dir */
		{
			if( relative_path[home_len - 1] == '\0' ) /* this is the home dir */
				if(G_LIKELY(home))
					return fm_path_ref(home);
				else
					goto _out;
			if( 0 == strncmp(relative_path, desktop_dir + home_len + 1, desktop_len - home_len -1) ) /* in desktop dir */
			{
				if(relative_path[desktop_len - 1] == '\0') /* this is the desktop dir */
					return fm_path_ref(desktop);
				return fm_path_new_relative(desktop, relative_path + desktop_len + 1);
			}
		}
	}
_out:
	sep = strchr(relative_path, '/');
	if(sep)
	{
		name_len = (sep - relative_path);
		parent = fm_path_new_child_len(parent, relative_path, name_len);
		relative_path = sep + 1;
		path = fm_path_new_relative(parent, relative_path);
		fm_path_unref(parent);
	}
	else
	{
		name_len = strlen(relative_path);
		path = fm_path_new_child_len(parent, relative_path, name_len);
	}
	return path;
}

FmPath* fm_path_new_for_gfile(GFile* gf)
{
	FmPath* path;
	char* str;
	if( g_file_is_native(gf) )
		str = g_file_get_path(gf);
	else
		str = g_file_get_uri(gf);
	path = fm_path_new(str);
	g_free(str);
	return path;
}

FmPath*	fm_path_ref(FmPath* path)
{
	g_atomic_int_inc(&path->n_ref);
	return path;
}

void fm_path_unref(FmPath* path)
{
	/* g_debug("fm_path_unref: %s, n_ref = %d", fm_path_to_str(path), path->n_ref); */
	if(g_atomic_int_dec_and_test(&path->n_ref))
	{
		/* g_debug("free path: %s", path->name); */
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

static int fm_path_strlen(FmPath* path)
{
	int len = 0;
	for(;;)
	{
		len += strlen(path->name);
		if(G_UNLIKELY(!path->parent ))
			break;
		if(*path->parent->name != '/')
			++len; /* add a character for separator */
		path = path->parent;
	}
	return len;
}

/* FIXME: handle display name and real file name (maybe non-UTF8) issue */
char* fm_path_to_str(FmPath* path)
{
	int len = fm_path_strlen(path);
	char* buf = g_new0(char, len+1), *pbuf = buf + len;
	FmPath* p = path;
	buf[len] = '\0';
	for(;;)
	{
		int name_len = strlen(p->name);
		pbuf -= name_len;
		memcpy(pbuf, p->name, name_len);
		if( p->parent )
		{
			if(p->parent->name[0] != '/')
			{
				--pbuf;
				*pbuf = '/';
			}
			p = p->parent;
		}
		else
			break;
	}
	return buf;
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
	return root;
}

FmPath* fm_path_get_home()
{
	return home;
}

FmPath* fm_path_get_desktop()
{
	return desktop;
}

FmPath* fm_path_get_trash()
{
	return trash_root;
}

gboolean fm_path_is_native(FmPath* path)
{
	return (path->flags&FM_PATH_IS_NATIVE) ? TRUE : FALSE;
}

void fm_path_init()
{
	const char* sep, *name;
	FmPath* tmp, *parent;

	/* path object of root dir */
	root = fm_path_new_child(NULL, "/");
	home_dir = g_get_home_dir();
	home_len = strlen(home_dir);

	/* build path object for home dir */
	name = home_dir + 1; /* skip leading / */
	parent = root;
	while( sep = strchr(name, '/') )
	{
		int len = (sep - name);
		/* ref counting is not a problem here since this path component
		 * will exist till the termination of the program. So mem leak is ok. */
		tmp = fm_path_new_child_len(parent, name, len);
		name = sep + 1;
		parent = tmp;
	}
	home = fm_path_new_child(parent, name);

	desktop_dir = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
	desktop_len = strlen(desktop_dir);

	/* build path object for desktop dir */
	name = desktop_dir + home_len + 1; /* skip home dir part / */
	parent = home;
	while( sep = strchr(name, '/') )
	{
		int len = (sep - name);
		/* ref counting is not a problem here since this path component
		 * will exist till the termination of the program. So mem leak is ok. */
		tmp = fm_path_new_child_len(parent, name, len);
		name = sep + 1;
		parent = tmp;
	}
	desktop = fm_path_new_child(parent, name);

	/* build path object for trash can */
    /* FIXME: currently there are problems with URIs. using trash:/ here will cause problems. */
	trash_root = fm_path_new_child(NULL, "trash:");
	trash_root->flags |= (FM_PATH_IS_TRASH|FM_PATH_IS_VIRTUAL);
}


/* For used in hash tables */

/* FIXME: is this good enough? */
guint fm_path_hash(FmPath* path)
{
    guint hash = g_str_hash(path->name);
    if(path->parent)
        hash ^= fm_path_hash(path->parent);
    return hash;
}

gboolean fm_path_equal(FmPath* p1, FmPath* p2)
{
    if(p1 == p2)
        return TRUE;
    if( !p1 || !p2 )
        return FALSE;
    if( strcmp(p1->name, p2->name) && fm_path_equal(p1->parent, p2->parent) )
        return TRUE;
    return FALSE;
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

FmPathList* fm_path_list_new_from_uris(const char** uris)
{
    const char** uri;
	FmPathList* pl = fm_path_list_new();
	for(uri = uris; *uri; ++uri)
	{
		FmPath* path;
		char* unescaped;
        if(g_str_has_prefix(*uri, "file:"))
            unescaped = g_filename_from_uri(*uri, NULL, NULL);
        else
            unescaped = g_uri_unescape_string(*uri, NULL);

        path = fm_path_new(unescaped);
		g_free(unescaped);
		fm_list_push_tail_noref(pl, path);
	}
	return pl;
}

FmPathList* fm_path_list_new_from_uri_list(const char* uri_list)
{
    char** uris = g_strsplit(uri_list, "\r\n", -1);
	FmPathList* pl = fm_path_list_new_from_uris(uris);
    g_strfreev(uris);
	return pl;
}

char* fm_path_list_to_uri_list(FmPathList* pl)
{
	GString* buf = g_string_sized_new(4096);
	fm_path_list_write_uri_list(pl, buf);
	return g_string_free(buf, FALSE);
}

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
