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

## trailing-dots-and-blanks

Trailing dots and blanks are not preserved by Windows file names.

## name-in-every-entry

Repeated in every entry although the file name already carries it: as_file_name() may have
changed the name, and an entry that is copied out of the file has to stay attributable on its
own.
