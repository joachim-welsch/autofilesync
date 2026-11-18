# ReadOnlyFilesystemTests

Module comment for ReadOnlyFilesystemTests.cpp.

## ReadOnly

A file system cannot be mounted read-only for a test without root. So the tests tell
FilesystemUtil which directory tree counts as read-only (FilesystemUtil.md#read_only_filesystem)
for as long as the object lives. Only that answer changes: the files below can still be
written and deleted, and that is what makes the tests sharp - a pass that tried anyway would
succeed and leave its traces. What the system answers to a real write is covered by the
mapping of its error code (FilesystemUtilTests.md#AReadOnlyFilesystemIsAReasonOfItsOwn) and
by the injected file-time error below.

## ReadOnlyFilesystemTest

A directory pair of its own, a Dir item compared by time, and a recycle bin of its own so
that a deletion that should not happen cannot reach the user's bin. TearDown takes every
override back even when a test failed inside its scope.

## pass

One pass of the real Synchronize over the configuration, because the check stands there
(Synchronize.md#write-protected). The answer is the last run the configuration recorded:
the number of errors and the first of them.

## AReadOnlyTargetIsOneErrorAndNothingIsTried

Three files for a read-only target used to be three failed copies. Now it is one error that
names the target, and the empty target directory proves that no copy was attempted.

## ATargetThatIsWritableAgainIsSynchronized

Nothing sticks: the pass after the protection is gone copies everything.

## ATwoWayItemWithAReadOnlySourceIsOneErrorAndNothingIsTried

Two-way writes to the source as well. The file that exists only in the target must not be
copied back, and the target must not receive the source's files either - the item is left
alone as a whole, since half a two-way pass would record a state that is not true.

## AReadOnlySourceOfAPlainCopyIsNoError

The common case of a read-only source - a CD, a camera card - must simply work when nothing
is to be written there.

## AMoveFromAReadOnlySourceCopiesAndSaysOnceThatTheSourceStays

Two passes. Each copies or finds everything in the target, deletes nothing, puts nothing in
the bin and reports one error - not one per file, which the second pass used to repeat when
it tried to finish the move (SyncFile.md#move-catch-up).

## AMoveIsFinishedOnceTheSourceIsWritable

The move is not given up, only postponed: with the protection gone the next pass removes
the source files that are already in the target.

## preview

One preview pass of the real Synchronize; the plan and the notes are read from the instance
before it goes, the way the tests of the plan do it (SyncPlanTests.md).

## ThePreviewOfAReadOnlyTargetPlansNothingAndSaysWhy

The preview used to list three copies the run would then refuse. It now plans nothing for
the item and carries the reason as a note (Synchronize.md#preview-of-a-protected-item); it
records no run.

## ThePreviewOfAMoveFromAReadOnlySourcePlansCopies

The same item is three moves while the source is writable and three copies while it is not,
with the note that the source keeps its files - the preview says what the run will do.

## AWriteRefusedInTheMiddleOfAPassAbandonsTheItemWithOneError

A drive that turns read-only during the pass is not caught by the check before it. The file
time refuses with the system's error code; the first file's error carries the reason and the
note about the skipped rest, the other two files are not tried, and once the report is gone
nothing is stopping any more (CopyLog.md#abandon).

## AFileTimeRefusedByAReadOnlyFilesystemIsNotTriedAgain

The copy back into the source of a two-way item sets the file time with retries
(SyncFile.md#bounded-retries). Ten of them are a second, so the time the pass takes tells
whether the read-only answer ended them.

## ADeletionOnAReadOnlyFilesystemNamesTheReasonAndCopiesNothingIntoTheBin

"Delete in target" with the recycle bin: the first deletion is refused with the reason
(RecycleBin.md#read-only-source), the second file is not tried, both are still there and the
bin was never created.

## AReadOnlyRefusalAbandonsTheReportOnce

The report by itself: another reason abandons nothing, the first read-only refusal is kept
with its note, the second adds nothing, and without a report the call is harmless.

## AReadOnlyDriveWithoutRoomIsCalledReadOnlyAndNotFull

A read-only drive that also has no room - every CD and image. The sync classes run without
the check before the pass: the first file does not fit, and the error is the write
protection with the note about the skipped rest, not a shortage
(SyncFile.md#no-room-on-a-read-only-drive).

## AHintOfAPassIsAWarningNoteAndNoError

One pass of the real Synchronize in which no file time can be stored: the last run has no
error, and the pass leaves one warning note with the number of files (CopyLog.md#hints).

## EveryReasonHasAReachAndTheItemWideOnesAbandon

The table of reaches by itself (CopyLog.md#reach): each reason that holds for the whole side
abandons the report at its first refusal and keeps one error, each that concerns one file
keeps both errors and abandons nothing, and shortages and hints are what they are called.
