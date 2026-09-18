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

## transfer-keeps-the-modification-time

A transfer carries the modification time of the file it copies, in every direction: the upload
stamps the remote copy with sftp_setstat(), the download stamps the local one, and a remote to
remote copy passes the source's time on. Without it a remote copy always looked newer than the
original it came from, which makes every comparison by time over SSH useless - see
FileDifference.md#ssh-content-fallback.

The stamp happens after the handle of the written file is closed: a server that updates the
modification time when it closes the file would otherwise overwrite it again. A failed stamp is
logged but leaves the result at OK - the file itself arrived, and reporting an error would make
the caller restore its backup over a perfectly good copy.

## copy-from-ssh-keeps-the-permissions

A copy from an SSH source takes over the permissions of the source, to a local
target as well as to another SSH target. The local file used to get the default
of the process, so a file that was private on the server (rw-------) became
readable for everybody on arrival. Only the nine permission bits: setuid, setgid
and sticky are not carried over, a synchronisation must not hand out such
rights. Set before the time, and like the time after the file is closed and
without touching the result - the file itself arrived. Windows keeps its
behaviour: POSIX permissions have no counterpart there.

## remote-target-stays-writable

A remote target is overwritten in place the next time the source changes, which
a target without write permission would refuse. The owner therefore always keeps
the right to write. A local target needs no such exception: SyncFile moves the
old file aside and the download creates a new one.
