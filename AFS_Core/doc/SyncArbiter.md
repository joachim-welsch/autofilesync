# SyncArbiter

Module comment for SyncArbiter.h / SyncArbiter.cpp.

## SyncArbiter

Cross process arbitration between the GUI and the backgroundsync daemon: both run the same
Synchronize engine over the same configuration, so only one of them may synchronise at a time.
The GUI has priority - the daemon pauses between its passes for as long as a GUI synchronisation
is running.

## acquire

Blocks until this process may synchronise; calling it again while already held returns
immediately. Returns false when abort turns true first, so a stop request is never delayed by
whatever the other process is doing.

## active_event_

A HANDLE to the Global\ event announcing that this side synchronises. It is deliberately kept as
a plain pointer so that no Windows header leaks into everything that synchronises.

## per-user-lock-path

Per user: /tmp is shared, and daemon and GUI only need to meet when they run under the same
account - which they must anyway to share the configuration under the user's profile.

## TEST_SCOPE

The test build announces itself under names of its own. With the shared names every test that
synchronises would wait for the passes of a daemon installed on the same machine, and the tests
of the arbiter itself would arbitrate against that daemon instead of against each other.
The test build also keeps its lock files in the temp directory of the process, which the test
run owns and removes, while the programs need the fixed /tmp: daemon and GUI may run with
different TMPDIR values and would otherwise never meet.

## use_private_scope

Switches a process to the TEST_SCOPE names at run time. The GUI tests need this because they link
the shipped library, which is compiled without _TEST: otherwise every synchronisation in a GUI
test waits for the passes of the user's running daemon - for example a 15 s SSH connect timeout,
longer than a test waits for a copy - and the daemon in turn pauses while a GUI test synchronises.
Must be called before the first synchronisation starts.
