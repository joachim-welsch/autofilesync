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
