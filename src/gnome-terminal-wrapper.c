/*
 *      gnome-terminal-wrapper.c
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
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

/* This is a wrapper program used to override gnome-terminal.
 * Unfortunately, glib/gio's GDesktopAppInfo always call gnome-terminal
 * when it needs a terminal emulator and this behavior is hard-coded.
 * So, the only way to solve this is to use a wrapper.
 * This is dirty, but there is no better way.
 * Complain to glib/gio/gtk+ developers if this make you uncomfortable. */

#include <stdio.h>
#include <glib.h>
#include <string.h>

#define CFG_FILENAME    "libfm/libfm.conf"

char* get_terminal(const char* conf_file)
{
    FILE* f = fopen(conf_file, "r");
    if(f)
    {
        char buf[1024];
        while(fgets(buf, 1024, f))
        {
            char* pbuf = strtok(buf, " \t=");
            if(pbuf && strcmp(pbuf, "terminal") == 0)
            {
                char* pval = strtok(NULL, "\n");
                if(pval)
                    return g_strdup(pval);
            }
        }
        fclose(f);
    }
    return NULL;
}

int main(int argc, char** argv)
{
    char* fpath;
    char* terminal;
    char* path, *sep;
    char** new_argv;
    /* FIXME: it's possible for libfm using programs to use a differnt path for libfm.conf. */
    fpath = g_build_filename(g_get_user_config_dir(), CFG_FILENAME, NULL);
    terminal = get_terminal(fpath);
    g_free(fpath);
    if(!terminal)
    {
        char** dirs, **dir;
        dirs = g_get_system_data_dirs();
        for(dir = dirs; *dir; ++dir)
        {
            fpath = g_build_filename(*dir, CFG_FILENAME, NULL);
            terminal = get_terminal(fpath);
            g_free(fpath);
            if(terminal)
                break;
        }
    }
    /* Remove /usr/lib/libfm from PATH */
    path = g_getenv("PATH");
    sep = strchr(path, ':');
    path = sep + 1;
    g_setenv("PATH", path, TRUE);

    if(argc < 2) /* only execute the temrinal emulator */
    {
        sep = strchr(terminal, ' ');
        if(sep)
            *sep = '\0';
        argv[0] = terminal;
    }

    if( strcmp(argv[1], "-x") == 0 ) /* gnome-terminal -x */
    {
        /* this is mostly called from glib/gio */
        int term_argc;
        char** term_argv;
        if(g_shell_parse_argv(terminal, &term_argc, &term_argv, NULL))
        {
            int i, j, k = 0;
            char** new_argv = g_new0(char*, argc + term_argc);

            for(i = 0; i <term_argc; ++i, ++k)
            {
                if(strcmp(term_argv[i], "%s") == 0)
                {
                    ++i;
                    break;
                }
                new_argv[k] = term_argv[i];
            }

            /* skip gnome-terminal (argv[0]) and -x (argv[1]) */
            for(j = 2; j < argc; ++j, ++k)
                new_argv[k] = argv[j];

            for(; i <term_argc; ++i, ++k)
                new_argv[k] = term_argv[i];

            argv = new_argv;
        }
    }
    else /* really call gnome-terminal? */
    {
        /* don't tough argv, just remove our dir from PATH.
         * So the original gnome-terminal on system will be called. */
    }
    return !g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}
