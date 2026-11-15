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

Fehler 6 (round 6): the non-WIN32 default log path was a hardcoded path in one developer's
home directory - the Linux/macOS equivalent of
get_default_log_file_path() (already implemented for Windows via GetTempPathW) simply didn't
exist, so every install other than that one machine got a default log location nobody else could
write to. Verified by pointing HOME/XDG_CACHE_HOME at a fake location and checking the computed
path actually derives from them, rather than being a fixed string.

## fake-home-for-every-variable

All three are set so that the fallback path also points into the fake directory and the test
never writes into the real profile.

## ScratchLogTest

Redirects the log to a scratch file and restores path, level and origin
afterwards, since Logging is a process-wide singleton the other tests share.
The assertions compare everything behind the timestamp, so they pin the exact
place of the marker: between timestamp and level.

## WritingFollowsAFileReplacedByAnotherProcess

Replays what LogCut of the other program does: a shortened copy is renamed over
the log while this process holds it open. The next message must land in the file
the path names now.

## alternating-messages

The failing source and target of one item alternate in the log, so remembering only the last
message would hold nothing back.

## LevelNamesRoundTripAndUnknownNamesFallBackToError

The level in the configuration file is text; every name the logger writes must read back as
the same level, and a hand-edited unknown name falls back to Error - loud rather than silent.

## ShouldLogFiltersBelowTheLevel

The plain filter without the collect mode that LoggingCollectTests exercise: everything below
the level is out, the level itself and above are in.

## EraseFileStartsOverAndForgetsTheRepetitions

"Delete log" in the GUI: the file is gone and immediately reopened, and the repetition
memory goes with it (Logging.md#repetitions-follow-the-file) - a warning that was suppressed as
a repeat before is new again in the new file. close_file() flushes before each read.
