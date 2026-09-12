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
