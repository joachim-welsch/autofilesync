# DropboxTests

Module comment for DropboxTests.cpp.

## DropboxTest

The counterpart of OneDriveTests for Dropbox: each test starts a stand-in for Dropbox's servers
(WebDavTestServer.md#DropboxTestServer) and points the sign-in settings and the account at it;
TearDown puts the real settings back. Without python3 the tests are skipped.

## account-per-test

The account's name contains the test's name, because access tokens are cached per account for
the life of the process and each test has a new server.

## on_server

Every file and folder in the drive, by path, in the case the names were given.

## PagedDropboxTest

A server that hands out two entries per page.

## login

The whole sign-in without a person (GoogleDriveTests.md#login), through one of the three
registered ports (GoogleAuth.md#redirect-ports).

## difficult-names

An apostrophe, an ampersand, umlauts and a percent sign. The umlauts matter most here: they
travel in an HTTP header for the content calls, and the server refuses a header that is not
plain ASCII (Dropbox.md#header-safe-json).

## LargeFileIsUploadedInPieces

Seventeen mebibytes: a first piece, a middle piece and a last one, so all three calls of an
upload session are used. The server refuses a piece that does not continue where the last one
ended.

## PathsIgnoreCaseButNamesKeepIt

A file laid down as "Urlaub/Strand.JPG" is found under "/urlaub/strand.jpg" and reports its
name in the original case.
