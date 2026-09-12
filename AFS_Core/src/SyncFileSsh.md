# SyncFileSsh

Module comment for SyncFileSsh.h / SyncFileSsh.cpp.

## ssh_is_directory_call_count

Counts real SSH directory checks (each one is a full connect), so tests can verify Synchronize's
per-target cache actually avoids reconnecting.

## short-write

SFTP write calls may legitimately write fewer bytes than requested (short write); only a
negative return is an error. Treating a short write as failure made large uploads abort
sporadically.

## resolve_host

Resolves hostname or IP address to sockaddr_in Returns true on success, false on failure

## connect_to

Resolves "account:/path", connects and returns the remote part of the path

## readdir-eof-vs-error

sftp_readdir returns NULL for both EOF and error - only sftp_dir_eof tells them apart. A
connection dropped mid-listing must not pass off the partial listing silently as the complete
directory.
