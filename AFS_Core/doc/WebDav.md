# WebDav

Module comment for WebDav.h / WebDav.cpp.

## webdav

The WebDAV client: the handful of HTTP requests a synchronisation needs, spoken through
libcurl. It knows accounts and paths on the server and nothing about synchronisation items;
the remote functions of SyncFileSsh call it for accounts that carry an address
(SyncFileSsh.md#webdav-dispatch). Paths are UTF-8, start with "/" and are relative to the
account's address.

## errors

Every function throws. `SshUnreachable` means the server could not be reached at all (name not
resolved, connection refused, timeout, TLS handshake): the callers treat that as "not there at
the moment" and warn, exactly as for an SSH host. `Error` is everything else, an HTTP status
that is not a success or a transport failure in mid-request. A wrong password is an Error, not
an absence: reporting "file does not exist" for a 401 would make a mirroring item believe the
whole target is empty.

## Request

One HTTP request on the calling thread's connection, with the account's credentials and the
settings every request shares. perform() returns the HTTP status, or nothing when the user
stopped the run during a transfer.

## connection-reuse

Each thread keeps one libcurl handle for its lifetime and resets it between requests. libcurl
then reuses the open connection, so a tree with thousands of files costs one TLS handshake per
thread instead of one per request. The handle is thread-local because an easy handle must not
be used from two threads.

## https-only

The password travels with every request (Basic authentication), so the address has to begin
with https://. Plain http is accepted for this computer alone (localhost, 127.0.0.1, ::1),
which is what the tests use. The check compares the host exactly up to the next ":" or "/", so
"http://localhost.example.org" does not pass.

## urls

The account's address plus the path, each path segment percent-encoded on its own so a "/" stays
a separator and a "%" or a blank in a name survives. A collection is addressed with a trailing
slash: Apache answers a collection without one with a redirect, and redirects are not followed.

## timeouts

Fifteen seconds to connect. After that a request has no total limit, since a large file may
take hours, but it is given up when no byte has moved for a minute.

## stop

Only transfers ask whether the user stopped the run, and they report the bytes moved so far to
the progress display. The small requests do not ask: the dialogs use them too, outside any run,
when Synchronize::stopping() is true all the time.

## stat

A PROPFIND of depth 0. 404 is "not there". A server that redirects a directory asked for
without the trailing slash is asked again with it.

## list

A PROPFIND of depth 1 on the collection. The answer contains the collection itself besides its
members; it is the entry with the shortest address and is left out.

## multistatus

The answer is XML with one `response` per resource. A resource may carry several `propstat`
blocks, one per status; the properties are collected from all of them and a missing one keeps
its default.

## xml-prefixes

Servers choose their own namespace prefix for "DAV:" ("D:", "d:", none at all). Only the local
name after the last colon is compared, which is enough because no other namespace uses the
names asked for.

## entry-names

The name is the last segment of the resource's address, cut off before it is decoded, so an
encoded "%2F" inside a name cannot pose as a separator. A decoded name that still is not a
plain entry name ("..", a name with a slash or backslash) is not listed
(FilesystemUtil.md#plain_entry_name).

## make_directory

MKCOL. A server answers 405 when something already has that name; that counts as success when
the something is a directory, because the callers create directories that may exist.

## move

MOVE with "Overwrite: F": the server refuses to replace an existing resource, which is the
behaviour the callers rely on for SSH as well (SyncFileSsh.md#ssh_rename).

## download

GET into the local file. The file gets the server's modification time, so the downloaded copy
compares as equal to its origin on the next pass.

## upload

PUT with the file's size known beforehand. The "Expect: 100-continue" handshake is switched off:
it only saves an upload the server would refuse, and some servers do not answer it, which costs
a second of waiting per file.

## modification-time

WebDAV has no standard way to set a file's modification time. Nextcloud and ownCloud take it
from the header "X-OC-Mtime" and confirm with the same header in the answer. Other servers
ignore the header and stamp the file with the time of the upload. That is survivable: a later
change of the source is still newer than the upload, and a two-way item compares against the
state it recorded after the upload. But "copy when the target is newer" would then see every
uploaded file as newer, so a warning names the connection once per program run when a server
did not confirm.
