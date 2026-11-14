# WebDav

Module comment for WebDav.h / WebDav.cpp.

## webdav

The WebDAV client: the handful of HTTP requests a synchronisation needs, spoken through
libcurl. It knows accounts and paths on the server and nothing about synchronisation items;
the remote functions of SyncFileSsh call it for accounts that carry an address
(SyncFileSsh.md#webdav-dispatch). Paths are UTF-8, start with "/" and are relative to the
account's address.

## errors

Every function throws (HttpClient.md#errors): `SshUnreachable` when the server could not be
reached, `Error` for everything else, including an HTTP status that is not a success. A wrong
password is an Error, not an absence: reporting "file does not exist" for a 401 would make a
mirroring item believe the whole target is empty.

## Request

An HTTP request (HttpClient.md#Request) with the account's credentials, refused before it is
sent when the account's address is not encrypted (#https-only).

## https-only

The password travels with every request (Basic authentication), so the address has to begin
with https://. Plain http is accepted for this computer alone (localhost, 127.0.0.1, ::1),
which is what the tests use. The check compares the host exactly up to the next ":" or "/", so
"http://localhost.example.org" does not pass.

## urls

The account's address plus the path, each path segment percent-encoded on its own so a "/" stays
a separator and a "%" or a blank in a name survives. A collection is addressed with a trailing
slash: Apache answers a collection without one with a redirect, and redirects are not followed.

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

PUT with the file's size known beforehand (HttpClient.md#transfers), directly under the final
name.

## modification-time

WebDAV has no standard way to set a file's modification time. Nextcloud and ownCloud take it
from the header "X-OC-Mtime" and confirm with the same header in the answer. Other servers
ignore the header and stamp the file with the time of the upload. That is survivable: a later
change of the source is still newer than the upload, and a two-way item compares against the
state it recorded after the upload. But "copy when the target is newer" would then see every
uploaded file as newer, so a warning names the connection once per program run when a server
did not confirm.
