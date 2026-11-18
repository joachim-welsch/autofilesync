# SyncFileSsh

Module comment for SyncFileSsh.h / SyncFileSsh.cpp.

## ssh_is_directory_call_count

Counts real SSH directory checks (each one is a full connect), so tests can verify Synchronize's
per-target cache actually avoids reconnecting.

## short-write

SFTP write calls may legitimately write fewer bytes than requested (short write); only a
negative return is an error. Treating a short write as failure made large uploads abort
sporadically.

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

## listing-failure-is-reported

A remote directory that cannot be opened or read to its end yields no entries or too few, and
the pass then copies nothing or too little from it. That must count as a failed run in the
proof and the last-run record, like an unreadable local directory does - logging alone left
the run looking clean. Nothing is deleted on the strength of such a listing: a target-only
file is only removed after its own source file was looked up and found missing.

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
overwritten in place. A target that lost its write bit by other means is handled below.

## read-only-target-made-writable

The local counterpart is SyncFile::pre_copy(), which gives the owner all rights before the
copy. Doing the same up front over SFTP would cost a stat and a setstat per file for a case
that is rare, since AFS keeps its own targets writable (above). So the open for writing is
tried first, and only a permission-denied answer leads to the setstat with the owner's write
bit and a second open. The permission check reads the file's own bits, so a denial that comes
from the directory keeps failing on the second open and is reported as before. In the libssh2
branch the SFTP status is only meaningful after a protocol error, hence the errno check first.

## ssh_rmdir

Removes an empty remote directory, for a directory whose files a two-way sync deleted because
the other side dropped it (SyncBase.md#remove_dir_if_empty). SFTP refuses a directory that is
not empty, so the caller checks first and nothing with content is ever removed here. In the
libssh2 build the connection dance of ssh_remove_file() is shared, since only the final SFTP
call differs.

## removal-refusal

ssh_remove_file() and ssh_rmdir() with the reason for a failure, for the report of a deletion
(SyncBase.md#deleted): FileNotExist when the server says the entry is not there - which the
caller takes for done (SyncBase.md#already-gone) and which is therefore no error line in the
log -, PermissionDenied when the server refuses, ReadOnlyFilesystem when its drive takes no
writes (ssh_read_only_filesystem), ServerUnreachable when no connection came
about, and a write error for everything the server or the library gives no known reason for.
A WebDAV or cloud store only throws, so there a failure is unreachable or unknown and never
"not there": a missing answer must not pass for a deleted file.

## ssh_read_only_filesystem

Whether the server's drive below a path takes writes, asked before an item runs
(Synchronize.md#write-protected) the way the local side is. SFTP has no code for it that
servers use: OpenSSH answers a write to a read-only file system with the plain "failure" it
has for everything unnamed. What it does offer is the statvfs extension, whose flags say
"read-only" - so that is asked, for the path or, when it is not there yet, for the nearest
part of it that is. A server without the extension, one that cannot be reached, and a WebDAV
or cloud store, which have no such question, count as writable: the write then tells.

A removal the server refused without a known reason asks the same question afterwards, so
the report names the write protection instead of nothing (removal-refusal); servers that do
send "write protect" are taken at their word. A copy and a new directory that failed ask it
as well (SyncFile.md#remote-copy-failure-reason, SyncBase.md#report_remote_dir).

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

## entry-names

ssh_list_dir() is the only way a remote name reaches SyncTree, which appends it to the local
and the remote directory path. A name FilesystemUtil.md#plain_entry_name rejects is left out
of the listing with an error in the log, so a lying server can neither write outside the target
tree nor, in a two-way item, make the source tree lose a directory it never had.

## wide-file-names

The local file is opened through std::filesystem::path, which keeps the wide name on Windows.
A narrow UTF-8 name would be read there in the ANSI code page, and a file with an umlaut in its
name would arrive under a garbled one - see Logging.md#wide-file-names.

## webdav-dispatch

The remote functions (ssh_exists, ssh_list_dir, the three copy functions and so on) are what
the synchronisation calls for any path of the form `<account>:/path`. They used to be the SFTP
implementation itself; that now lives in the namespace `sftp_backend`, once per platform as
before, and the public functions look at the account: one with an address goes to the WebDAV
client (WebDav.md#webdav), every other to SFTP. This is the single place where the two
protocols part, which is why the synchronisation, the comparison and the father-son principle
work over WebDAV without knowing it. The names still say "ssh" for that reason.

## webdav-errors

The WebDAV client throws; the remote functions answer with false or CopyResult::Error like
their SFTP counterparts. An unreachable server is logged as a warning, everything else as an
error (SyncFileSsh.md#unreachable-is-a-warning).

## webdav-through-a-local-file

A copy between two remote sides of which at least one is WebDAV goes through a file in the temp
directory: download, then upload. SFTP to SFTP streams directly, but there is no common stream
between the two libraries, and a server-side copy would only cover the case of one account on
both sides. The file is removed when the copy ends, however it ends.

## links

What SyncLink needs of a server (SyncLink.md#places). ssh_link_info() asks for the entry itself
(lstat) and, for a link, its text and whether it leads to a directory; an entry that is not
there is an answer, a question that failed is none, and the caller must not take the one for
the other. ssh_create_link() creates a link with a text. A server that answers "not supported"
or "permission denied" does not let links be created there - OpenSSH passes on the "operation
not permitted" of a FAT drive as "permission denied" - and that is LinksNotSupported, which the
caller reports once per pass instead of once per link; a directory that is really read-only
shows itself in the files of the same pass. OpenSSH takes the two arguments of the request in
the reverse order of the protocol; libssh knows that, and SyncLink reads every link back
before it uses it. A cloud account has no links: ssh_keeps_links() says so, and the two calls
answer accordingly.

## links-windows

The libssh2 branch does what the libssh one does, with one difference: libssh2 does not know
that OpenSSH takes the two arguments of the request in the reverse order. So the link is
created with the text first, as OpenSSH wants it, and read back. If no link with that text
lies at the place, a server that kept to the protocol has created a link named like the text
that points to the place; when such a stray link is found it is removed, and the request is
made once more the other way round and read back again. Anything else is a failure, and
SyncLink then leaves the old target alone. This branch was written without a Windows build:
it has never been compiled or run, against any server.

## real-path

Where a path really lies on the server, with every link in it resolved, or nothing when the
path is not there or the server does not say. A server resolves a missing path lexically
instead of refusing, so the path is asked for first. The walk that follows links needs it once
for the root and once per directory link (SyncTree.md#followed-links); SshLink::resolves
tells a link that leads somewhere from a dead one without that call.
