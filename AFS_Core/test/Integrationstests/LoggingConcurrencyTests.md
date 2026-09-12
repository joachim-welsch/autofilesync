# LoggingConcurrencyTests

Module comment for LoggingConcurrencyTests.cpp.

## ConcurrentRotationAndLoggingDoesNotCrash

Regression test for Fehler 4: Logging::write() read and used the shared FILE* fos_ without
holding mutex_, while close_file()/open_file() (triggered by log rotation, e.g. LogCut) did lock
it. A rotation racing an in-flight write could fclose() fos_ out from under it, leaving write()
to dereference a dangling or null FILE*. Hammering rotation and logging concurrently from
multiple threads must not crash.

## WriteFailureDoesNotThrow

Fehler 4 (round 5): write() threw std::runtime_error when fprintf/fflush failed (e.g. disk
full). LOG_ macros run inside catch handlers and worker threads, so that exception escaped them
and terminated the process exactly when something was already going wrong. Logging to /dev/full
makes the flush fail deterministically; it must be swallowed, not thrown.

## OpenFileOnUnwritablePathDoesNotThrow

Fehler (round 6): open_file() threw std::runtime_error when the log path could not be opened
(missing/unwritable directory). set_log_file_path_name() calls it right after close_file()-ing
the previous handle - reachable from LoggingDialog's OK handler with bad user input - and
LogCut::close_files() calls it unconditionally at the end of every LogCut() run, which
MainWindow's constructor invokes unguarded on every single application startup. Either path
throwing would crash the whole app instead of just leaving logging disabled.

## blocked-parent-path

A path whose parent is a regular file, not a directory: create_directories and fopen() can never
succeed for it.

## DefaultLogFilePathHonorsHomeEnvVar

Fehler 6 (round 6): the non-WIN32 default log path was a hardcoded developer path
(/home/welschj/temp/autofilesync.log) - the Linux/macOS equivalent of
get_default_log_file_path() (already implemented for Windows via GetTempPathW) simply didn't
exist, so every install other than that one machine got a default log location nobody else could
write to. Verified by pointing HOME/XDG_CACHE_HOME at a fake location and checking the computed
path actually derives from them, rather than being a fixed string.

## fake-home-for-every-variable

All three are set so that the fallback path also points into the fake directory and the test
never writes into the real profile.
