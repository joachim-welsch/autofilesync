# SshTestServer

Module comment for SshTestServer.h / SshTestServer.cpp.

## SshTestServer

Everything the SSH GUI tests share: the account of the local test server from
test/Integrationstests/SSH_TESTS_README.md, a bare SFTP session to build and inspect remote
fixtures, and the fixture that skips a test when the server is not there.

## RemoteSftp

Bare SFTP session for fixture setup and verification: SshDirInspector only lists and reads
directories, it has no upload, mkdir or rmdir - those are only needed by the tests, to build the
small remote directories they navigate into and to read back what a run put on the server. It
uses libssh on Unix and libssh2 under `_WIN32`, the same split AFS_Core makes; the platform
session lives behind the opaque Session, so the tests see one interface.

## scratchDir

On Unix the remote fixtures live under `/tmp`. The SFTP server of Win32-OpenSSH has no `/tmp` -
its paths look like `/C:/Users/testuser` - so on Windows they are placed in the login
directory the server reports.

## SshGuiTest

GuiTest plus the test server: SetUp connects once and skips the test with the connection error
when that fails, the same way the SSH Integrationtests do, and provides an empty probe
directory per process.

## probeDir

Own directory per process, distinct from the fixtures the Integrationtests project keeps under
the same account, so the two suites cannot collide. A leftover from an interrupted run is
removed before creation. The fixture removes only the directory itself at the end: every test
removes what it put inside, so a file a test forgot shows up as a directory that stays.
