# GoogleDriveTests

Module comment for GoogleDriveTests.cpp.

## GoogleDriveTest

Each test starts a stand-in for Google's servers (WebDavTestServer.md#GoogleDriveTestServer)
and points the sign-in settings and the account at it; TearDown puts the real settings back.
The files of the drive live in the server's memory, so the tests prepare and inspect them
through the server's test addresses, not through the client under test. Without python3 the
tests are skipped.

## account-per-test

The account's name contains the test's name. Access tokens and folder identifiers are cached
per account for the life of the process, and each test has a new server that knows neither.

## on_server

Every file and folder in the drive that is not in the trash, by path.

## PagedGoogleDriveTest

A server that hands out two entries per page, whatever page size is asked for.

## login

The whole sign-in without a person: a thread plays the browser, visits the consent address and
follows the redirect to the port the login listens on, while the test polls the login the way
the dialog's timer does.

## reason

The stand-in refuses a wrong token the way Google does, with the reason in `error.message`;
the exception has to carry it (HttpClient.md#reason).

## difficult-names

An apostrophe and a backslash, which the query syntax escapes, a quotation mark, which JSON
escapes, umlauts and a percent sign, which the address encodes.

## trash

The removed file is gone for the client and for the listing of the drive, and a second removal
fails because there is nothing to remove.

## StatusAndAnswerGiveTheReason

The table of HttpClient.md#reason_of by itself, with the answers the three services give for
a full store, and a conflict that must not be taken for one.

## AStaleAccessTokenIsRenewedOnceAndTheRequestRepeated

A token the test server never issued is put into the cache. The server refuses the request
with 401; the remote layer forgets the token, fetches a new one with the refresh token and
asks again, and the caller sees none of it (SyncFileSsh.md#token-renewed-once).

## ARefusedRenewalIsAnExpiredSignInAndEndsTheItem

An account whose refresh token the server does not know: the renewal is refused with
"invalid_grant". Listing through that account is one error that names the expired sign-in
and abandons the pass, and a removal answers AccessTokenExpired (GoogleAuth.md#expired).
