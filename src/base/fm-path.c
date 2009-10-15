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
#include <limits.h>

static FmPath* root = NULL;

static char* home_dir = NULL;
static int home_len = 0;
static FmPath* home = NULL;

static FmPath* desktop = NULL;
static char* desktop_dir = NULL;
static int desktop_len = 0;

static FmPath* trash_root = NULL;
static FmPath* network_root = NULL;

static FmPath* apps_root = NULL;

FmPath* fm_path_new(const char* path)
{
	const char* sep;
    /* FIXME: need to canonicalize paths */

    if( path[0] == '/' ) /* if this is a absolute native path */
    {
        if (path[1])
            return fm_path_new_relative(root, path + 1);
        else
            /* special case: handle root dir */
            return fm_path_ref( root );
    }
    else if ( path[0] == '~' && (path[1] == '\0' || path[1]=='/') ) /* home dir */
    {
        ++path;
        return *path ? fm_path_new_relative(home, path) : fm_path_ref(home);
    }
	else /* then this should be a URL */
	{
        FmPath* parent, *ret;
        char* colon = strchr(path, ':');
        char* hier_part;
        char* rest;
        int root_len;

        if( !colon ) /* this shouldn't happen */
            return NULL; /* invalid path FIXME: should we treat it as relative path? */

        /* FIXME: convert file:/// to local native path */
        hier_part = colon+1;
        if( hier_part[0] == '/' )
        {
            if(hier_part[1] == '/') /* this is a scheme:// form URI */
                rest = hier_part + 2;
            else /* a malformed URI */
                rest = hier_part + 1;

            if(*rest == '/') /* :/// means there is no authoraty part */
                ++rest;
            else /* we are now at autority part, something like <username>@domain/ */
            {
                while( *rest && *rest != '/' )
                    ++rest;
                if(*rest == '/')
                    ++rest;
            }

            if( strncmp(path, "trash:", 6) == 0 ) /* in trash:// */
            {
                if(*rest)
                    return fm_path_new_relative(trash_root, rest);
                else
                    return fm_path_ref(trash_root);
            }
            /* other URIs */
        }
        else /* this URI doesn't have //, like mailto: */
        {
            /* FIXME: is this useful to file managers? */
            rest = colon + 1;
        }
        root_len = (rest - path);
        parent = fm_path_new_child_len(NULL, path, root_len);
        /* FIXME: computer: is not remote, but a virtual path */
        parent->flags |= FM_PATH_IS_REMOTE;
        if(*rest)
        {
            ret = fm_path_new_relative(parent, rest);
            fm_path_unref(parent);
        }
        else
            ret = parent;
        return ret;
	}
	return fm_path_new_relative(NULL, path);
}

FmPath* fm_path_new_child_len(FmPath* parent, const char* basename, int name_len)
{
	FmPath* path;
    if(parent) /* remove tailing slash if needed. */
    {
        while(basename[name_len-1] == '/')
            --name_len;
    }
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
            /* FIXME: do we have more efficient way here? */
			/* FIXME: trash:///, computer:///, network:/// are virtual paths */
            /* FIXME: add // if only trash:/ is supplied in basename */
            if(strncmp(basename, "trash:", 6))
            {
    			path->flags |= FM_PATH_IS_TRASH|FM_PATH_IS_VIRTUAL;
            }
            else if(strncmp(basename, "computer:", 9))
            {
    			path->flags |= FM_PATH_IS_VIRTUAL;
            }
            else if(strncmp(basename, "network:", 8))
            {
    			path->flags |= FM_PATH_IS_VIRTUAL;
            }
            else if(strncmp(basename, "applications:", 13))
            {
    			path->flags |= FM_PATH_IS_VIRTUAL;
            }
            else
            {
                /* FIXME: http://, ftp://, smb://, ...etc. are remote paths */
                path->flags |= FM_PATH_IS_REMOTE;
            }
		}
		path->parent = NULL;
	}
	memcpy(path->name, basename, name_len);
	path->name[name_len] = '\0';
	return path;
}

FmPath* fm_path_new_child(FmPath* parent, const char* basename)
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

FmPath* fm_path_new_relative(FmPath* parent, const char* relative_path)
{
	FmPath* path;
	const char* sep;
	gsize name_len;

    /* FIXME: need to canonicalize paths */
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
        char* end = sep;
        while(*end && *end == '/') /* prevent tailing slash or duplicated slashes. */
            ++end;
        name_len = (sep - relative_path);
        parent = fm_path_new_child_len(parent, relative_path, name_len);
        if(*end != '\0')
        {
            relative_path = end;
            path = fm_path_new_relative(parent, relative_path);
            fm_path_unref(parent);
        }
        else /* this is tailing slash */
            path = parent;
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
		if(G_LIKELY(path->parent))
			fm_path_unref(path->parent);
		g_free(path);
	}
}

/* FIXME: should we use _dup_ instead of _get_ here? */
FmPath* fm_path_get_parent(FmPath* path)
{
	return path->parent ? fm_path_ref(path->parent) : NULL;
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
	if (path->parent->parent)
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

FmPath* fm_path_get_applications()
{
	return apps_root;
}

gboolean fm_path_is_native(FmPath* path)
{
	return (path->flags&FM_PATH_IS_NATIVE) ? TRUE : FALSE;
}

gboolean fm_path_is_trash(FmPath* path)
{
	return (path->flags&FM_PATH_IS_TRASH) ? TRUE : FALSE;
}

gboolean fm_path_is_trash_root(FmPath* path)
{
    return path == trash_root;
}

gboolean fm_path_is_virtual(FmPath* path)
{
	return (path->flags&FM_PATH_IS_VIRTUAL) ? TRUE : FALSE;
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
	trash_root = fm_path_new_child(NULL, "trash:///");
	trash_root->flags |= (FM_PATH_IS_TRASH|FM_PATH_IS_VIRTUAL);

    apps_root = fm_path_new_child(NULL, "applications:///");
	apps_root->flags |= (FM_PATH_IS_VIRTUAL);
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
    if( !p1 || !p2 )
        return FALSE;
    if( strcmp(p1->name, p2->name) != 0 )
        return FALSE;
    return fm_path_equal( p1->parent, p2->parent);
}

/* Check if this path contains absolute pathname str*/
gboolean fm_path_equal_str(FmPath *path, const gchar *str, int n) 
{
    const gchar *last_part;
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
