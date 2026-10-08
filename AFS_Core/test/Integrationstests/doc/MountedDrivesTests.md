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
