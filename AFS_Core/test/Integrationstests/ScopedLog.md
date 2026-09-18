# ScopedLog

Module comment for ScopedLog.h.

## ScopedLog

A test that needs a log of its own redirects Logging to a file in the temp
directory for its lifetime. The destructor hands path and level back to what
they were - the scratch log of TestMain.cpp - and deletes the file, so neither
the settings nor the file outlive the test. Logging is a process-wide singleton:
a test that forgets to restore it changes every test that follows.
