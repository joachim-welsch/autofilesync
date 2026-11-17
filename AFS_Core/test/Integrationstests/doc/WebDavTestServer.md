# WebDavTestServer

Module comment for WebDavTestServer.h / WebDavTestServer.cpp.

## WebDavTestServer

Starts test/tools/webdavTestServer.py as a child process serving one directory and stops it in
the destructor. The server asks for the user and password given here. The GUI tests use the
class as well, which is why it does not include the precompiled header of the integration
tests.

## script

The script is found relative to this source file, so the class needs no definition from the
build of the test program that uses it.

## port

The server binds to a free port and prints it as its first line; the constructor returns once
that line has arrived, so the server is ready when the test goes on. No line means the server
did not start, and running() is false.

## windows-python

Windows has no python3 on the PATH as a rule: the installer of python.org brings the launcher
py and python.exe, and python3.exe is often only the alias of the Store that starts nothing.
The candidates are tried in this order until one of them prints a port; one that is missing or
ends without a port is passed over. Without any Python running() is false and the tests are
skipped, as elsewhere.

## windows-quoting

CreateProcessW takes one command line, which Python splits again by the rules of the C runtime:
an argument with a space, tab or quote is put in quotes, a quote inside is escaped with a
backslash, and backslashes are doubled only in front of a quote, including the closing one.

## windows-handles

The server gets a handle list naming its output pipe and the null device and nothing else. A
child that inherits every inheritable handle would keep the redirected output of the test
program open for as long as it lives (GUI-Tests UsbPhone.md#inherited-handles). Errors of the
server go to the null device: nobody reads them, and a pipe nobody reads would fill and stop
the server.

## windows-job

The launcher py starts the real python.exe as a child of its own, so ending the process that
was started would leave the server running. The process is therefore put into a job before it
runs a single instruction (started suspended), and the job ends everything in it when its
handle is closed - also when the test program dies without reaching the destructor. The
destructor then waits for the started process, so the port and the files of the served
directory are free when it returns.

## PythonTestServer

The part both test servers share: start a script from test/tools as a child process, read its
port, stop it in the destructor.

## GoogleDriveTestServer

Starts test/tools/googleDriveTestServer.py, a stand-in for Google's sign-in and Drive API that
keeps its files in memory. It accepts the client and the refresh token named here.

## visit_like_a_browser

Fetches an address and follows redirects, discarding the pages. That is all the browser does
during a sign-in when the user agrees.

## nextcloud-layout

With this layout the server serves its directory below NEXTCLOUD_PREFIX only and answers every
other address with 405, as the web page of a Nextcloud answers a PROPFIND.

## OneDriveTestServer

Starts test/tools/oneDriveTestServer.py, a stand-in for Microsoft's sign-in and the OneDrive
part of Microsoft Graph that keeps its files in memory. It accepts the client and the refresh
token named here and issues a new refresh token with every access token.

## DropboxTestServer

Starts test/tools/dropboxTestServer.py, a stand-in for Dropbox's sign-in and the files part of
its API that keeps its files in memory. It accepts the app key and the refresh token named here
and redirects only to the registered ports.

## S3TestServer

Starts test/tools/s3TestServer.py, a stand-in for an S3 service that keeps two buckets in
memory and answers only requests with a valid signature for the keys and the region named
here.
