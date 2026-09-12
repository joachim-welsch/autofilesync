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
