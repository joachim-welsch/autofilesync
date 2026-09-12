# SyncFileTests

Module comment for SyncFileTests.h / SyncFileTests.cpp.

## target-dir-must-exist

Missing is the target *file*, not its directory: SyncFile copies into an existing directory, so
without this the test only passed on a scratch area some earlier run had already created.

## source-must-be-larger

Source must be the larger file: FileDifference only maps a size difference to SourceModified
(which copyToTarget acts on without extra flags) when the source is bigger - a bigger
destination means DestModified instead, which needs copy_when_target_newer() to copy.

## CheckOnlyCountDoesNotCopy

Regression test: the "count files" pre-pass (onlyCount=true) must behave like
SyncBase::sync_dir_entry() already does for Dir/Tree items - increment the counter without
touching the filesystem. SyncFile::operator()() used to ignore only_count_ entirely and perform
the real copy immediately, so a single-file SyncItem's "count" pass wasn't a dry run at all.

## FailedSshCopyRestoresBackupAndReportsFailure

Regression test (Fehler 1, round 4): save_copy_file() used to "return true" for ANY SSH copy
result before evaluating it, so a failed or aborted SSH transfer was reported as success - the
pre-copy .bak rename was never rolled back (destination replaced by nothing/a truncated file)
and callers went on to e.g. delete the source (del_aftercopy). Only the content verification may
be skipped for SSH, not the error handling.

## empty-host-fails-locally

Account with an empty host: the connect fails immediately and locally, keeping the test
deterministic without network or DNS.

## own-block-for-windows

A block of its own: Windows refuses to delete a file that is still open while POSIX simply
unlinks it - without the close() the fs::remove() below failed with "is being used by another
process".

## set_last_write_time

Reaches the file-local helper in SyncFile.cpp; no header declares it. The default argument
(call_wait = true) lives at the definition, so it must be passed explicitly here.

## SetLastWriteTimeGivesUpOnPersistentError

Regression test (Fehler 3, round 4): set_last_write_time() retried in a "do { ... } while (ec)"
loop with no exit condition. A persistent error (target drive unplugged, file deleted) spun the
sync thread forever in 100ms sleeps; SyncControler::stop_sync() then busy-waited on it from the
GUI thread, freezing the application on shutdown. The retry loop must be bounded and give up on
persistent errors.

## CopyDestToSourceDoesNotDoubleWaitForFiletime

Regression test (Fehler 10, round 5): copy_dest_to_source() called both set_last_write_time()
(which already calls wait_for_updated_filetime() internally by default) AND
wait_for_updated_filetime() again explicitly right after - doubling the wait for every
two-way-sync back-copy (up to ~20s instead of ~10s in the worst case). copy_source_to_dest() has
no such issue - it uses the plain util::set_last_write_time(), which never waits.

## SyncExceptionWhatReturnsTheRealMessage

Fehler 8 (round 6): SyncException passed a hardcoded "Fehler" to its runtime_error base instead
of the real message, so every catch site logging ex.what() (there is no dedicated
catch(SyncException&) anywhere - it's always caught via catch(std::exception&) or catch(...))
silently dropped the actual diagnostic move_file_to_backup() had built (which file, which
underlying rename error).

## EmptyFileTest

Creates a source file of size 0 and cleans up both source and target.

## EmptySourceFileIsNotCopiedByDefault

The default: an empty source file is not copied. Size 0 is more often an aborted write than a
deliberately empty file.

## SyncEmptyFilesCopiesTheEmptySourceFile

With the flag it is copied - placeholder files (.gitkeep), lock files and marker files exist
precisely to be empty.

## EmptySourceDoesNotWipeANonEmptyTargetByDefault

The dangerous case: the source is empty, the target is not. Without the flag the target's
content is kept - that is the actual purpose of the default.

## SyncEmptyFilesReplacesANonEmptyTarget

With the flag the synchronisation mirrors the source, even when that means replacing existing
target content with an empty file. That is intended and pinned down here so the reach of the
flag is documented.

## OnlyCountDoesNotCopyTheEmptyFile

Counting mode touches nothing, not even with the flag switched on.
