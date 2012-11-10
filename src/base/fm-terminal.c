/*
 *      fm-terminal.c
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

/**
 * SECTION:fm-terminal
 * @short_description: Terminals representation for libfm.
 * @title: FmTerminal
 *
 * @include: libfm/fm-terminal.h
 *
 * The FmTerminal object represents description how applications which
 * require start in terminal should be started.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <string.h>

#include "fm-terminal.h"
#include "fm-config.h"

#define FM_TERMINAL_TYPE               (fm_terminal_get_type())
#define FM_IS_TERMINAL(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj), FM_TERMINAL_TYPE))

typedef struct _FmTerminalClass         FmTerminalClass;

struct _FmTerminalClass
{
    GObjectClass parent;
};

static GType fm_terminal_get_type(void);
static void fm_terminal_finalize(GObject *object);

G_DEFINE_TYPE(FmTerminal, fm_terminal, G_TYPE_OBJECT);

static void fm_terminal_class_init(FmTerminalClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_terminal_finalize;
}

static void fm_terminal_finalize(GObject *object)
{
    FmTerminal* self;
    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_TERMINAL(object));

    self = (FmTerminal*)object;
    g_free(self->program);
    g_free(self->open_arg);
    g_free(self->noclose_arg);
    g_free(self->desktop_id);
    g_free(self->custom_args);

    G_OBJECT_CLASS(fm_terminal_parent_class)->finalize(object);
}

static void fm_terminal_init(FmTerminal *self)
{
}

static FmTerminal* fm_terminal_new(void)
{
    return (FmTerminal*)g_object_new(FM_TERMINAL_TYPE, NULL);
}


static GSList *terminals = NULL;
static FmTerminal *default_terminal = NULL;

static void on_terminal_changed(FmConfig *cfg, gpointer unused)
{
    FmTerminal *term;
    gsize n;
    GSList *l;
    gchar *name, *basename;

    if(default_terminal)
        g_object_unref(default_terminal);
    default_terminal = NULL;
    if(cfg->terminal == NULL)
        return;

    for(n = 0; cfg->terminal[n] && cfg->terminal[n] != ' '; n++);
    name = g_strndup(cfg->terminal, n);
    basename = strrchr(name, '/');
    if(basename)
        basename++;
    else
        basename = name;
    /* g_debug("terminal in FmConfig: %s, args=%s", name, &cfg->terminal[n]); */
    for(l = terminals; l; l = l->next)
        if(strcmp(basename, ((FmTerminal*)l->data)->program) == 0)
            break;
    if(l)
    {
        if(name[0] != '/') /* not full path; call by basename */
        {
            g_free(name);
            term = g_object_ref(l->data);
        }
        else /* call by full path: add new description and fill from database */
        {
            term = fm_terminal_new();
            term->program = name;
            term->open_arg = g_strdup(((FmTerminal*)l->data)->open_arg);
            term->noclose_arg = g_strdup(((FmTerminal*)l->data)->noclose_arg);
            term->desktop_id = g_strdup(((FmTerminal*)l->data)->desktop_id);
        }
    }
    else /* unknown terminal */
    {
        term = fm_terminal_new();
        term->program = name;
        term->open_arg = g_strdup("-e"); /* assume it is default */
    }
    default_terminal = term;
    g_free(term->custom_args);
    term->custom_args = NULL;
    if(cfg->terminal[n] == ' ' && cfg->terminal[n+1])
    {
        term->custom_args = g_strdup(&cfg->terminal[n+1]);
        /* support for old style terminal line alike 'xterm -e %s' */
        name = strchr(term->custom_args, '%');
        if(name)
        {
            /* skip end spaces */
            while(name > term->custom_args && name[-1] == ' ')
                name--;
            /* drop '-e' or '-x' */
            if(name > term->custom_args + 1 && name[-2] == '-')
            {
                name -= 2;
                /* skip end spaces */
                while(name > term->custom_args && name[-1] == ' ')
                    name--;
            }
            if(name > term->custom_args)
                *name = '\0'; /* cut the line */
            else
            {
                g_free(term->custom_args);
                term->custom_args = NULL;
            }
        }
    }
}

/* init terminal list from config */
void _fm_terminal_init(void)
{
    GKeyFile *kf;
    gsize i, n;
    gchar **programs;
    FmTerminal *term;

    /* read system terminals file */
    kf = g_key_file_new();
    if(g_key_file_load_from_file(kf, PACKAGE_DATA_DIR "/terminals.list", 0, NULL))
    {
        programs = g_key_file_get_groups(kf, &n);
        if(programs)
        {
            for(i = 0; i < n; ++i)
            {
                /* g_debug("found terminal configuration: %s", programs[i]); */
                term = fm_terminal_new();
                term->program = programs[i];
                term->open_arg = g_key_file_get_string(kf, programs[i],
                                                       "open_arg", NULL);
                term->noclose_arg = g_key_file_get_string(kf, programs[i],
                                                          "noclose_arg", NULL);
                term->desktop_id = g_key_file_get_string(kf, programs[i],
                                                         "desktop_id", NULL);
                terminals = g_slist_append(terminals, term);
            }
            g_free(programs); /* strings in the vector are stolen by objects */
        }
    }
    g_key_file_free(kf);
    /* TODO: read user terminals file? */
    /* read from config */
    on_terminal_changed(fm_config, NULL);
    /* monitor the config */
    g_signal_connect(fm_config, "changed::terminal",
                     G_CALLBACK(on_terminal_changed), NULL);
}

/* free all resources */
void _fm_terminal_finalize(void)
{
    /* cancel monitor of config */
    g_signal_handlers_disconnect_by_func(fm_config, on_terminal_changed, NULL);
    /* free the data */
    g_slist_foreach(terminals, (GFunc)g_object_unref, NULL);
    g_slist_free(terminals);
    terminals = NULL;
    if(default_terminal)
        g_object_unref(default_terminal);
    default_terminal = NULL;
}

/**
 * fm_terminal_get_default
 *
 * Retrieves description of terminal which is defined in libfm config.
 * Returned data should be freed with g_object_unref() after usage.
 *
 * This API is not thread-safe.
 *
 * Returns: (transfer full): terminal descriptor or %NULL if no terminal is set.
 *
 * Since: 1.2.0
 */
FmTerminal* fm_terminal_get_default(void)
{
    return default_terminal ? g_object_ref(default_terminal) : NULL;
}
