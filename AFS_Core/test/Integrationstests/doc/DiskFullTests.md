# DiskFullTests

Module comment for DiskFullTests.cpp.

## FreeSpace

A disk cannot be filled for a test without root, and filling the real one would endanger
everything else on the machine. So the tests tell FilesystemUtil how much room there is
(FilesystemUtil.md#free_space): for as long as the object lives every drive has that many
bytes left. What the system answers when a write runs out of room in the middle is covered by
the mapping of its error code (FilesystemUtilTests.md#ReasonForFailureTellsLockedFromRefusedFromMissing)
and, for the restoring of the old target, by the cut-off writes of CopyFailureTests.md.

## DiskFullTest

A directory pair of its own, items compared by time. TearDown gives the real free space back
even when a test failed inside a FreeSpace scope.

## run

Fills the outcome it is handed instead of returning one: the report is read after the guard
is gone, and a returned copy could be taken before that.

## FileThatDoesNotFitIsReportedAndTheOldTargetIsNotTouched

Three mebibytes for a disk with one. The copy used to find out while writing: the old target
was renamed to ".bak", a mebibyte written, the half file removed and the old target put back.
Now the room is asked for first. The real disk has room, so a copy that goes through anyway
would show at once; the write time proves the old target was never moved.

## FileThatJustFitsIsCopied

The boundary: exactly as many bytes as are free is enough.

## DirectoryPassCopiesWhatFitsAndSumsUpTheRestInOneError

Three large files and a small one. The small one is copied - a full disk is no reason to stop
the pass, since what still fits should arrive - and the three that do not fit are one error,
not three: on a full backup disk the list would otherwise be as long as the directory.

## PassAfterSpaceWasMadeCopiesEverything

Nothing sticks: the pass after the user made room copies what the last one could not.

## TwoWayCopyBackThatDoesNotFitKeepsTheSource

The same for the other direction, where the full disk is the source's. The error names the
file on that side, and the state must not record the pair as in sync.

## DeletionsStillHappenOnAFullDisk

"Delete in target" frees room and needs none. A pass that stopped at the first file that does
not fit would never get to the deletions that could make it fit next time.

## ADeletionTheRecycleBinHasNoRoomForNamesTheBin

"Delete in target" with a recycle bin on another file system that is said to have three
bytes. The file stays, nothing is copied into the bin, and the error names the bin's lack
of room next to the file - "disk full" alone would point at the drive the user wants to make
room on (RecycleBin.md#no-room). Skipped where /dev/shm is not another file system.

## SyncReportShortage

The report keeps the first shortage as an ordinary error with the file's name and the room it
needed. Every further one rewrites that entry into a summary - how many files, how much they
need together, the first of them - and adds nothing, so the place among the other errors and
their number stay as they are. Beyond the error limit a shortage adds nothing either, and the
run still counts as failed.
