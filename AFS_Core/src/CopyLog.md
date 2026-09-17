# CopyLog

Module comment for CopyLog.h / CopyLog.cpp.

## SyncReportGuard

Makes report the one that report_sync_error()/report_sync_volume() feed, for as long as the
guard lives. Per thread, because the GUI and the daemon each run their own sync thread (see
Synchronize::create()).

## report_sync_error

No-ops while no proof is being collected, so the call sites deep in the sync classes need no
condition of their own.

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

## trailing-dots-and-blanks

Trailing dots and blanks are not preserved by Windows file names.

## name-in-every-entry

Repeated in every entry although the file name already carries it: as_file_name() may have
changed the name, and an entry that is copied out of the file has to stay attributable on its
own.
