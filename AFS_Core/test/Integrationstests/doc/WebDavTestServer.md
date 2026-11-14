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
