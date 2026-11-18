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
expect_success(). Both exceptions carry the reason (SftpConnection.md#RemoteFailure): a
name that does not resolve is NetworkUnavailable, a timeout ConnectionTimeout, a refused
connection or failed TLS handshake ServerUnreachable, and a transfer that broke off -
nothing received, nothing sent, part of a file - ConnectionLost.

## reason_of

The reason behind an HTTP status, in one place for every store: 401 is a refused login, 403
a missing right, 404 a missing file, 405 and 501 something the server does not do, and 429,
502, 503 and 504 a server that says "later". A full store has no status of its own outside
WebDAV's 507: Google answers 403, Dropbox 409, and say what they mean in the body, so the
statuses that can mean it are looked at for the words the services use. Everything else is
UnknownError, and the caller reports the text.

## reason

Why the server refused, taken from a JSON answer: Google and Microsoft put it into
`error.message`, the token addresses into `error_description`, Dropbox into `error_summary`.
A status alone says too little; "HTTP 403" from Google can mean that the Drive API is not
switched on for the project, that the scope does not cover the file or that a quota is used
up, and only the text tells which. It stands before the address, because the address of a
query is long and has no place to break, so a dialog shows only what comes before it. An
answer that is not JSON, or one written into a file (#transfers), yields no reason and the
message stays as it was.

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

## rewind

libcurl sends a request again by itself when the reused connection (connection-reuse) turns
out to have been closed by the server in the meantime, which it may only notice after the
first bytes of the file are gone. It then asks for the file to be set back; without an answer
the upload fails with "necessary data rewind was not possible". The file is set back to where
it stood when upload_from() was called - an upload in pieces starts in the middle of the file -
and the count of bytes still to send starts again.

## stop

Only transfers ask whether the user stopped the run, and they report the bytes moved so far to
the progress display. The small requests do not ask: the dialogs use them too, outside any run,
when Synchronize::stopping() is true all the time.

## perform

Returns the HTTP status, or nothing when the user stopped the run during a transfer.

## without_body

For HEAD requests: tells libcurl that no body follows the headers. Without it libcurl would wait
for the bytes the Content-Length header announces.
