# SshDirDialogTest

Module comment for SshDirDialogTest.cpp.

## sort_probe_dir

Own directory per test run, distinct from the fixtures the Integrationtests project keeps under
the same account, so the two suites cannot collide. On Unix it lives under `/tmp`. The SFTP
server of Win32-OpenSSH has no `/tmp` - its paths look like `/C:/Users/testuser` - so on Windows
the directory is placed in the login directory the server reports.

## RawSftpSession

Bare SFTP session for fixture setup: SshDirInspector only lists and reads directories, it has no
upload/mkdir - those are only needed here, to build the small remote directory the sort test
navigates into. It uses libssh on Unix and libssh2 under `_WIN32`, the same split AFS_Core makes.
The small wrappers below it (`remove_file`, `make_dir`, ...) hide that split, so the fixture logic
exists only once.

## remove_sort_probe_dir

Best-effort: an interrupted previous run may have left the directory behind, so removal happens
before creation too and neither treats a missing file as an error.

## SshDirDialogTest

Drives the Home and Sort-by-name buttons SshDirDialog got alongside Up and Refresh: Home should
jump to the account's login directory, and Sort should reorder the currently shown entries
ascending, then descending, without a new server round trip. Needs the local SSH test server
from test/Integrationstests/SSH_TESTS_README.md and skips itself when that is not reachable, the
same way the SSH Integrationtests do.

## home-start-dir

The Home test opens the dialog in the probe directory rather than a fixed path: that directory
exists on every platform once SetUp has run, and it is not the login directory, so a click on
Home has to change the shown path.
