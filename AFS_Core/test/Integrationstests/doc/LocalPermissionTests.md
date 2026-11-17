# LocalPermissionTests

Module comment for LocalPermissionTests.cpp.

"Permission denied" on the local filesystem had no test; the SSH side has
FailedSshCopyRestoresBackupAndReportsFailure. Every case here asks the same two questions: is
the old target still intact, and does the run report the failure - a silent skip would let the
proof file claim a successful run. Unix only, and skipped as root, who is not refused anything.

## LocalPermissionTest

A directory pair of its own; the locks are plain chmod. TearDown gives everything owner_all
back before removing it, because remove_all() cannot delete inside a directory the test
locked.

## unlock-before-removing

The locked directory may be the root, the target or the extra "gesperrt" parent, and the
locked file may be unreadable: unlock the known directories first, then whatever the
iterator can now reach.

## run

The errors of a run reach the proof through the SyncReport that SyncReportGuard installs; the
tests install one of their own and read it, like Synchronize::report_sync_item() does.

## ReadOnlyTargetFileIsStillReplaced

The positive case that explains pre_copy(): a read-only target file is the sync's own business
and gets owner permissions before it is replaced, so no error.

## UnwritableTargetDirectoryKeepsTheOldTargetAndReportsTheError

The target directory refuses the rename to ".bak" that save_copy_file() does first. The old
file is untouched, which is right - but the item must still count as failed.

## NewFileInAnUnwritableTargetDirectoryIsReported

Without an existing target the copy itself fails at the open; the message names the target
so that the user knows which directory to fix.

## UnreadableSourceKeepsTheOldTargetAndReportsTheError

The copy fails at the first read; restore_backup() has to put the renamed old target back.

## UncreatableTargetDirectoryIsReportedByName

create_target_dir_if_not_exist() cannot create the target below a locked parent; the first
error names the target directory, the file-level errors that follow are noise behind it.

## UnreadableSourceDirectoryIsReportedAndCopiesNothing

The directory iterator throws before any file is seen; SyncDir catches it once for the whole
item.

## locked-source-is-no-deletion

A directory that may be listed but not entered (r--) gives the names of its files and refuses
everything else about them; without any permission a File item cannot even ask for its one
file. Either way the file is there. With "delete in target" the pass used to take it for gone
and deleted its copy in the target, the Dir pass without a single error. The tests ask for
the target to stay, for the run to count as failed, and - after the directory is unlocked -
for the next pass to do what it would have done: copy the newer file and delete what is
superfluous in the target. Deletions are permanent here, so a failing test leaves nothing in
a recycle bin.

## FileBehindALockedTargetDirectoryIsReportedAndNothingIsCopied

The mirror image for the target: the pass cannot tell whether the target file is there, so it
does not copy over it blindly, and the one error names the file it could not ask for. The
directory is unlocked before the content is read back.

## TwoWayWithALockedDirectoryDeletesAndCopiesNothing

With a state the stakes are higher: a file the state knows and that is "gone" on one side was
deleted there, and the other side follows. The locked side is the source or the target, once
without any permission and once listable only. Nothing may be deleted or copied, the run
counts as failed, and the state still knows the file, so that the pass after unlocking finds
everything in sync instead of taking the file for new. The File item is the same case with
the directory of its one file locked.

## names

The error has to name the file that would not open and say why. "Copy failed" alone sends the
user looking at the wrong side.

## ContentComparisonWithAFileThatDoesNotOpenChangesNothing

Two identical files, compared by content, and one of them does not open: without read
permission here, which is how a file locked by another program behaves on Windows
(LockedFileTests.md). Opening it used to fail silently and read as "different". One-way that
meant a copy attempt per pass with an error that named no reason, or the target overwritten;
two-way with equal times it was a content conflict, which renamed the target file to a
conflict copy and carried that copy to the other side. All eight combinations of direction,
closed side and times must leave both directories with their one file, report one error per
pass that names the closed file, and find everything equal once it opens again.

## SourceThatDoesNotOpenIsNamedAndTheTargetIsNotTouched

The source is newer and does not open. The copy used to find that out last, after the target
had been renamed to ".bak" and opened for writing, and then put everything back. Here the
target directory is locked too, so the old order would fail at the rename and blame the
target; the error has to name the source, which proves the source was tried first. The
two-way test is the same for a copy back from the target.

## ConflictWhoseWinnerDoesNotOpenRenamesNothing

Both sides were edited since the last pass, so the newer one wins and the older one is renamed
to a conflict copy first (SyncFile.md#conflict-copies). When the winner does not open, that
rename would leave the losing side without the file under its name and with nothing to
replace it. Nothing may be renamed, and once the winner opens the conflict is settled as
usual: the winner over, the conflict copy back.

## write_source_tree

Four files on three levels: enough for a failure at the top to multiply, if the pass goes on
below it.

## UncreatableTargetDirectoryIsOneErrorWithItsReason

The target directory cannot be created. The pass used to go on regardless: it tried every
subdirectory and every file below and reported each of them, eight errors for these four files
and as many as the tree has entries in general, with the reason only in the log. One cause is
one error, and it says why.

## FileInThePlaceOfATargetDirectorySkipsOnlyThatSubtree

A plain file bears the name of a subdirectory of the target. Only that subtree is given up:
its one error names the path, the file in the way is neither replaced nor - with "delete in
target" - deleted, and the rest of the tree is copied.

## UncreatableSourceDirectoryOfATwoWayItemIsOneErrorAndKeepsTheTarget

The mirror image for a directory that is new in the target and has to be created in the
source, which is write-protected. One error for the directory, its files stay in the target,
and the file beside it is synchronised as usual.

## UncreatableParentOfAFileCopiedBackIsOneErrorWithItsReason

A directory was deleted in the target, but one of its files was edited in the source since, so
the file is kept and copied back, for which its directory has to be created again
(SyncFile.md#parent-of-a-kept-file). When that fails the copy used to be tried anyway and
reported as a bare "copy failed"; the failed directory was only in the log. The edited file
must stay in the source.

## FileThatCannotBeWrittenNamesItsReason

The copy cannot open its target for writing. The error names the target, as
NewFileInAnUnwritableTargetDirectoryIsReported asks, and now also the reason.

## TwoWayDirectoryThatCannotBeListedIsOneError

A two-way pass lists both sides of a directory. When either listing fails the directory is
given up with one error that names it and the reason; a target that could not be listed used
to pass for an empty one, silently.

## SourceThatDoesNotOpenLeavesTheGenerationsAlone

With the generation principle the old target is moved to its backup name before the copy.
When the source cannot be read that move must not happen: the target would be left without
the file under its name, and every failed pass would push the generations on by one.
