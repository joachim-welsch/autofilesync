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

## discard_backup

The counterpart for the successful copy: the ".bak" exists only so restore_backup() has
something to put back, so it must not outlive the copy it protected. A leftover would not just
waste the space of a second full copy - with two_way_sync it is a source-only file for the next
pass and gets copied to the other side, and the sync never settles.

## copy-direction-follows-the-paths

Which of the four copy functions applies follows from the two paths, not from the SyncItem:
copy_dest_to_source() passes them the other way round, so deciding by ssh_source()/ssh_target()
picked the opposite direction for every copy back from the target - e.g. copy_fs_to_ssh() with
the remote path as its local source, which can only fail. get_account_from_path() is the same
classification SyncItem uses when it builds its paths.

## copy_local_file

fs::copy_file copied in one call: nothing for the progress bar until the file was through, and
no way to stop inside a large file. The block loop reports the bytes after every block
(Synchronize.md#copied_bytes_) and checks the stop flag, so Cancel ends inside the file and
save_copy_file()'s Stopped path removes the partial target. The source's permission bits are
set afterwards as fs::copy_file did; a failure there is no failed copy, and the file time
follows separately in copy_source_to_dest().

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

fs::last_write_time() only resolves a local path, so the time is stamped here only when both
sides are local. Every transfer with an SSH side brings the modification time along itself (see
SyncFileSsh.md#transfer-keeps-the-modification-time).

## reported-once

A target that is newer than its source is left alone unless the item copies
newer targets or syncs both ways. Without a message the user cannot tell why
the file never arrives. Continuous sync meets the same file every cycle, so the
target is remembered and reported only once; it is forgotten as soon as the
file no longer differs this way, so a recurrence is reported again.

The same bookkeeping serves every difference that is left alone on purpose, one
ReportedOnce per kind of message.

## content-conflict

A two-way item whose files have the same time and size but different content
(see FileDifference.md#content-conflict) is not copied in either direction. It is
an error because the two sides stay different until the user decides which one
is right, and it goes to the sync report as well, so a backup proof shows it.
