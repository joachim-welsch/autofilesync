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

## drive-without-medium

refresh() asks every logical drive for its volume name, and a card reader without a card or an
empty optical drive answers with an error. That is the normal state of such a drive, not a
failure of the synchronisation, so it is a debug line; as an error it turned up in the GUI's
message box after every single run.

## requested-drive-only

Only the drive asked for gets its volume name looked up. GetVolumeInformation on a network
drive wakes the server behind it - a NAS in standby answers after ten to twenty seconds - and
the name of a drive the synchronisation does not touch is never compared with anything, so
asking every drive stalled every local run for as long as the sleeping NAS took. Found by
GuiTest.StartUndStoppLaufenMehrerePassesUndStoppenSauber.
