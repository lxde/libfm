//      fm-app-info.c
//
//      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

/**
 * SECTION:fm-app-info
 * @short_description: FM application launch handlers
 * @title: GAppInfo extensions
 *
 * @include: libfm/fm-app-info.h
 *
 */

#include "fm-app-info.h"
#include "fm-config.h"
#include "fm-utils.h"
#include "fm-file.h"

#include <string.h>
#include <gio/gdesktopappinfo.h>

static void append_file_to_cmd(GFile* gf, GString* cmd)
{
    char* file = g_file_get_path(gf);
    char* quote;
    if(file == NULL) /* trash:// gvfs is incomplete in resolving it */
    {
        /* we can retrieve real path from GVFS >= 1.13.3 */
        if(g_file_has_uri_scheme(gf, "trash"))
        {
            GFileInfo *inf;
            const char *orig_path;

            inf = g_file_query_info(gf, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI,
                                    G_FILE_QUERY_INFO_NONE, NULL, NULL);
            if(inf == NULL) /* failed */
                return;
            orig_path = g_file_info_get_attribute_string(inf, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
            if(orig_path != NULL) /* success */
                file = g_filename_from_uri(orig_path, NULL, NULL);
            g_object_unref(inf);
        }
        if(file == NULL)
            return;
    }
    quote = g_shell_quote(file);
    g_string_append(cmd, quote);
    g_string_append_c(cmd, ' ');
    g_free(quote);
    g_free(file);
}

static void append_uri_to_cmd(GFile* gf, GString* cmd)
{
    char* uri = g_file_get_uri(gf);
    char* quote = g_shell_quote(uri);
    g_string_append(cmd, quote);
    g_string_append_c(cmd, ' ');
    g_free(quote);
    g_free(uri);
}

static char* expand_exec_macros(GAppInfo* app, const char* full_desktop_path, GKeyFile* kf, GList* gfiles)
{
    GString* cmd;
    const char* exec = g_app_info_get_commandline(app);
    const char* p;
    gboolean files_added = FALSE;

    cmd = g_string_sized_new(1024);
    for(p = exec; *p; ++p)
    {
        if(*p == '%')
        {
            ++p;
            if(!*p)
                break;
            switch(*p)
            {
            case 'f':
                if(gfiles)
                    append_file_to_cmd(G_FILE(gfiles->data), cmd);
                files_added = TRUE;
                break;
            case 'F':
                g_list_foreach(gfiles, (GFunc)append_file_to_cmd, cmd);
                files_added = TRUE;
                break;
            case 'u':
                if(gfiles)
                    append_uri_to_cmd(G_FILE(gfiles->data), cmd);
                files_added = TRUE;
                break;
            case 'U':
                g_list_foreach(gfiles, (GFunc)append_uri_to_cmd, cmd);
                files_added = TRUE;
                break;
            case '%':
                g_string_append_c(cmd, '%');
                break;
            case 'i':
                {
                    char* icon_name = g_key_file_get_locale_string(kf, "Desktop Entry",
                                                                   "Icon", NULL, NULL);
                    if(icon_name)
                    {
                        g_string_append(cmd, "--icon ");
                        g_string_append(cmd, icon_name);
                        g_free(icon_name);
                    }
                    break;
                }
            case 'c':
                {
                    const char* name = g_app_info_get_name(app);
                    if(name)
                        g_string_append(cmd, name);
                    break;
                }
            case 'k':
                /* append the file path of the desktop file */
                if(full_desktop_path)
                {
                    /* FIXME: how about quoting here? */
                    char* desktop_location = g_path_get_dirname(full_desktop_path);
                    g_string_append(cmd, desktop_location);
                    g_free(desktop_location);
                }
                break;
            }
        }
        else
            g_string_append_c(cmd, *p);
    }

    /* if files are provided but the Exec key doesn't contain %f, %F, %u, or %U */
    if(gfiles && !files_added)
    {
        /* treat as %f */
        g_string_append_c(cmd, ' ');
        append_file_to_cmd(G_FILE(gfiles->data), cmd);
    }

    return g_string_free(cmd, FALSE);
}

struct ChildSetup
{
    char* display;
    char* sn_id;
};

static void child_setup(gpointer user_data)
{
    struct ChildSetup* data = (struct ChildSetup*)user_data;
    if(data->display)
        g_setenv ("DISPLAY", data->display, TRUE);
    if(data->sn_id)
        g_setenv ("DESKTOP_STARTUP_ID", data->sn_id, TRUE);
}

const char* expand_terminal_s(char opt, gpointer unused)
{
    return fm_config->terminal;
}

static FmAppCommandParseOption expand_terminal_options[] =
{
    {'s', &expand_terminal_s },
    { 0, NULL }
};

static char* expand_terminal(char* cmd)
{
    char* ret;
    /* add terminal emulator command */

    if(fm_app_command_parse(cmd, expand_terminal_options, &ret, NULL) > 0)
        return ret;
    else /* if %s is not found, fallback to -e */
    {
        const char* term = fm_config->terminal;
        char* ret2 = ret;
        /* bug #3457335: Crash on application start with Terminal=true. */
        if(!term) /* fallback to xterm if a terminal emulator is not found. */
        {
            /* FIXME: we should not hard code xterm here. :-(
             * It's better to prompt the user and let he or she set
             * his preferred terminal emulator. */
            term = "xterm";
        }
        ret = g_strdup_printf("%s -e %s", term, ret2);
        g_free(ret2);
    }
    return ret;
}

static gboolean do_launch(GAppInfo* appinfo, const char* full_desktop_path, GKeyFile* kf, GList* gfiles, GAppLaunchContext* ctx, GError** err)
{
    gboolean ret = FALSE;
    char* cmd, *path;
    char** argv;
    int argc;
    gboolean use_terminal;
    GAppInfoCreateFlags flags;

    cmd = expand_exec_macros(appinfo, full_desktop_path, kf, gfiles);
    if(G_LIKELY(kf))
        use_terminal = g_key_file_get_boolean(kf, "Desktop Entry", "Terminal", NULL);
    else
    {
        flags = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(appinfo), "flags"));
        use_terminal = (flags & G_APP_INFO_CREATE_NEEDS_TERMINAL) != 0;
    }

    if(use_terminal)
    {
        char* term_cmd = expand_terminal(cmd);
        g_free(cmd);
        cmd = term_cmd;
    }

    g_debug("launch command: <%s>", cmd);
    if(g_shell_parse_argv(cmd, &argc, &argv, err))
    {
        struct ChildSetup data;
        if(ctx)
        {
            gboolean use_sn;
            if(G_LIKELY(kf) && g_key_file_has_key(kf, "Desktop Entry", "StartupNotify", NULL))
                use_sn = g_key_file_get_boolean(kf, "Desktop Entry", "StartupNotify", NULL);
            else if(fm_config->force_startup_notify)
            {
                /* if the app doesn't explicitly ask us not to use sn,
                 * and fm_config->force_startup_notify is TRUE, then
                 * use it by default, unless it's a console app. */
                use_sn = !use_terminal; /* we only use sn for GUI apps by default */
                /* FIXME: console programs should use sn_id of terminal emulator instead. */
            }
            else
                use_sn = FALSE;
            data.display = g_app_launch_context_get_display(ctx, appinfo, gfiles);

            if(use_sn)
                data.sn_id = g_app_launch_context_get_startup_notify_id(ctx, appinfo, gfiles);
            else
                data.sn_id = NULL;
        }
        else
        {
            data.display = NULL;
            data.sn_id = NULL;
        }
        g_debug("sn_id = %s", data.sn_id);

        if(G_LIKELY(kf))
            path = g_key_file_get_string(kf, "Desktop Entry", "Path", NULL);
        else
            path = NULL;

        ret = g_spawn_async(path, argv, NULL,
                            G_SPAWN_SEARCH_PATH,
                            child_setup, &data, NULL, err);
        g_free(path);
        g_free(data.display);
        g_free(data.sn_id);

        g_strfreev(argv);
    }
    g_free(cmd);
    return ret;
}

