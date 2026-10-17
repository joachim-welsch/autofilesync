# SshDirDialogTest

Module comment for SshDirDialogTest.cpp.

## SshDirDialogTest

Drives the Home and Sort-by-name buttons SshDirDialog got alongside Up and Refresh: Home should
jump to the account's login directory, and Sort should reorder the currently shown entries
ascending, then descending, without a new server round trip. The three unsorted files live in
the probe directory SshGuiTest provides (SshTestServer.md#probe_dir) and are removed before the
directory is; the fixture skips itself when the test server is not reachable.

## home-start-dir

The Home test opens the dialog in the probe directory rather than a fixed path: that directory
exists on every platform once SetUp has run, and it is not the login directory, so a click on
Home has to change the shown path.
