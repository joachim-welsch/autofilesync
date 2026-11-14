# CloudFolderButtonTest

Module comment for CloudFolderButtonTest.cpp.

## CloudFolderButtonTest

The sandbox already points the home directory at a temp directory, so a cloud client is
"installed" by writing the file it would leave there: Dropbox's info.json, Nextcloud's
nextcloud.cfg. Everything a test writes is remembered and removed in TearDown before the
fixture compares the state, so the next test starts without any cloud folder again.

## target_after_cloud_button

Opens the dialog for a new synchronisation, clicks "Cloud…" beside the target, reads what then
stands in the target field and cancels, so no synchronisation is created. The dialogs the
button opens are answered by the expectations the passed function queues. It is called after
the dialog itself has been queued, because the watcher answers dialogs in the order of its
queue.

## needs-clean-machine

Under Linux and macOS the detection reads nothing outside the redirected home directory. Under
Windows it also asks LOCALAPPDATA and the drives for Google Drive, which the sandbox does not
redirect: on a machine with Dropbox or Google Drive for desktop installed this test and the
single-folder test see those folders too and fail.
