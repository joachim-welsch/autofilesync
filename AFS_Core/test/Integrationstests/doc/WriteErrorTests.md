# WriteErrorTests

Module comment for WriteErrorTests.cpp.

What the report says when something could not be written for a reason the system does not
name more closely: the file time after a copy, and the rename that parks the old target. Both
used to end as "error reading the file information" or as nothing at all.

## WriteTimeError

A file the user owns never refuses a new time, and the cases that do - a foreign owner, a
file system without times - need root or another drive. So the tests tell FilesystemUtil which
error to answer with (FilesystemUtil.md#write_time_refusal): for as long as the object lives no
file time can be set.

## RightsError

The same for the permission bits (FilesystemUtil.md#permissions_refusal).

## WriteErrorTest

A directory pair of its own, items compared by time. TearDown takes the error back even when
a test failed inside a WriteTimeError scope.

## FileTimeThatCannotBeSetIsAnErrorOfTheReport

The content arrives, so the copy counts and no ".bak" is left, but the run is not clean: the
target now bears the time of the copy instead of the source's (SyncFile.md#file-time-failure).
The error names the target and is a write error, not a read error.

## FileTimeErrorNamesTheReasonTheSystemGave

A refusal for a missing right is reported as that, not as the general write error.

## FileTimeThatCannotBeSetOnTheCopyBackNamesTheSourceFile

The other direction goes through the retrying set_last_write_time() of SyncFile.cpp, which
takes about a second to give up; the file whose time is wrong is then the source.

## PassAfterwardsSetsTheFileTimeWithoutCopyingAgain

The next pass finds a target that is newer than its source and has the same content, and gives
it the source's time (SyncFile.md#restamp). No state is involved: a plain one-way item has none.

## FileTimeThatStaysRefusedIsReportedInEveryPass

As long as the time cannot be set the target is not what it should be, and every pass says so.

## TwoWayPassAfterwardsSetsTheTargetTimeInsteadOfCopyingBack

With two-way sync the newer time of the target would otherwise pass for a change and travel
back to the source. The state remembers the time the target was meant to get, so the pass
after sets it; the third pass shows that the pair has settled.

## TwoWayPassAfterAFailedCopyBackSetsTheSourceTime

The same for the other direction, where the source is the file with the wrong time.

## NewerTargetWithTheSameContentGetsTheSourceTime

The rule does not ask how the times came apart: a target that was merely touched is brought
back in line as well.

## NewerTargetWithOtherContentOfTheSameSizeKeepsItsTime

The boundary: the same size is not the same content, and an edited target is left as it is.

## PreviewNeitherCountsNorTouchesAFileThatOnlyLacksItsTime

Two-way, because there the preview used to announce a copy back for the newer target.

## CopyWhoseFileTimeIsSetReportsNoError

The counterpart without the error, so that the report does not complain about every copy.

## RightsThatCannotBeSetAreAnErrorOfTheReport

The copy counts and bears the source's time, but its rights are not the source's, and the
report says so with the target's name (SyncFile.md#attribute-failure).

## RightsErrorNamesTheReasonTheSystemGave

The error says what failed and, in brackets, why - "permission denied" alone would not tell
that the content did arrive.

## FileSystemThatKeepsNoRightsIsNoError

"Not supported" is the answer of a file system without permission bits. Nothing was lost
there, and an error for every file would make each backup onto such a drive fail.

## RightsThatCannotBeSetOnTheCopyBackNameTheSourceFile

The other direction, where the source is the file that was written.

## CopyCarriesTheRightsOfTheSourceAndReportsNoError

The counterpart without the error, and the proof that the rights still travel now that
copy_local_file() no longer sets them itself. Unix only: Windows knows the read-only bit alone.

## RenameRefusedForAnotherReasonIsAWriteErrorNotAReadError

A target whose name is as long as the file system allows cannot be parked as "<name>.bak":
the rename fails with "file name too long", which is none of the reasons the program knows.
The length is asked of the file system, since not every one allows 255 bytes. Unix only:
Windows answers a name of that length differently.
