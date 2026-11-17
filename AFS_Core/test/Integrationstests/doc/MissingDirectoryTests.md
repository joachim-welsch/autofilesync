# MissingDirectoryTests

Module comment for MissingDirectoryTests.cpp.

## MissingDirectoryTest

What a pass does when a directory it needs is not there. The tests go through Synchronize, as
SyncStateRunTests do (SyncStateRunTests.md#SyncStateRunTest), because the decisions are spread
over the reachability check (CheckPath.md#path_exists), the state next to the configuration
(Synchronize.md#vanished-target) and the sync classes, and only the whole pass shows whether
they fit together. Unmounted drives have their own tests (UnpluggedDriveTests.md). Three files
per side stay below the mass deletion limit (SyncBase.md#mass-deletion) on purpose: the tests
must not pass only because that limit caught the deletions.

## run_pass

One pass over the configured item. It waits for the pass to finish rather than for last_sync
to change, because an unreachable item leaves last_sync alone. The LastRun of the item tells
what the pass thought of the two sides.

## MissingTargetDirectoryIsCreatedAndFilled

A new synchronisation whose target folder does not exist yet has to work at once, without the
user creating the folder by hand.

## TargetBelowAMissingDirectoryIsUnreachableAndCreatesNothing

More than the target directory itself is missing: this is how a drive looks that was pulled
and that no fstab knows. Creating the whole chain would put the backup on the wrong disk.

## MissingSourceDirectoryDeletesNothingInTheTarget

With "delete in target" a source that lists as empty would empty the target. A missing source
is not an empty one.

## vanished-target

The target of a two-way item was in sync and is gone as a whole. To the state every file was
deleted in the target, and following that would delete the source too. The pass has to take
the target for new instead and fill it again; the state it writes afterwards has to work like
any other. For a File item the directory of the target file plays the part of the root.

## remote-target

The same behind SSH, where the reachability check never asks for the target directory and
the pass would create it empty. Needs the SSH test server (SshTest.md#SKIP_WITHOUT_SSH_SERVER).
The remote directory is removed before the assertions, so a failure leaves nothing behind.

## TwoWayKeepsTheTargetWhenTheSourceDirectoryVanished

The mirror image. The source must exist, so the item is unreachable and nothing happens; the
state keeps its files for the day the source is back.

## TwoWayDirectoryDeletedBelowTheRootIsStillDeletedOnTheOtherSide

The protection of the root must not reach further down: a directory deleted inside the target
is a deletion and travels to the source (TwoWaySyncTests.md#deleted-directories). The same
goes for the target file of a File item that is missing from a directory that is there.

## TargetThatIsAFileIsAnErrorAndStaysUntouched

A plain file bears the name of the target directory. The pass has to report it and must
neither replace the file nor touch the source.

## UnreadableSourceDirectoryDeletesNothingInTheTarget

A directory without read permission cannot be listed. Its files are still there, so neither
"delete in target" nor the two-way state may take them for deleted. The permission is given
back before the assertions, so that a failing test can still be cleaned up.
