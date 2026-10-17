# SyncSshToSshTests

Module comment for SyncSshToSshTests.cpp.

Source and target both behind SSH (SSHPATH::both, copy_ssh_to_ssh) had no test: every other SSH
suite has one local side. The local test server serves both sides at once, so no second server
is needed.

## SyncSshToSshTest

Both sides are prepared and inspected through the local filesystem and only the code under
test goes through SFTP: the server is localhost, so the SSH user's files are the same files.
A directory per test below the run's temp directory, so that two tests never share a remote
path.

## both-sides-on-disk

The directories are created by the test process and opened up for everyone, because the SSH
user has to create files in the target and read them in the source; files it creates in a
directory the test process owns can still be removed by the test process.

## clean-up-as-the-ssh-user

Directories the SSH user created in the target (ssh_mkdir, mode 755) belong to it, so the test
process cannot remove what lies inside them; the target tree is removed through SFTP as that
user first, then the suite directory locally - TestMain reports every leftover as a failure.

## compare_by_time

FileDifference silently replaces the content criterion with size and time for SSH paths; the
tests say time explicitly so that a repeated run really has to see the kept modification time.

## CopyKeepsContentTimeAndPermissions

The transfer alone: content, the modification time in whole seconds (SFTP has no finer) and
the permission bits, which keep_remote_permissions() takes over - owner write stays on, which
the source already has here.

## FileItemBetweenTwoSshPathsIsCopiedOnce

Through SyncFile, where SSHPATH::both switches off the local file-time handling: the second
run must find both sides equal, otherwise the time was not kept.

## DirItemCopiesEveryFileAndDeletesTargetOnlyFiles

SyncDir lists the SSH source, copies file by file and runs the target-only pass over the SSH
target, where the deletion goes through ssh_remove_file().

## TreeItemCreatesTheNestedDirectories

SyncTree creates every target directory through ssh_mkdir() before it descends.

## TwoWaySyncCopiesTargetOnlyFilesBack

The copy back is copy_ssh_to_ssh() with the roles swapped; afterwards both sides agree.

## StopDuringTheCopyLeavesNoHalfFileInTheTarget

The counterpart of SyncSshStopTest for two SSH sides: restore_backup() removes the partial
remote file through SFTP (SyncFile.md#restore_backup).
