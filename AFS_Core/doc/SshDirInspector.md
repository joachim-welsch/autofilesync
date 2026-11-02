# SshDirInspector

Module comment for SshDirInspector.h / SshDirInspector.cpp.

## get_home_dir

SFTP has no command for the home directory; by convention the server resolves "." relative to
the login directory.

## require_directory

Fills exists and is_directory; false with error_message set when the path cannot be listed

## readdir-eof-vs-error

NULL from sftp_readdir means EOF or error; only sftp_dir_eof tells them apart, and a partial
listing must not pass as complete.

## entry-time

The entry's time text comes from SshFileInspector's time_to_iso() so that every SSH listing
in the GUI shows the same format (SshFileInspector.md#time_to_iso).

## entry-names

Every listing drops "." and "..", and refuses with an error in the log any name
FilesystemUtil.md#plain_entry_name rejects. The dialog then shows the rest of the directory;
the error names the directory and the entry so the user can see what the server sent.
