# CopyLog

Module comment for CopyLog.h / CopyLog.cpp.

## SyncReportGuard

Makes report the one that report_sync_error()/report_sync_volume() feed, for as long as the
guard lives. Per thread, because the GUI and the daemon each run their own sync thread (see
Synchronize::create()).

## report_sync_error

No-ops while no proof is being collected, so the call sites deep in the sync classes need no
condition of their own.

## reach

How far a reason reaches decides what follows from it, and that is laid down once for every
reason (FilesystemUtil.md#FileInfoResult) instead of at each place that meets it. Most reach
one file: an error, and the pass goes on. Some hold for everything else the item would do
on that side - the drive takes no writes, the server is gone or does not answer in time or
says "later", the login is refused or has expired - and end the item's pass (abandon). A
shortage is a file that does not fit: the pass goes on, and the files are summed up
(shortage). A hint is no failure at all but something the place cannot do, told once per
pass with a count (SyncBase.md#report_links). Shortages and hints need more than a message -
a size, a tally - so their callers report them through the calls made for that;
report_sync_refusal() only acts on the reach "item" and takes every other reason for an
ordinary error.

## abandon

A refusal that holds for the whole drive or server ends the item's pass (reach): after
"read-only file system" (FilesystemUtil.md#read_only_error) every further file would fail
the same way and fill the report up to its limit, and after a server that cannot be reached
every further file would also wait for the connect timeout first. report_sync_refusal() takes the reason along with the message; for
this reason the report keeps the message once, with the note that the remaining files were
skipped, and counts as abandoned. Synchronize::stopping() then answers yes for the rest of
this report's life, so every loop that honours a stop leaves the item alone - and the next
item, which has a report of its own, runs as usual. The pass is not marked complete, so the
state forgets nothing it did not see. Without a report (counting, preview, a caller that
collects none) the refusal is an ordinary error and nothing is abandoned.

## write_copy_log

Appends one entry to <directory>/<sanitised item name>.json, creating the file and the directory
as needed. Returns false if the entry did not reach the file - which is worth logging, since a
missing proof is the one thing this feature must not do quietly.

## entries_of

The list of entries is the only member of the file's root. Its name is translated when the file
is created and never looked up afterwards: a file begun in one language is continued under the
name it has, because a second list under the name of the current language would split the proof
in two. push_back() instead of put_child(): the latter reads a '.' in the name as a path
separator, and the name comes out of a catalogue.

## utf8-file

The file is UTF-8, written and read through util::write_utf8_file() and util::read_utf8_file() -
see FilesystemUtil.md#utf8-files for why not through a wide stream. Before, a German umlaut in a name or
path, or the field names of French and Polish, meant that no proof was written at all.
json::to_text() writes every character as itself, so a Ukrainian proof reads as Ukrainian.

## add_field

push_back() instead of put() for the same reason as in entries_of: a translated field name may
contain a '.'.

## build_entry

Field names and status are written in the language of the moment, so a file that outlives a
change of language holds entries in both. That is accepted: every entry is complete in itself,
and rewriting the older ones would mean altering evidence.

## untranslated-file-name

The name of the file decides which file an entry is appended to. Translated, it would start a
second file for the same sync item after a change of language.

## name-in-every-entry

Repeated in every entry although the file name already carries it: as_file_name() may have
changed the name, and an entry that is copied out of the file has to stay attributable on its
own.

## shortage

A file that does not fit on the drive it should go to. A full backup disk refuses every file
of the pass, and one error per file would bury the one fact the user needs under thousands of
lines. So the report keeps a single entry for it: the first file with the room it needs and
the room there is, and from the second file on a summary in the same place - how many files,
how much they need together, and the first of them as an example. The entry keeps its
position among the other errors. The entry begins with the reason - the local disk or the
server (SyncFile.md#disk-full-on-a-server) - and keeps the first one's. Megabytes are
decimal, as file managers show them. Past the
error limit no entry is kept, and successful() asks the count as well, so the run still fails.
