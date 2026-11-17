# UnpluggedDriveTests

Module comment for UnpluggedDriveTests.cpp. The lookup itself is tested with the module
(MountedDrivesTests.md in Utilities/test/doc).

## UnpluggedDriveTest

A fstab of the test's own (ScopedFstab.md#ScopedFstab) that lists "/mnt/afx_unplugged" without
it being mounted, and a file synchronisation from or to that drive. Linux only: the fstab lookup
exists only there.

## FileDifferenceTellsAnUnmountedDriveFromAMissingFile

The result that everything downstream relies on: an unmounted drive is DriveNotMounted, never
NotExists, on either side - and a plain missing file on a mounted drive stays NotExists.

## UnpluggedSourceDriveNeverDeletesTheTarget

Fehler 3 end to end: delete_in_target with the source drive unplugged must leave the target
alone, because SyncFile::decide() only deletes for SourceNotExits.

## empty_mountpoint

What an unplugged drive usually leaves behind: its mount point as an empty directory on the
disk above. The directory exists, so every check that only asks whether the path is there
takes it for the drive.

## UnmountedDriveIsReportedEvenBelowAMissingDirectory

Below an unmounted drive every directory is missing as well. DriveNotMounted has to win over
DirectoryNotExist, because a missing directory counts as deleted
(FileDifference.md#absent) and an unplugged drive must never do.

## TargetOnAnUnmountedDriveIsNotReachable

CheckPath lets a target pass that does not exist yet (CheckPath.md#path_exists). That must
not reach an unmounted drive, neither its mount point nor a directory to be created below it.

## TreeIntoAnUnmountedDriveCreatesNoDirectoriesThere

The copy of a file already refuses an unmounted drive; the directories SyncTree creates on its
way have to do the same, or the tree of the source grows on the disk the mount point lies on
(SyncBase.md#create_target_dir_if_not_exist).

## EmptyMountpointOfAnUnpluggedSourceDeletesNothingInTheTarget

The empty mount point lists as an empty source. With delete_in_target every file of the target
would be superfluous, were it not for the drive check behind each of them.

## TwoWayWithAnUnpluggedSideDeletesNothingOnTheOther

The same with a state that knows the files: both sides were in sync, then the drive is pulled,
once as source and once as target. To the state the files of the drive are gone, which reads
as deleted, and the other side would follow. It must stay as it is, and nothing may be written
to the mount point.
