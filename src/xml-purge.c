/*
 *      xml-purge.c
 *
 *      Copyright 2008 - 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#define IS_BLANK(ch)    strchr(" \t\n\r", ch)

static void purge_file( const char* file )
{
    char* buf, *pbuf;
    int in_tag = 0, in_quote = 0;
    FILE* fo;

    if(!g_file_get_contents(file, &buf, NULL, NULL))
        exit(1);

    fo = fopen( file, "w" );
    if( ! fo )
        goto error;

    for( pbuf = buf; *pbuf; ++pbuf )
    {
        if( in_tag > 0 )
        {
            if( in_quote )
            {
                if( *pbuf == '\"' )
                    in_quote = 0;
            }
            else
            {
                if( *pbuf == '\"' )
                    ++in_quote;
                if( ! in_quote && IS_BLANK(*pbuf) ) /* skip unnecessary blanks */
                {
                    do{
                        ++pbuf;
                    }while( IS_BLANK( *pbuf ) );

                    if( *pbuf != '>' )
                        fputc( ' ', fo );
                    --pbuf;
                    continue;
                }
            }
            if( *pbuf == '>' )
                --in_tag;
            fputc( *pbuf, fo );
        }
        else
        {
            if( *pbuf == '<' )
            {
                if( 0 == strncmp( pbuf, "<!--", 4 ) )   /* skip comments */
                {
                    pbuf = strstr( pbuf, "-->" );
                    if( ! pbuf )
                        goto error;
                    pbuf += 2;
                    continue;
                }
                ++in_tag;
                fputc( '<', fo );
            }
            else
            {
                char* tmp = pbuf;
                while( *tmp && IS_BLANK( *tmp ) && *tmp != '<' )
                    ++tmp;
                if( *tmp == '<' )   /* all cdata are blank characters */
                    pbuf = tmp - 1;
                else /* not blank, keep the cdata */
                {
                    if( tmp == pbuf )
                        fputc( *pbuf, fo );
                    else
                    {
                        fwrite( pbuf, 1, tmp - pbuf, fo );
                        pbuf = tmp - 1;
                    }
                }
            }
        }
    }

    fclose( fo );

error:
    free( buf );
}

int main( int argc, char** argv )
{
    int i;
    if( argc < 2 )
        return 1;

    for( i = 1; i < argc; ++i )
        purge_file( argv[ i] );

    return 0;
}
