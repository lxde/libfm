//      condition.vala
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

// FIXME: we can use getgroups() to get groups of current process
// then, call stat() and stat.st_gid to handle capabilities
// in this way, we don't have to call euidaccess

public enum FileActionCapability {
	OWNER = 0,
	READABLE = 1 << 1,
	WRITABLE = 1 << 2,
	EXECUTABLE = 1 << 3,
	LOCAL = 1 << 4
}

[Compact]
public class FileActionCondition {
	
	public FileActionCondition(KeyFile kf, string group) {
		only_show_in = Utils.key_file_get_string_list(kf, group, "OnlyShowIn");
		not_show_in = Utils.key_file_get_string_list(kf, group, "NotShowIn");
		try_exec = Utils.key_file_get_string(kf, group, "TryExec");
		show_if_registered = Utils.key_file_get_string(kf, group, "ShowIfRegistered");
		show_if_true = Utils.key_file_get_string(kf, group, "ShowIfTrue");
		show_if_running = Utils.key_file_get_string(kf, group, "ShowIfRunning");
		mime_types = Utils.key_file_get_string_list(kf, group, "MimeTypes");
		base_names = Utils.key_file_get_string_list(kf, group, "Basenames");
		match_case = Utils.key_file_get_bool(kf, group, "Matchcase");

		var selection_count_str = Utils.key_file_get_string(kf, group, "SelectionCount");
		if(selection_count_str != null) {
			switch(selection_count_str[0]) {
			case '<':
			case '>':
			case '=':
				selection_count_cmp = selection_count_str[0];
				const string s = ">%d";
				s.scanf(out selection_count);
				break;
			default:
				selection_count_cmp = '>';
				selection_count = 0;
				break;
			}
		}
		else {
			selection_count_cmp = '>';
			selection_count = 0;
		}

		schemes = Utils.key_file_get_string_list(kf, group, "Schemes");
		folders = Utils.key_file_get_string_list(kf, group, "Folders");
		var caps = Utils.key_file_get_string_list(kf, group, "Capabilities");
		foreach(unowned string cap in caps) {
			stdin.printf("%s\n", cap);
		}
	}

	private static bool match_mime_type(List<FileInfo> files, string? common_mime_type, string allowed_mime_type) {
		bool allowed = false;
		if(allowed_mime_type == "all/all" || allowed_mime_type == "*") {
			allowed = true;
		}
		else if(allowed_mime_type == "all/files") {
			// see if all fileinfos are files
			allowed = true;
			foreach(unowned FileInfo fi in files) {
				if(fi.is_dir()) { // at least 1 of the fileinfos is not a file.
					allowed = false;
					break;
				}
			}
		}
		else if (allowed_mime_type.has_suffix("/*")) {
			string prefix = allowed_mime_type[0:-1];
			if(common_mime_type.has_prefix(prefix))
				allowed = true;
		}
		else if(allowed_mime_type == common_mime_type) {
			allowed = true;
		}
		return allowed;
	}

	private bool match_base_name(List<FileInfo> files, string allowed_base_name) {
		// all files should match the base_name pattern.
		bool allowed = true;
		if(allowed_base_name.index_of_char('*') >= 0) {
			string allowed_base_name_ci;
			if(!match_case) {
				allowed_base_name_ci = allowed_base_name.casefold(); // FIXME: is this ok?
				allowed_base_name = allowed_base_name_ci;
			}
			var pattern= new PatternSpec(allowed_base_name);
			foreach(unowned FileInfo fi in files) {
				unowned string name = fi.get_name();
				if(match_case) {
					if(!pattern.match_string(name)) {
						allowed = false;
						break;
					}
				}
				else {
					if(!pattern.match_string(name.casefold())) {
						allowed = false;
						break;
					}
				}
			}
		}
		else {
			foreach(unowned FileInfo fi in files) {
				unowned string name = fi.get_name();
				if(match_case) {
					if(allowed_base_name != name) {
						allowed = false;
						break;
					}
				}
				else {
					if(allowed_base_name.collate(name) != 0) {
						allowed = false;
						break;
					}
				}
			}
		}
		return allowed;
	}

	private static bool match_folder(string folder, string allowed_folder) {
		var pattern = new PatternSpec(allowed_folder);
		// FIXME: this is inefficient and incorrect
		// trailing /* should always be implied.
		return pattern.match_string(folder);
	}

