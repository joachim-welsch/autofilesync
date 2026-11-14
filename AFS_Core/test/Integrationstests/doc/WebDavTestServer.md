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

## windows

Not implemented for Windows: the tests that need the server are skipped there.

## PythonTestServer

The part both test servers share: start a script from test/tools as a child process, read its
port, stop it in the destructor.

## GoogleDriveTestServer

Starts test/tools/googleDriveTestServer.py, a stand-in for Google's sign-in and Drive API that
keeps its files in memory. It accepts the client and the refresh token named here.

## visit_like_a_browser

Fetches an address and follows redirects, discarding the pages. That is all the browser does
during a sign-in when the user agrees.
