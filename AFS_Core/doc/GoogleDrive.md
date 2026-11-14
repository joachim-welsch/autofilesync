# GoogleDrive

Module comment for GoogleDrive.h / GoogleDrive.cpp.

## google_drive

The Google Drive client: the same functions as the WebDAV client (WebDav.md#webdav), on the
Drive API v3. The synchronisation works with paths, the drive with identifiers and parents, so
most of this module translates one into the other.

## DriveRequest

A request carrying the account's access token. The token is fetched before the request is
built, because fetching it is a request of its own on the same connection
(HttpClient.md#Request).

## queries

Folders are read with files.list and the query `'<folder id>' in parents and trashed = false`,
narrowed by `name = '<name>'` when one entry is wanted. Inside the quotes a quote and a
backslash are escaped with a backslash. The answer comes in pages; every page is fetched.

## json

The few JSON bodies sent are assembled by hand; only strings need care, and json_text() quotes
them. Answers are read with the property tree.

## Place

Where a path leads: the identifier of its folder, its name, and the file or folder with that
name if there is one. Nothing when the folder itself does not exist.

## folder-cache

The identifier of every folder found or created is remembered per account and path, so a tree
is not walked from the root again for each file. Files are not remembered; they are looked up
in their folder each time. After a removal or a move the whole cache is dropped, because a
remembered path below the moved folder would lead to the old place.

## same-names

A drive may hold several entries of one name in a folder. The first one the query returns is
taken. An upload replaces the content of an existing entry instead of adding a second, so the
program itself never creates such twins.

## google-documents

Documents, sheets and other files in Google's own formats have no content to download and no
size. They are left out as if they were not there. With the `drive.file` scope they are rare
anyway, since the program only sees what it created (GoogleAuth.md#scope).

## list

The entries of a folder. A name that is not a plain entry name is left out (WebDav.md#entry-names).

## remove

Moves the entry into the drive's trash instead of deleting it for good. The user can take it
back there for thirty days, which fits the program's rule that nothing is lost.

## move

A rename, a move to another folder or both in one request. An existing entry at the destination
is refused first: the drive would happily create a twin of the same name, and the callers rely
on a move never replacing anything.

## upload

A resumable upload in two requests: the first sends the metadata (the modification time, and
for a new file its name and folder) and receives the address of an upload session in the
Location header, the second sends the content there. The session address is used only when it
is encrypted or local. The drive keeps the modification time, so comparisons by time work as
with a local target.
