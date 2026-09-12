# SyncSshDirTests

Module comment for SyncSshDirTests.h / SyncSshDirTests.cpp.

## remote-cleanup-once-per-process

Remove remote target directories left over from earlier runs: they belong to the SSH test
account, so the local fs::remove_all() above cannot touch them - and a target left standing
would pass as "already synchronised" and wrongly leave copyCount at 0. Every call connects over
SSH anew, hence once per process instead of before every single test.

## reset-synchronize

Reset Synchronize: without this, stop_ stays set from an earlier test and SyncFile::operator()()
silently does nothing.

## sync-dir-copies-no-subdirs

SyncDir copies only the files in the given directory, never its subdirectories.

## copy-count-regression

Regression: SyncFile::copy_source_to_dest() called fs::last_write_time() on the SSH source path
unconditionally, which threw and returned before ++copy_count_ for every single file - the copy
itself still succeeded (the assertions above passed even while broken), so only copyCount
catches it. This is also what the GUI's progress bar is driven by, so a stuck copyCount showed
up there as a progress bar that never moved. Not pinned to an exact count: DEST_TEST_DIR is
shared with SyncLocalDirToRemote above and only cleaned once per process, so how many files end
up here depends on test order.

## ssh-cleanup-needed

Cleanup: remove the local test directory. DEST_TEST_DIR belongs to the SSH test account (it was
synced there over SSH) and therefore has to be deleted over SFTP; a local fs::remove_all() would
have no access to it.
