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

#include <stdio.h>
#include "fm-utils.h"

char* fm_file_size_to_str( char* buf, guint64 size, gboolean si_prefix )
{
    char * unit;
    /* guint point; */
    gfloat val;

    /*
       FIXME: Is floating point calculation slower than integer division?
              Some profiling is needed here.
    */
    if ( size > ( ( guint64 ) 1 ) << 30 )
    {
        if ( size > ( ( guint64 ) 1 ) << 40 )
        {
            /*
            size /= ( ( ( guint64 ) 1 << 40 ) / 10 );
            point = ( guint ) ( size % 10 );
            size /= 10;
            */
	        if(si_prefix)
	        {
	            val = ((gfloat)size) / (( guint64 ) 1000000000000 );
	            unit = "TB";
            }
            else
            {
                val = ((gfloat)size) / ( ( guint64 ) 1 << 40 );
                unit = "TiB";
            }
        }
        else
        {
            /*
            size /= ( ( 1 << 30 ) / 10 );
            point = ( guint ) ( size % 10 );
            size /= 10;
            */
	        if(si_prefix)
	        {
	            val = ((gfloat)size) / (( guint64 ) 1000000000 );
	            unit = "GB";
            }
            else
            {
                val = ((gfloat)size) / ( ( guint64 ) 1 << 30 );
                unit = "GiB";
            }
        }
    }
    else if ( size > ( 1 << 20 ) )
    {
        /*
        size /= ( ( 1 << 20 ) / 10 );
        point = ( guint ) ( size % 10 );
        size /= 10;
        */
	    if(si_prefix)
	    {
	        val = ((gfloat)size) / (( guint64 ) 1000000 );
	        unit = "MB";
        }
        else
        {
            val = ((gfloat)size) / ( ( guint64 ) 1 << 20 );
            unit = "MiB";
        }
    }
    else if ( size > ( 1 << 10 ) )
    {
        /*
        size /= ( ( 1 << 10 ) / 10 );
        point = size % 10;
        size /= 10;
        */
	    if(si_prefix)
	    {
	        val = ((gfloat)size) / (( guint64 ) 1000 );
	        unit = "KB";
	    }
	    else
	    {
	        val = ((gfloat)size) / ( ( guint64 ) 1 << 10 );
            unit = "KiB";
	    }
    }
    else
    {
        unit = size > 1 ? "Bytes" : "Byte";
        sprintf( buf, "%u %s", ( guint ) size, unit );
        return ;
    }
    /* sprintf( buf, "%llu.%u %s", size, point, unit ); */
    sprintf( buf, "%.1f %s", val, unit );
	return buf;
}
