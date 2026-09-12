# SyncFile

Module comment for SyncFile.h / SyncFile.cpp.

## wait_for_updated_filetime_call_count

Counts calls to the free wait_for_updated_filetime() below, so a regression test can catch a
caller waiting for the same filetime confirmation twice.

## bounded-retries

Bounded retries: transient errors right after a copy settle quickly, but a persistent one
(target drive unplugged) must not spin forever - the GUI's stop_sync() busy-waits for this
thread to finish.

## restore_backup

Removes a failed or aborted copy and puts the previous target back.

## ssh-skips-verification-only

SSH skips only the content verification below (TODO); Error/Stopped must still run the
cleanup/restore paths, or a failed transfer is reported as success - the caller would then e.g.
delete the source (del_aftercopy).

## only-count-touches-nothing

only_count_ means "count only" - it must not touch the filesystem, the way
SyncBase::sync_dir_entry() already handles it for Dir/Tree items.

## empty-source-file

An empty file is more often an aborted write than an intentionally empty file; copying it over
an existing one would lose data. Placeholders like .gitkeep need sync_empty_files.

## volume-is-target-size

The target file is the one being copied here, so its size is the volume moved - filesize is the
source's (see FileDifference).

## ssh-target-delete

util::remove_file() interprets "account:/path" as a bogus local path and silently does nothing
for it. A remote file cannot go into the local recycle bin without downloading it first, so SSH
targets stay a permanent delete.

## failures-return-early

Every failure returns before the bookkeeping below: a copy that did not happen must neither
count as done nor let del_aftercopy delete the source.

## local-only-filetime

fs::last_write_time() only resolves a local path; an SSH source keeps the time the transfer
produced (see pre_copy()'s TODO).
