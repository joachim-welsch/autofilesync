# MountedDrivesTests

Module comment for MountedDrivesTests.cpp.

## concurrent-singleton

MountedDrives is a process-wide singleton called concurrently from the sync worker thread for
every file compared; repeatedly forcing time_elapsed() to stale via set_test_time() interleaves
refresh() (writer) with drive_mounted()/ check() (reader) calls from other threads, which races
the shared MountInfos vectors and check_time_ without the internal lock.

## FstabMountpointDecidesMountedState

Fehler 3 (round 5): for a path under an unplugged drive, get_mount_root() walks up past the
missing mountpoint to "/" - which is always mounted - so drive_mounted() reported true and a
missing source read as "file deleted" instead of "drive gone", which delete_in_target acts on.
Paths under a known fstab mountpoint are now judged by whether that mountpoint is actually
mounted right now.

## drive-mounted-cases

An fstab mountpoint that /proc/mounts does not list is not mounted; one that really is mounted
counts as mounted; a path under no fstab mountpoint keeps the previous behaviour.

## root-mount-point

The expected mount point must not depend on the machine's layout: since Ubuntu 25.10 /tmp
(and thus the scratch dir) is its own tmpfs. A missing directory directly under / makes
get_mount_root() walk up to /, which is a mount point on every Unix system.

## FakeFstabTest

A fstab of the test's own, pointed to with set_fstab_path_for_test() and put back to /etc/fstab
in TearDown. "/mnt/afx_unplugged" is the drive that is listed but not mounted - the unplugged
USB disk of the field reports. Linux only: the fstab lookup exists only there.

## FstabPrefixEndsAtAPathComponent

fstab_mountpoint_for() compares path prefixes; "/mnt/afx_unplugged2" starts with the mountpoint's
characters but is a sibling, and must fall through to the mount table.

## DeepestFstabMountpointDecides

Two nested entries: the deeper one wins, so a mounted parent cannot vouch for an unmounted
child mountpoint. /proc and the invented /proc/sys serve as the pair because /proc is mounted
on every Linux box the tests run on.

## CommentsRootAndRelativeEntriesAreIgnored

A commented line, the root entry and a relative mountpoint are no mountpoints for the lookup;
with nothing applicable the old mount-root behaviour applies, which says mounted for anything
below /.

## MountTableAnswerIsCachedAndRefreshed

refresh() reads /proc/mounts at most every ten seconds and check() answers from the copy in
between; set_test_time() steers which of the two answers, and both must say the same.

## FileDifferenceTellsAnUnmountedDriveFromAMissingFile

The result that everything downstream relies on: an unmounted drive is DriveNotMounted, never
NotExists, on either side - and a plain missing file on a mounted drive stays NotExists.

## UnpluggedSourceDriveNeverDeletesTheTarget

Fehler 3 end to end: delete_in_target with the source drive unplugged must leave the target
alone, because SyncFile::decide() only deletes for SourceNotExits.
