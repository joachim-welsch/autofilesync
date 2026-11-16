# GoogleDriveLiveTests

Module comment for GoogleDriveLiveTests.cpp.

## GoogleDriveLiveTest

The same client as in GoogleDriveTests.cpp, but against Google's real servers instead of the
stand-in. The stand-in answers the way the documentation was understood; only the real drive
shows whether the queries, the escaping, the resumable upload and the trash behave like that.
The tests need a network connection, a build with the Google client (GoogleAuth.md#client) and
a signed-in account of the user (#account-of-the-user); without one of them they are skipped
with the reason, like the SSH tests without a server.

## account-of-the-user

The sign-in is taken from the configuration of the user: the Google Drive account named
"Testuser", whose refresh token is decrypted with the key file beside the configuration. Both
files are only read. The configuration is parsed here instead of loaded through Config,
because Config looks for the key beside its own path, which during the tests lies in the
private home (TestMain.md#private-home). The account the tests work with carries its own name,
so that a path such as `drivelive:/...` can never be mistaken for one of the user's items.

While the OAuth client is in Google's status "Testing", a refresh token expires after seven
days. The tests are then skipped with Google's reason, and signing in again in the program
brings them back.

## user_config_dir

The directory handed over by the test program (TestMain.md#config-of-the-user). On Windows,
where the tests run with the real home, and under a debugger that sets up the environment
itself, it is the directory Config works with.

## live_drive

Found once per run: the account, or why there is none. One access token is fetched as a probe,
which also tells a revoked or expired sign-in and a missing network apart from a usable drive.
All tests share the account, and with it the access token cached for its name.

## folder-per-test

Every test works in a folder of its own in the root of the drive, named after the test and the
process, so that two runs and the user's own files do not meet. With the scope `drive.file`
(GoogleAuth.md#scope) the program cannot see the user's other files anyway. The name is set
before the folder is created, so that TearDown removes it also when creating it failed halfway.

## delete_for_good

The client only ever moves entries into the trash (GoogleDrive.md#remove), which would leave
the drive different from before. TearDown therefore deletes the test's folder for good, with a
request of its own. Google deletes everything below a folder with it, including what the test
had moved into the trash from there.

## named

Every entry of the drive with that name, trashed or not, asked from the drive directly. It is
how the tests see what the client under test hides: twins of one name and the trash.

## put_on_drive

Prepares a file in the drive. Unlike with the stand-in there is no back door, so it goes
through the client's own upload; the tests of the upload itself do not use it.

## difficult-names

An apostrophe and a backslash, which the query syntax escapes, a quotation mark, which JSON
escapes, umlauts and a percent sign, which the address encodes. Whether the escaping is right
only the real drive can tell. The listing of the folder is not asked, since it leaves out a name
with a backslash on purpose (GoogleDrive.md#list).

## trash

The removed file is gone for the client but still in the drive, marked as trashed. Its name
contains the name of the test's folder, because named() searches the whole drive.

## single-file

The item a user sets up first: one file into a folder of the drive. The program finds out
that the target is a folder and lets SyncFile append the file's name (Synchronize::run_sync_item);
copy_into_folder() does the same, which tu::run_sync does not.
