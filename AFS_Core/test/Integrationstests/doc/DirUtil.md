# DirUtil

Module comment for DirUtil.h / DirUtil.cpp.

## erase_ssh_test_dir

Removes files under an "account:/remote/path" tree via SFTP. Needed wherever a test writes
through the SSH test account: that account can own files a plain fs::remove_all() (running as
the developer's own user) has no permission to touch, which would otherwise leave a previous
run's files in place and make the next run see stale state.

## erase_ssh_test_dir_variants

Same reasoning as erase_ssh_test_dir(), but for fixtures whose tests each derive their own
destination by appending a suffix (e.g. "..._many", "..._mixed") to a shared base name: clears
every named variant in one call. Takes the explicit suffix list rather than listing the parent
directory over SFTP - that parent is the shared system temp dir, and an unrelated entry there
with an unconvertible name would make the listing itself throw.
