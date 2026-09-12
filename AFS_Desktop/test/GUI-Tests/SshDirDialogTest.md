# SshDirDialogTest

Module comment for SshDirDialogTest.cpp.

## kSortProbeDir

Own directory per test run, distinct from the fixtures the Integrationtests project keeps under
the same account, so the two suites cannot collide.

## RawSftpSession

Bare libssh/SFTP session for fixture setup: SshDirInspector only lists and reads directories, it
has no upload/mkdir - those are only needed here, to build the small remote directory the sort
test navigates into.

## removeSortProbeDir

Best-effort: an interrupted previous run may have left the directory behind, so removal happens
before creation too and neither treats a missing file as an error.

## SshDirDialogTest

Drives the Home and Sort-by-name buttons SshDirDialog got alongside Up and Refresh: Home should
jump to the account's login directory, and Sort should reorder the currently shown entries
ascending, then descending, without a new server round trip. Needs the local SSH test server
from test/Integrationstests/SSH_TESTS_README.md and skips itself when that is not reachable, the
same way the SSH Integrationtests do.
