# HttpClient

Module comment for HttpClient.h / HttpClient.cpp.

## http

The HTTP requests of the WebDAV and the Google Drive client, on libcurl. The header shows no
libcurl type, so only this module includes its headers.

## errors

perform() throws `SshUnreachable` when the server could not be reached at all (name not
resolved, connection refused, timeout, TLS handshake) and `Error` for every other transport
failure. The callers treat the first as "not there at the moment", like an SSH host that is
switched off. An HTTP status is no error here; the caller judges it, usually with
expect_success().

## Request

One request on the calling thread's connection. Because the connection is shared, a Request
must be performed before the next one is constructed on the same thread: a Request built in
between takes the connection over. Whatever a request needs from another request (an access
token, say) has to be fetched first (GoogleDrive.md#DriveRequest).

## connection-reuse

Each thread keeps one libcurl handle for its lifetime and resets it for every Request. libcurl
then reuses the open connection, so a tree with thousands of files costs one TLS handshake per
thread instead of one per request. The handle is thread-local because an easy handle must not
be used from two threads.

## timeouts

Fifteen seconds to connect. After that a request has no total limit, since a large file may
take hours, but it is given up when no byte has moved for a minute.

## transfers

download_to() streams the answer into a file and makes an HTTP error status a failure, so an
error page is never written into the file as if it were the content. upload_from() sends a
file of known size. The "Expect: 100-continue" handshake is switched off: it only saves an
upload the server would refuse, and some servers do not answer it, which costs a second of
waiting per file.

## stop

Only transfers ask whether the user stopped the run, and they report the bytes moved so far to
the progress display. The small requests do not ask: the dialogs use them too, outside any run,
when Synchronize::stopping() is true all the time.

## perform

Returns the HTTP status, or nothing when the user stopped the run during a transfer.

## escaping

Percent-encoding of one path segment or one query value, and its reverse.
