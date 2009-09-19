/*
 *      fm-utils.c
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

#include <glib/gi18n.h>
#include <libintl.h>

#include <stdio.h>
#include <stdlib.h>
#include "fm-utils.h"

#define BI_KiB	((gdouble)1024.0)
#define BI_MiB	((gdouble)1024.0 * 1024.0)
#define BI_GiB	((gdouble)1024.0 * 1024.0 * 1024.0)
#define BI_TiB	((gdouble)1024.0 * 1024.0 * 1024.0 * 1024.0)

#define SI_KB	((gdouble)1000.0)
#define SI_MB	((gdouble)1000.0 * 1000.0)
#define SI_GB	((gdouble)1000.0 * 1000.0 * 1000.0)
#define SI_TB	((gdouble)1000.0 * 1000.0 * 1000.0 * 1000.0)

char* fm_file_size_to_str( char* buf, goffset size, gboolean si_prefix )
{
    const char * unit;
    gdouble val;

	if( si_prefix ) /* 1000 based SI units */
	{
		if(size < (goffset)SI_KB)
		{
			sprintf( buf, ngettext("%u byte", "%u bytes", (guint)size), (guint)size);
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
			sprintf( buf, ngettext("%u byte", "%u bytes", (guint)size), (guint)size);
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
    sprintf( buf, "%.1f %s", val, unit );
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
    gboolean ret;
    char* str = g_key_file_get_value(kf, grp, key, NULL);
    if(G_LIKELY(str))
    {
        *val = (str[0] == '1' || str[0] == 't');
        g_free(str);
    }
    return str != NULL;
}
