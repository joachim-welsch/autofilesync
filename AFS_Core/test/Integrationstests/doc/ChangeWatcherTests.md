# ChangeWatcherTests

Module comment for ChangeWatcherTests.cpp.

## ChangeWatcherTest

Drives the ChangeWatcher (ChangeWatcher.md#ChangeWatcher) on a temporary tree and a flat
directory; the log goes into the same directory so that the "cannot be watched" line of the
missing-directory test leaves nothing behind.

## log-in-test-dir

The logging guard is reset in TearDown before the directory is removed: as a plain member it
would restore the log file only after TearDown, and Windows refuses to remove the open file.

## watch

Every test builds its watcher through watch(), which first lets the file system catch up with
the directories the test just created: FSEvents assigns its event IDs when fseventsd reads the
kernel's queue, a few milliseconds after the change, so a stream started "since now" right
after a mkdir still delivers that mkdir (ChangeWatcher.md#fsevents-paths). A pause of 100 ms
is ten times what was measured; without it the flat and the two-item test see a change before
anything happened. The watcher lives in the fixture and goes in TearDown before the logging
guard, so that a watch lost during the removal still logs into the test directory.

## noticed

Events arrive on the watcher's thread a few milliseconds after the write, so every test polls
last_change() for up to three seconds instead of asserting right away; a negative expectation
polls for one second, which is long for inotify and short for the suite.

## wait_until_quiet

One write produces several events (create, modify, close) and noticed() returns on the first.
A test that clears the item afterwards must first let the rest arrive, or a late event marks
the item again right after clear(). FSEvents delivers the first event at once (NoDefer) and
the others bundled after its latency of one second; Windows reports them on the next
ReadDirectoryChangesW call. So the test waits until no event came for 1.5 s.

## ADirectoryCreatedLaterIsWatchedToo

inotify watches directories one by one (ChangeWatcher.md#inotify-per-directory): a directory
created after the start, two levels deep, has to be picked up by the thread that saw it
appear, or the file written into it would go unnoticed until the scan interval.

## AMissingDirectoryIsNotWatched

A directory that cannot be watched leaves the item unwatched - the tick applies to it - and
the other items watched, so one bad path does not take the whole watcher down.

## AnOverflowMarksEveryWatchedItemAsChanged

After lost events nobody knows which item changed, so all watched ones count as changed and
the next pass looks (ChangeWatcher.md#overflow). The overflow is reported by the test
(ChangeWatcher.md#test_report_overflow); that the kernel's overflow event leads to the same
call is not covered.

## AWatchedDirectoryThatDisappearsIsNoLongerWatched

The directory of an item is removed while it is watched - a drive that is unplugged, a folder
that is replaced. The item must fall back to its tick (ChangeWatcher.md#root-directory-gone);
a subdirectory of a tree that goes the same way is only a change. Linux only: the other
backends are not built here and their behaviour is not known.