/**
 * fm_app_info_launch
 * @appinfo: application info to launch
 * @files: (element-type GFile): files to use in run substitutions
 * @launch_context: (allow-none): a launch context
 * @error: (out) (allow-none): location to store error
 *
 * Launches desktop application doing substitutions in application info.
 *
 * Returns: %TRUE if application was launched.
 *
 * Since: 0.1.15
 */
gboolean fm_app_info_launch(GAppInfo *appinfo, GList *files,
                            GAppLaunchContext *launch_context, GError **error)
{
    gboolean supported = FALSE, ret = FALSE;
    if(G_IS_DESKTOP_APP_INFO(appinfo))
    {
        const char*id = g_app_info_get_id(appinfo);
        if(id) /* this is an installed application */
        {
            /* load the desktop entry file to obtain more info */
            GKeyFile* kf = g_key_file_new();
            char* rel_path = g_strconcat("applications/", id, NULL);
            char* full_desktop_path;
            supported = g_key_file_load_from_data_dirs(kf, rel_path, &full_desktop_path, 0, NULL);
            g_free(rel_path);
            if(supported)
            {
                ret = do_launch(appinfo, full_desktop_path, kf, files, launch_context, error);
                g_free(full_desktop_path);
            }
            g_key_file_free(kf);
        }
        else
        {
#if GLIB_CHECK_VERSION(2,24,0)
            const char* file = g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(appinfo));
            if(file) /* this is a desktop entry file */
            {
                /* load the desktop entry file to obtain more info */
                GKeyFile* kf = g_key_file_new();
                supported = g_key_file_load_from_file(kf, file, 0, NULL);
                if(supported)
                    ret = do_launch(appinfo, file, kf, files, launch_context, error);
                g_key_file_free(kf);
            }
            else
#endif
            {
                /* If this is created with fm_app_info_create_from_commandline() */
                if(g_object_get_data(G_OBJECT(appinfo), "flags"))
                {
                    supported = TRUE;
                    ret = do_launch(appinfo, NULL, NULL, files, launch_context, error);
                }
            }
        }
    }
    else
        supported = FALSE;

    if(!supported) /* fallback to GAppInfo::launch */
        return g_app_info_launch(appinfo, files, launch_context, error);
    return ret;
}

