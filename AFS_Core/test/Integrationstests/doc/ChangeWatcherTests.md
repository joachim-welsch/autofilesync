# ChangeWatcherTests

Module comment for ChangeWatcherTests.cpp.

## ChangeWatcherTest

Drives the ChangeWatcher (ChangeWatcher.md#ChangeWatcher) on a temporary tree and a flat
directory; the log goes into the same directory so that the "cannot be watched" line of the
missing-directory test leaves nothing behind.

## noticed

Events arrive on the watcher's thread a few milliseconds after the write, so every test polls
last_change() for up to three seconds instead of asserting right away; a negative expectation
polls for one second, which is long for inotify and short for the suite.

## ADirectoryCreatedLaterIsWatchedToo

inotify watches directories one by one (ChangeWatcher.md#inotify-per-directory): a directory
created after the start, two levels deep, has to be picked up by the thread that saw it
appear, or the file written into it would go unnoticed until the scan interval.

## AMissingDirectoryIsNotWatched

A directory that cannot be watched leaves the item unwatched - the tick applies to it - and
the other items watched, so one bad path does not take the whole watcher down.
