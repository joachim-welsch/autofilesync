# SyncSshTreeTests

Module comment for SyncSshTreeTests.h / SyncSshTreeTests.cpp.

## remote-cleanup-once-per-process

Remove remote target directories left over from earlier runs: they belong to the SSH test
account, so the local fs::remove_all() above cannot touch them - and a target left standing
would pass as "already synchronised". Every call connects over SSH anew, hence once per process
instead of before every single test.

## ssh-cleanup-needed

Cleanup: remove the local test directory. DEST_TEST_DIR belongs to the SSH test account (it was
synced there over SSH) and therefore has to be deleted over SFTP; a local fs::remove_all() would
have no access to it.

## reverse-cleanup-path

Cleanup: remove the local test directory. The remote directory written over SSH was
DEST_TEST_DIR + "_reverse" (see remoteDestPath above); it belongs to the SSH test account and
therefore has to be deleted over SFTP - a local fs::remove_all() would have had neither the
access nor the right path.
