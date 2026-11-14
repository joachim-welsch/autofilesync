# WebDavTests

Module comment for WebDavTests.cpp.

## WebDavTest

Each test starts a WebDAV server of its own (WebDavTestServer.md#WebDavTestServer) on a
directory below the temp directory, so "the server's files" can be prepared and inspected
through the file system. The account that points at it is added to the configuration, because
the remote functions and the synchronisation look accounts up by name. TearDown resets the
configuration, stops the server and removes the directory. Without python3 the tests are
skipped.

## PlainWebDavTest

The same with a server that ignores X-OC-Mtime and stamps uploads with the upload time, like
servers other than Nextcloud and ownCloud (WebDav.md#modification-time). The tests show what
still has to hold there: nothing is uploaded twice, a changed file goes up, and a two-way item
does not take the upload time for a change made on the server.

## difficult-names

A blank, umlauts and a percent sign: each has to survive the percent-encoding on the way to the
server and the decoding of the listing.

## WrongPasswordIsAnErrorNotAnAbsence

A refused login must not look like "the file is not there" (WebDav.md#errors).

## UnencryptedAddressOutsideThisComputerIsRefused

Needs no network: the address is refused before any connection is made. The second address
begins with "http://localhost" without being this computer (WebDav.md#https-only).

## two-way

Changes and deletions in both directions through a sync state, with the delete mode set to
permanent for the second half so the local deletion does not depend on a recycle bin.
