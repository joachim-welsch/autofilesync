# GoogleDriveAccountTest

Module comment for GoogleDriveAccountTest.cpp.

## GoogleDriveAccountTest

Drives "Datei → Neue Google-Drive-Verbindung…". TearDown waits for the thread that played the
browser, restores the real sign-in settings and the real way to open a browser, and the base
fixture reads the configuration back, which removes the connection a test saved.

## browser

The dialog is told to open the sign-in page with a function that visits it on a thread, as the
user's browser would (WebDavTestServer.md#visit_like_a_browser). The test server agrees at
once and redirects to the dialog's port.

## login

OK must be locked before the sign-in and free after it; the test waits for it to become free,
which is the moment the dialog's timer has seen the browser come back and exchanged the code.
The saved connection is then opened in the directory dialog to see a folder of the drive.
