//      profile.vala
//      
//      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@pcman.tw@gmail.com>
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
//      
//      

namespace Fm {

public enum FileActionExecMode {
	NORMAL,
	TERMINAL,
	EMBEDDED,
	DISPLAY_OUTPUT
}

[Compact]
public class FileActionProfile {

	public FileActionProfile(KeyFile kf, string profile_name) {
		id = profile_name;
		name = Utils.key_file_get_string(kf, profile_name, "Name");
		exec = Utils.key_file_get_string(kf, profile_name, "Exec");

		path = Utils.key_file_get_string(kf, profile_name, "Path");
		var s = Utils.key_file_get_string(kf, profile_name, "ExecutionMode");
		if(s == "Normal")
			exec_mode = FileActionExecMode.NORMAL;
		else if( s == "Terminal")
			exec_mode = FileActionExecMode.TERMINAL;
		else if(s == "Embedded")
			exec_mode = FileActionExecMode.EMBEDDED;
		else if( s == "DisplayOutput")
			exec_mode = FileActionExecMode.DISPLAY_OUTPUT;
		else
			exec_mode = FileActionExecMode.NORMAL;

		startup_notify = Utils.key_file_get_bool(kf, profile_name, "StartupNotify");
		startup_wm_class = Utils.key_file_get_string(kf, profile_name, "StartupWMClass");
		exec_as = Utils.key_file_get_string(kf, profile_name, "ExecuteAs");
		
		condition = new FileActionCondition(kf, profile_name);
	}

	public bool launch(AppLaunchContext ctx, List<FileInfo> files, out string? output) {
		var exec = FileActionParameters.expand(exec, files);
		bool ret = false;
		if(exec_mode == FileActionExecMode.DISPLAY_OUTPUT) {
			int exit_status;
			ret = Process.spawn_command_line_sync(exec, out output, 
												   null, out exit_status);
			if(ret)
				ret = (exit_status == 0);
		}
		else {
			/*
			AppInfoCreateFlags flags = AppInfoCreateFlags.NONE;
			if(startup_notify)
				flags |= AppInfoCreateFlags.SUPPORTS_STARTUP_NOTIFICATION;
			if(exec_mode == FileActionExecMode.TERMINAL || 
			   exec_mode == FileActionExecMode.EMBEDDED)
				flags |= AppInfoCreateFlags.NEEDS_TERMINAL;
			GLib.AppInfo app = Fm.AppInfo.create_from_commandline(exec, null, flags);
			stdout.printf("Execute command line: %s\n\n", exec);
			ret = app.launch(null, ctx);
			*/

			// NOTE: we cannot use GAppInfo here since GAppInfo does
			// command line parsing which involving %u, %f, and other
			// code defined in desktop entry spec.
			// This may conflict with DES EMA parameters.
			// FIXME: so how to handle this cleaner?
			// Maybe we should leave all %% alone and don't translate
			// them to %. Then GAppInfo will translate them to %, not
			// codes specified in DES.
			ret = Process.spawn_command_line_async(exec);
		}
		return ret;
	}

	public bool match(List<FileInfo> files) {
		return condition.match(files);
	}

	public string id;
	public string? name;
	public string exec;
	public string? path;
	public FileActionExecMode exec_mode;
	public bool startup_notify;
	public string? startup_wm_class;
	public string? exec_as;

	public FileActionCondition condition;
}

}