/**
 * fm_app_info_launch_uris
 * @appinfo: application info to launch
 * @uris: (element-type char *): URIs to use in run substitutions
 * @launch_context: (allow-none): a launch context
 * @error: (out) (allow-none): location to store error
 *
 * Launches desktop application doing substitutions in application info.
 *
 * Returns: %TRUE if application was launched.
 *
 * Since: 0.1.15
 */
gboolean fm_app_info_launch_uris(GAppInfo *appinfo, GList *uris,
                                 GAppLaunchContext *launch_context, GError **error)
{
    GList* gfiles = NULL;
    gboolean ret;

    for(;uris; uris = uris->next)
    {
        GFile* gf = fm_file_new_for_uri((char*)uris->data);
        if(gf)
            gfiles = g_list_prepend(gfiles, gf);
    }

    gfiles = g_list_reverse(gfiles);
    ret = fm_app_info_launch(appinfo, gfiles, launch_context, error);

    g_list_foreach(gfiles, (GFunc)g_object_unref, NULL);
    g_list_free(gfiles);
    return ret;
}

/**
 * fm_app_info_launch_default_for_uri
 * @uri: the uri to show
 * @launch_context: (allow-none): a launch context
 * @error: (out) (allow-none): location to store error
 *
 * Utility function that launches the default application
 * registered to handle the specified uri. Synchronous I/O
 * is done on the uri to detect the type of the file if
 * required.
 *
 * Returns: %TRUE if application was launched.
 *
 * Since: 0.1.15
 */
gboolean fm_app_info_launch_default_for_uri(const char *uri,
                                            GAppLaunchContext *launch_context,
                                            GError **error)
{
    /* FIXME: implement this */
    return g_app_info_launch_default_for_uri(uri, launch_context, error);
}

/**
 * fm_app_info_create_from_commandline
 * @commandline: the commandline to use
 * @application_name: (allow-none): the application name, or %NULL to use @commandline
 * @flags: flags that can specify details of the created #GAppInfo
 * @error: (out) (allow-none): location to store error
 *
 * Creates a new #GAppInfo from the given information.
 *
 * Note that for @commandline, the quoting rules of the Exec key of the
 * <ulink url="http://freedesktop.org/Standards/desktop-entry-spec">freedesktop.org Desktop
 * Entry Specification</ulink> are applied. For example, if the @commandline contains
 * percent-encoded URIs, the percent-character must be doubled in order to prevent it from
 * being swallowed by Exec key unquoting. See the specification for exact quoting rules.
 *
 * Returns: (transfer full): new #GAppInfo for given command.
 *
 * Since: 0.1.15
 */
GAppInfo* fm_app_info_create_from_commandline(const char *commandline,
                                              const char *application_name,
                                              GAppInfoCreateFlags flags,
                                              GError **error)
{
    GAppInfo* app = g_app_info_create_from_commandline(commandline, application_name, flags, error);
    g_object_set_data(G_OBJECT(app), "flags", GUINT_TO_POINTER(flags));
    return app;
}
