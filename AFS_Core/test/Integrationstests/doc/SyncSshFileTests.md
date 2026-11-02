# SyncSshFileTests

Module comment for SyncSshFileTests.h / SyncSshFileTests.cpp.

## SyncSshFileInspectorTest

These tests pin down the calling convention of inspect_file() across every path form - with and
without a drive letter, with spaces, relative, a directory. They need the test server and skip
themselves where none is running.

## ssh-temp-path

concat_ssh_temp, not concat_temp: the SSH account cannot even traverse the per-user temp
directory macOS hands out, which answers "permission denied" instead of "no such file".

## account-name-must-be-localhost

The account is added to Config dynamically. Its name has to be "localhost", because that is what
the path uses as its prefix.

## shared-parent-permissions

test_source_dir() and test_dest_dir() share a parent directory ("/tmp/test"). It was just
created as this local user, but the remote side below creates "dest" underneath it through the
SSH test account - without g+w/o+w that account's ssh_mkdir() could not write to the parent.

## target-dir-must-exist

SyncFile copies a single target and, unlike SyncDir/SyncTree, does not create its directory
itself (see create_target_dir_if_not_exist()), so the target directory has to exist beforehand
here. ssh_mkdir() is not recursive, just like a plain mkdir, hence the parent directory first.

## inspect-path-without-prefix

inspect_file() expects the path without the account prefix - the way the cleanup line below
already builds it. This used to pass the full path, and the Windows branch then cut the prefix
off a second time itself.

## ssh-delete-needed

Delete over SSH, not locally: the remote file belongs to the SSH test account, and the local
std::filesystem::remove() (running as the developer's own user) has no access to it - see
erase_ssh_test_dir() in DirUtil.cpp.

## write-to-src-path-directly

Create the source file: SetUp() creates a file of the same name under LOCAL_TEST_DIR
($HOME/tmp/...), but src_path points into the OS temp directory - the two need not be identical,
so write to src_path directly here instead of relying on SetUp().

## no-dir-creation-in-copy

copy_ssh_to_fs() does not create the target directory itself - in the real sync flow
SyncBase::create_target_dir_if_not_exist() has already done that.

## src-under-local-test-dir

large_file_path lies under LOCAL_TEST_DIR ($HOME/tmp/...), not under the OS temp directory -
src_path has to point where the file was actually just created.

## SyncSshFileTestConfig

SSH test configuration. These values have to be adapted to the local test environment.

## local-test-dir

In the temp directory, not below the home directory: TearDown() removes the
whole directory again, and the tests that build their paths with concat_temp()
use the same one, so nothing of them outlives the fixture either.

## CopyRemoteFileToLocalKeepsThePermissions

The source gets permissions no umask produces for a new file (rwxr-xr--), so
the copy can only show them when they were taken over. Others may read: the
file is fetched through the SSH server as the SSH test user.

## SyncSshStopTest

The SSH counterpart of StopDuringCopyTest (SyncFileTests.md): a 64 MiB file is copied through
the local SSH server as the test user and stopped as soon as the target appears. The shared
directory lies in /tmp with permissions for everyone, because the SSH test user has to create
the upload target in it and read the download source from it; the test process itself cleans
it up, which works because it owns the directory.

## sync_and_stop_when

The target is checked through the local filesystem, not through SFTP: the server is localhost,
so the file the SSH user creates is the same file, and a local exists() is quick enough to be
polled every millisecond while an SFTP round trip is not.

## StopDuringTheUploadLeavesNoHalfFileOnTheRemote

upload() returns Stopped between two SFTP writes. SyncFile then has to remove the partial
remote file - a local remove_file() on the "account:/path" string does nothing there.

## StopDuringTheDownloadLeavesNoHalfFileLocally

download() returns Stopped between two SFTP reads; the half-written local file must be removed.

## SyncSshFlagTest

Reuses the shared directory and account of SyncSshStopTest for tests of the sync flags over SSH.

## MoveLeavesTheSshSourceInPlace

SyncFile::decide() and copy_source_to_dest() exclude an SSH source from "delete after copy"
because the source cannot be deleted through the local filesystem. That is a documented limit,
not an accident, so the test pins it: the copy happens, the remote source stays.

## FatherSonKeepsEveryGenerationOnTheSshTarget

The remote counterpart of SyncFlagTests' FatherSonNumbersEveryGeneration: pre_copy() used to
skip the father-son backup for every SSH item, so the option was silently ignored
(SyncFile.md#remote-father-son). The three source versions lie more than the default time
tolerance of two seconds apart (FileDifference.md#time-tolerance): the SFTP modification time
has whole seconds, and a version written within the tolerance would look unchanged. The generations are read through the local filesystem, the server being
localhost. A fourth pass copies nothing: the generations live only in the target and must not
count as a difference.

## SyncSshFileToDirTest

A File item whose target is a directory gets the source's file name appended
(SyncFile::transfer_filename); whether the target is a directory is decided in
Synchronize::run_sync_item - locally with is_directory(), over SSH with the cached
ssh_is_directory(). The local case had tests, the SSH cases only the cache-count test, which
never looked at where the file ended up. The fixture reuses the shared directory of
SyncSshStopTest, resets the configuration so the item and the account are the only ones, and
destroys a leftover Synchronize in TearDown.

## run_file_item

The decision sits in Synchronize, so the item goes through a real single pass; the wait polls
for the expected file, because an SSH pass takes several connections, and destroy() joins the
worker before the test reads anything.

## file-into-directory

Four combinations of local and SSH sides. The SSH file target is the control: with a file name
as target nothing may be appended, so the cached directory check must answer "no" there.

## remote_size

An uploaded file belongs to the SSH user, and until the upload took the source's permissions
over it arrived as 0600, unreadable for the test process; existence and size are therefore
read through SFTP as that user, which stays correct whatever the mode.

## CopyLocalFileToRemoteKeepsThePermissions

The mirror image of CopyRemoteFileToLocalKeepsThePermissions: the source gets permissions no
umask produces (rwxr-xr--), the upload goes to /tmp where the SSH user may create files, and
the mode is read back through the filesystem because the server is localhost. The remote file
is removed as the SSH user, before and after, since it belongs to that user.
