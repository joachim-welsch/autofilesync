# ScopedFstab

Module comment for ScopedFstab.h.

## ScopedFstab

Points MountedDrives at a fstab file of the test's own and back to /etc/fstab when it goes out
of scope, so that a failing test cannot leave the singleton looking at a file that is deleted
with the test directory. content() writes the file and makes it the one in use; before the first
call the real fstab stays in place. Linux only, like MountedDrives::set_fstab_path_for_test().
