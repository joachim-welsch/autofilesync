# SyncArbiterTests

Module comment for SyncArbiterTests.cpp.

## time_acquire

Two arbiters in one process see each other exactly as two processes do: the Windows events live
in the Global\ namespace and flock treats two open file descriptors of the same file
independently, even within one process.

## BackgroundWaitsWhileForegroundHolds

The requirement: while the GUI synchronises, the daemon pauses.

## ForegroundWaitsForRunningBackgroundPass

The other direction: only one synchronisation at a time means the GUI has to let a pass that is
already running finish.

## AbortEndsTheWaitInsteadOfBlocking

A stop request must not be delayed by whatever the other side is doing, or stopping a sync would
hang for as long as the other process keeps going.

## RepeatedAcquireOfTheSameHolderReturnsAtOnce

acquire() is called once per pass, so the second call of a holder must not wait for itself.

## UncreatableLockFileIsWarnedAboutAndDoesNotBlock

Without its lock files the arbiter cannot arbitrate. It must then let both sides run and say
so in the log, rather than keep the service waiting for a lock that will never exist.

## locked-temp-dir

In the tests the lock files lie in the temp directory (SyncArbiter.md#TEST_SCOPE), so pointing
TMPDIR at a directory without write permission is what makes them uncreatable.
