# SyncDirTests

Module comment for SyncDirTests.h / SyncDirTests.cpp.

## old-file-remains

The old file should still be there; adjust if the desired behaviour changes.

## SubdirectoryInSourceIsNeitherCopiedNorTreatedAsMissing

Fehler 2 (round 7): sync_dir_entry() passed subdirectories through to FileDifference unfiltered
- fs::file_size() fails for a directory, which was misread as "source file not found." Harmless
with delete_in_target off (confirmed here: the subdirectory is neither copied nor deleted), but
with delete_in_target on it could delete an unrelated same-named *file* in the target. Verifies
the subdirectory itself is left alone either way.

## same-named-target-file

Pre-existing target file that happens to share the subdirectory's name - this must survive even
with delete_in_target enabled.

## DeleteInTargetRemovesFileMissingFromSource

Fehler 1 (round 7): SyncDir/SyncTree only ever enumerated the source side, so a file removed
from the source was never noticed - delete_in_target ("Gelöschte Dateien im Ziel ebenfalls
löschen") had no code path that could ever fire for Dir/Tree items, even though the dialog
explicitly enables the checkbox for exactly these types. Verifies a target-only file (no
counterpart in source) is now deleted when the option is enabled.

## TargetOnlyFileIsUntouchedWithoutDeleteInTargetOrTwoWaySync

Without delete_in_target (and without two_way_sync), a target-only file must be left alone
entirely - this is the existing, still-correct default.

## DeleteInTargetSparesWildcardExcludedFiles

A wildcard-excluded target file must survive delete_in_target: it was never "managed" by this
sync item in either direction, matching how the source-to-target pass already treats
non-matching files as invisible.

## TwoWaySyncCopiesTargetOnlyFileBackToSource

two_way_sync must copy a target-only file *back* to source instead of deleting it - the two
options are mutually exclusive in effect per file (deleteTarget is checked first in SyncFile's
decision logic).

## ExcludedFileIsNotCopied

Exclusions feature: a file matching an exclusion pattern must never be synced, even though it
matches an empty (match-everything) wildcard.

## DeleteInTargetSparesExcludedFiles

An excluded file must also survive delete_in_target: it is not managed by this sync item in
either direction, so a pre-existing target-side copy (from before the exclusion was added, say)
must not get deleted either.

## SyncEmptyFilesCopiesEmptyFilesInADirectory

The flag has to take effect at directory level just the same: SyncDir hands every file to
SyncFile one by one, and the empty ones must not get lost as a special case there - while the
non-empty ones have to keep being copied regardless.

## EmptyFilesAreSkippedInADirectoryByDefault

Without the flag the empty file stays behind and the one with content is copied.

## DeleteInTargetKeepsTargetWhenSourceDirIsGone

The worst case for this option: the source is not "empty", it is *gone* (drive unplugged,
network share not mounted, directory removed). Every target file then looks like an orphan, so a
naive target-only pass would wipe the entire backup. Only the directory_iterator throwing before
sync_target_only_entries() is reached prevents that - assert it stays that way.

## SyncDirUnreachableSshSourceTest

Registers an SSH account whose host can never resolve (RFC 2606 reserves ".invalid"), so the
connection fails fast and deterministically without a real server - see the identical fixture in
FileDifferenceTests.cpp.

## DeleteInTargetKeepsTargetWhenSshSourceIsUnreachable

The SSH counterpart of the test above, and a nastier one: ssh_list_dir() returns an empty vector
on a connection or authentication failure, which is indistinguishable from "the remote directory
is empty" - so the target-only pass does run, over every file in the target. What saves the data
is the per-file check in FileDifference (a failed SSH read reports "drive gone", not
SourceNotExits). This test pins that second line of defence down at the SyncDir level, where the
mass deletion would actually happen.

## SyncCreatesTargetDirWhoseParentIsMissing

The target directory used to be created with create_directory(), so a target whose parent did
not exist yet - a fresh backup drive, a path one level deeper than anything present - failed on
every run and the item never synced.

## platform-separators

Built through fs::path so the separators are the platform's own.
