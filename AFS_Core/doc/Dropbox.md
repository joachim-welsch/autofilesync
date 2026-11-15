# Dropbox

Module comment for Dropbox.h / Dropbox.cpp.

## dropbox

The Dropbox client: the same functions as the WebDAV client (WebDav.md#webdav), on the Dropbox
API v2.

## two-hosts

Dropbox answers under two addresses. Calls about names and folders go to the API address the
account carries and send their arguments as a JSON body. Calls that carry file content go to
the content address from the sign-in settings (GoogleAuth.md#dropbox) and send their arguments
in the header "Dropbox-API-Arg", with the content as the body.

## header-safe-json

An HTTP header may only hold ASCII, so every character outside it is written as a \u escape,
those beyond the basic plane as a surrogate pair. The same form is used for the bodies; it is
valid JSON there too, and one function is less to get wrong.

## json-keys

Dropbox marks the kind of an entry with a property called ".tag". The property tree would take
the dot for a step into a child, so the keys are given with another separator.

## paths

Dropbox wants the root as an empty text and every other path with a leading slash and without a
trailing one. It ignores the case of a path and keeps the case of a name; the names the
synchronisation sees are those the listing returns.

## errors

A call that fails for a reason of its own (not found, name taken) answers with status 409 and
an "error_summary" such as "path/not_found/..". call() returns that summary to the function,
which knows which summaries are an answer and which are a failure. Every other status that is
not a success is an Error.

## list

The entries of a folder, continued with the cursor while Dropbox says there are more. Entries
that are neither file nor folder (the listing can report deleted ones) and names that are not
plain entry names are left out (WebDav.md#entry-names).

## remove

`delete_v2`. Dropbox keeps a deleted file for thirty days or longer, depending on the plan, and
the user can restore it on its web page.

## move

`move_v2` without automatic renaming: Dropbox refuses when the destination exists, so a move
never replaces anything.

## download

The content call with GET, which Dropbox allows for downloads. The modification time is asked
for first, with the metadata, and set on the local file.

## upload

A file of up to eight mebibytes goes up in one request. A larger one uses an upload session:
start with the first piece, append the middle ones, finish with the last piece and the commit
that names the path. The commit asks to overwrite, sets `client_modified` to the file's time,
which Dropbox keeps to the second, and asks Dropbox not to notify the user's devices for every
file. The progress display gets the bytes of the pieces before added.
