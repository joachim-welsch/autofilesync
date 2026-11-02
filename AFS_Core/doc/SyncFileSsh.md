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

Resolves "account:/path", connects - or shares the pass's session, see
SftpConnection.md#SshSessionScope - and returns the remote part of the path

## unreachable-is-a-warning

A host that cannot be reached is an expected state, not an error - see
SftpConnection.md#SshUnreachable.

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

## upload-keeps-the-permissions

The other direction: a file uploaded from a local source used to arrive as rw------- for the
SSH user, whatever the source had - a script lost its execute bit, a shared document its group
read. The upload now carries the source's nine permission bits over like the download does,
through the same keep_remote_permissions() (so the owner keeps write, see below). Only on Unix:
a Windows source has no POSIX bits, and the libssh2 branch leaves the remote default.

## remote-target-stays-writable

A remote target is overwritten in place the next time the source changes, which
a target without write permission would refuse. The owner therefore always keeps
the right to write. A local target needs no such exception: SyncFile moves the
old file aside and the download creates a new one. A remote target under the father-son
principle is moved aside the same way (SyncFile.md#remote-father-son); every other one is
overwritten in place.

## ssh_rmdir

Removes an empty remote directory, for a directory whose files a two-way sync deleted because
the other side dropped it (SyncBase.md#remove_dir_if_empty). SFTP refuses a directory that is
not empty, so the caller checks first and nothing with content is ever removed here. In the
libssh2 build the connection dance of ssh_remove_file() is shared, since only the final SFTP
call differs.

## ssh_mkdir_p

sftp_mkdir() makes one level. A copy back into a directory the source side no longer has
(SyncFile.md#parent-of-a-kept-file) may need several, so this walks up to the first existing
ancestor - never past the account's root - and creates the rest on the way down.

## ssh_rename

Renames or moves a file on the server, within the one account both paths name, for a file the
other side renamed (SyncFile.md#renames). The new path's directory has to exist; the caller
creates it (ssh_mkdir_p). SFTP's rename refuses an existing destination on most servers, and
a rename is only attempted where the comparison found none, so that is never relied upon.

## pipelined-requests

One SFTP request per 64 KiB block, each waiting for its answer, made a copy over WLAN crawl at
about 1 MiB/s against a phone whose round trip is 20-50 ms (ANDROID-CONCEPT.md section 13).
libssh 0.11's sftp_aio_* calls let several requests travel at once: ReadPipeline keeps
REQUESTS_IN_FLIGHT reads outstanding and hands the blocks back in order, WritePipeline sends
blocks ahead and collects the answers, at the end or when the window is full. Sixteen requests
of 64 KiB are a megabyte in flight, enough for a home network and small enough for the
server's window. A write request may be shortened by the server's limit; the remainder goes as
a request of its own, with its data kept alive until its answer arrives. Order is preserved on
both sides, so a stop between two blocks leaves the file as a sequential copy would have. The
libssh2 branch (Windows) still sends one request at a time.

## part-files

A copy to a phone (an account with a pinned host key, Config.md#device-accounts) is written
under the final name plus PART_SUFFIX and renamed when it is complete, with time and
permissions already set. A pass that is interrupted - the phone leaves the Wi-Fi, the battery
saver stops the app - then leaves the old file untouched instead of a truncated one that the
gallery or music app would show as broken; the part file is removed on error or stop, and a
part file that survived a crash is excluded from every listing (SyncItem.md#is_excluded). The
rename falls back to unlink and rename, because SFTP version 3 refuses to rename onto an
existing file; between the two calls the file is briefly missing, never half written. Other
SSH targets keep writing in place, as before (remote-target-stays-writable), so that a server's
ownership, hard links and ACLs on existing files are not replaced by a new file. The libssh2
branch (Windows) writes in place for phones as well.