	public bool match(List<FileInfo> files) {
		// all of the condition are combined with AND
		// So, if one of the conditions is not matched, we quit.

		// TODO: OnlyShowIn, NotShowIn

		if(try_exec != null) {
			var exec_path = Environment.find_program_in_path(
					file_action_expand_parameters(try_exec, files));
			if(!FileUtils.test(exec_path, FileTest.IS_EXECUTABLE)) {
				return false;
			}
		}

		if(show_if_registered != null) {
			var service = file_action_expand_parameters(show_if_registered, files);
			// TODO, check if the dbus service is registered
			return false;  // we do not support this now
		}

		if(show_if_true != null) {
			var cmd = file_action_expand_parameters(show_if_true, files);
			int exit_status;
			// FIXME: should we pass the command to sh instead?
			if(!Process.spawn_command_line_sync(cmd, null, null, out exit_status)
				|| exit_status != 0)
				return false;
		}

		if(show_if_running != null) {
			var process_name = file_action_expand_parameters(show_if_running, files);
			// FIXME: how to check if a process is running?
			return false;  // we do not support this now
		}

		if(mime_types != null) {
			// check if all files are of the same type.
			unowned FileInfo first_file = files.first().data;
			string common_mime_type = first_file.get_mime_type().get_type();
			foreach(unowned FileInfo fi in files.next) {
				string mime_type = fi.get_mime_type().get_type();
				if(mime_type != common_mime_type)
					// some files are of different types
					common_mime_type = null;
					break;
			}

			bool allowed = false;
			// see if the files match any mime_type listed in mime_types.
			foreach(unowned string allowed_type in mime_types) {
				// FIXME: need to confirm with the DES-EMA author
				// about the priority of allowed mime_types and negated mime_types.
				// For example, if MimeTypes=audio/*;!audio/mpeg; it's quite clear
				// that we want all audio files other than audio/mpeg.
				// However, what will happen if we have MimeTypes=!audio/mpeg; only?
				// Does this mean all files other than audio/mpeg, or should all/all
				// be listed before !audio/mpeg?
				if(allowed && allowed_type[0] == '!') { // negated
					// files of this type are disallowed
					unowned string disallowed_type = (string)((uint8*)allowed_type + 1);
					if(match_mime_type(files, common_mime_type, disallowed_type))
						allowed = false;
				}
				else {
					// files of this type are allowed
					if(match_mime_type(files, common_mime_type, allowed_type))
						allowed = true;
				}
			}
			if(!allowed)
				return false;
		}

		if(base_names != null) {
			bool allowed = false;
			// see if the files match any mime_type listed in mime_types.
			foreach(unowned string allowed_base_name in base_names) {
				// FIXME: need to confirm with the DES-EMA author
				// about the priority of allowed basenames and negated basenames.
				if(allowed && allowed_base_name[0] == '!') { // negated
					// files of this type are disallowed
					unowned string disallowed_base_name = (string)((uint8*)allowed_base_name + 1);
					if(match_base_name(files, disallowed_base_name))
						allowed = false;
				}
				else {
					// files of this type are allowed
					if(!allowed && match_base_name(files, allowed_base_name))
						allowed = true;
				}
			}
			if(!allowed)
				return false;
		}

		uint n_files = files.length();
		switch(selection_count_cmp) {
		case '<':
			if(n_files >= selection_count)
				return false;
			break;
		case '=':
			if(n_files != selection_count)
				return false;
			break;
		case '>':
			if(n_files <= selection_count)
				return false;
			break;
		}

		if(schemes != null) {
			// see if all files have the same scheme
			unowned FileInfo first_file = files.first().data;
			var uri = first_file.get_path().to_uri();
			string common_scheme = Uri.parse_scheme(uri);
			foreach(unowned FileInfo fi in files.next) {
				// FIXME: this is inefficient
				uri = fi.get_path().to_uri();
				var scheme = Uri.parse_scheme(uri);
				if(common_scheme != scheme)
					return false;
			}

			bool allowed = false;
			foreach(unowned string allowed_scheme in schemes) {
				if(allowed && allowed_scheme[0] == '!') { // negated schemes
					unowned string disallowed = (string)((uint8*)allowed_scheme + 1);
					if(disallowed == common_scheme)
						allowed = false;
						break;
				}
				else if(!allowed && common_scheme == allowed_scheme) {
					allowed = true;
					break;
				}
			}
			if(!allowed)
				return false;
		}

		if(folders != null) {
			var common_dir = files.first().data.get_path();
			// see if all files are in the same folder
			foreach(unowned FileInfo fi in files.next) {
				var dir = fi.get_path();
				if(!common_dir.equal(dir)) {
					common_dir = null;
					break;
				}
			}
			var common_folder = common_dir != null ? common_dir.to_str() : null;
			// FIXME: we do some limitation here for ease of implementation.
			// all files should be in one common folder in order to apply the match rule.
			if(common_folder == null)
				return false;

			var allowed = false;
			foreach(unowned string allowed_folder in folders) {
				if(allowed && allowed_folder[0] == '!') {
					unowned string disallowed_folder = (string)((uint8)allowed_folder+1);
					if(match_folder(common_folder, disallowed_folder))
						allowed = false;
				}
				else if(!allowed) {
					if(match_folder(common_folder, allowed_folder))
						allowed = true;
				}
			}
			if(!allowed)
				return false;
		}

		// TODO: Capabilities
		// currently, due to limitations of Fm.FileInfo, this cannot
		// be implemanted correctly.

		return true;
	}

	public string[]? only_show_in;
	public string[]? not_show_in;
	public string? try_exec;
	public string? show_if_registered;
	public string? show_if_true;
	public string? show_if_running;
	public string[]? mime_types;
	public string[]? base_names;
	public bool match_case;
	public char selection_count_cmp;
	public int selection_count;
	public string[]? schemes;
	public string[]? folders;
	public FileActionCapability capabilities;
}

}
