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
#include "fm-utils.h"

char* fm_file_size_to_str( char* buf, goffset size, gboolean si_prefix )
{
    const char * unit;
    gfloat val;

	if( si_prefix ) /* 1024 based */
	{
		if(size < 1024)
		{
			sprintf( buf, ngettext("%u byte", "%u bytes", (guint)size), (guint)size);
			return buf;
		}
		else if(size < ((goffset)1<<20))
		{
			val = ((gfloat)size)/((goffset)1<<10);
			unit = _("KiB");
		}
		else if(size < ((goffset)1)<<30)
		{
			val = ((gfloat)size)/((goffset)1<<20);
			unit = _("MiB");
		}
		else if(size < ((goffset)1)<<40)
		{
			val = ((gfloat)size)/((goffset)1<<30);
			unit = _("GiB");
		}
		else if(size < ((goffset)1)<<50)
		{
			val = ((gfloat)size)/((goffset)1<<40);
			unit = _("TiB");
		}
	}
	else /* 1000 based */
	{
		if(size < 1000)
		{
			sprintf( buf, ngettext("%u byte", "%u bytes", (guint)size), (guint)size);
			return buf;
		}
		else if(size < (goffset)1000000)
		{
			val = ((gfloat)size)/((goffset)1000 );
			unit = _("KB");
		}
		else if(size < (goffset)1000000000)
		{
			val = ((gfloat)size)/((goffset)1000000 );
			unit = _("MB");
		}
		else if(size < (goffset)(gfloat)1000000000000)
		{
			val = ((gfloat)size)/((goffset)(gfloat)1000000000 );
			unit = _("GB");
		}
		else if(size < (goffset)(gfloat)1000000000000000)
		{
			val = ((gfloat)size)/((goffset)(gfloat)1000000000000 );
			unit = _("TB");
		}
	}
    sprintf( buf, "%.1f %s", val, unit );
	return buf;
}
