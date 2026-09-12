# MountedDrives

Module comment for MountedDrives.h / MountedDrives.cpp.

## mutex_

Guards all three members above: drive_mounted() is called from the sync worker thread for every
file compared, and this is a process-wide singleton, so any other caller (present or future)
races it without this lock. Private helpers below assume the caller already holds it.

## fstab_mountpoint_for

Deepest fstab mountpoint (other than "/") that is a path prefix of 'path'.

## macos-refresh

macOS has no /proc; the BSD getmntinfo() syscall wrapper returns the currently mounted
filesystems directly from the kernel instead.
