# OneDrive

Module comment for OneDrive.h / OneDrive.cpp.

## one_drive

The OneDrive client: the same functions as the WebDAV client (WebDav.md#webdav), on Microsoft
Graph. It serves private Microsoft accounts and OneDrive for Business alike, since both answer
under `/me/drive`.

## addresses

Graph addresses an item by its path: `/me/drive/root:/folder/file.txt` for the item itself and
`/me/drive/root:/folder/file.txt:/children` for something of it, with the root written without
the colons. Each segment of the path is percent-encoded. No identifiers have to be looked up,
which is why this client needs no cache.

## json-keys

Graph uses property names with dots and an at sign ("@odata.nextLink",
"@microsoft.graph.downloadUrl"). The property tree would take a dot for a step into a child, so
the keys are given with another separator.

## modification-time

An item has two modification times: `lastModifiedDateTime`, which the server sets on every
change, and `fileSystemInfo.lastModifiedDateTime`, which the client may set. The second is
written on upload and read for the comparison, so times survive as with a local target.

## list

The children of a folder, page by page: the answer names the address of the next page, which is
followed only when it is encrypted or local. A name that is not a plain entry name is left out
(WebDav.md#entry-names).

## remove

DELETE. OneDrive puts the item into its recycle bin, from which the user can take it back.

## move

A rename, a move to another folder or both in one PATCH. An existing item at the destination is
refused first, and the request asks the server to fail on a conflict as well, so a move never
replaces anything.

## download

The item's metadata carries a short-lived address from which the content can be fetched
without a token (`@microsoft.graph.downloadUrl`). Asking for the content directly would answer
with a redirect to that address, and redirects are not followed.

## upload

An upload session: one request names the file, asks to replace an existing one and sets the
modification time, and returns an address to which the content is sent in pieces of ten
mebibytes, each with its byte range. Microsoft requires pieces to be multiples of 320 KiB and
at most 60 MiB; one piece per request keeps a failure cheap. The progress display gets the
bytes of the pieces before added, so it does not start over with each piece.

## empty-files

An upload session cannot carry zero bytes. An empty file is written with a plain PUT of its
content, and its modification time is set with a second request.
