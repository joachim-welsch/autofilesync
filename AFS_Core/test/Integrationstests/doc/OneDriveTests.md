# OneDriveTests

Module comment for OneDriveTests.cpp.

## OneDriveTest

The counterpart of GoogleDriveTests for OneDrive: each test starts a stand-in for Microsoft's
servers (WebDavTestServer.md#OneDriveTestServer) and points the sign-in settings and the
account at it; TearDown puts the real settings back. The files live in the server's memory and
are prepared and inspected through its test addresses. Without python3 the tests are skipped.

## account-per-test

The account's name contains the test's name, because access tokens are cached per account for
the life of the process and each test has a new server.

## on_server

Every file and folder in the drive, by path.

## PagedOneDriveTest

A server that hands out two entries per page.

## login

The whole sign-in without a person (GoogleDriveTests.md#login). The server issues a new refresh
token for every login, so the test proves the token by opening the drive with it.

## difficult-names

An apostrophe, an ampersand, umlauts and a percent sign. Quotation marks and backslashes are
left out here: OneDrive does not allow them in names.

## LargeFileIsUploadedInPieces

Eleven mebibytes, one more than a piece, so the upload needs two requests. The server refuses a
piece that does not continue where the last one ended.

## RenewedRefreshTokenIsKeptInTheConfiguration

After the first request the account in the configuration must hold the refresh token the
server issued with the access token, not the one it was set up with (GoogleAuth.md#renewed-refresh-token).
