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
