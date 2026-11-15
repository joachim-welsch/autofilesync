# WebDavAccountTest

Module comment for WebDavAccountTest.cpp.

## WebDavAccountTest

Drives "Datei → Neue WebDAV-Verbindung…". The fixture's TearDown reads the configuration back
from the file, which removes a connection a test saved.

## add_connection

Opens the dialog, fills it and presses OK. The dialog that OK may open (the validation message
or the question whether to save an unchecked connection) is queued from inside the action,
after the connection dialog has been taken from the watcher's queue. If the connection dialog
is still open afterwards it is cancelled.

## NOBODY_LISTENS

Port 9 on this computer: nothing listens there, the connection is refused at once, and the
address passes the https rule because it is local.

## with-server

Uses the WebDAV server of the integration tests (WebDavTestServer.md#WebDavTestServer). After
the checked connection is saved, the directory dialog is opened on it directly to see that the
server's folder is listed.

## nextcloud

The address of the server is typed, as a user would, against a server laid out like a Nextcloud
(WebDavTestServer.md#nextcloud-layout). The connection must be saved without a question, with
the address of the files.
