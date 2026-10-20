# ChangeWatcher

Module comment for ChangeWatcher.h / ChangeWatcher.cpp.

## ChangeWatcher

Tells a continuous run that something changed in the local directories of an item, so that
the run can follow at once instead of scanning every tick (Synchronize.md#due). It records
only that a change happened and when - the pass finds out what - and it never decides
anything itself: the run reads the record, waits for the change to settle and clears it after
the pass. One watcher per run, all items in it, since the operating system's notification
handle is one per process on every platform and a thread per item would be waste.

## WatchedDir

One directory of one item, recursive for a tree item, flat for a directory item and for the
parent directory of a file item. The same item usually appears twice, once per side.

## watching

False for an item whose directories could not be watched - a missing path, a platform or
account limit, a file system that gives no notifications - from the start or later. The run
then treats the item as before, by its tick, and the log says so once (watching_unavailable),
which is the whole fallback: a watcher that cannot watch must never make a pass wait.

## clear

Forgets the recorded change. The run clears after its pass, because the pass's own copies into
a watched target are changes too and would otherwise trigger the next pass at once.

## overflow

Every backend can lose events when too many arrive - inotify's queue overflows, FSEvents
reports "must scan subdirectories" or dropped events, ReadDirectoryChangesW returns with an
empty buffer. Lost events are reported as a change of every item (or of the item whose buffer
overflowed), so that a pass follows and finds what was missed; the ten-minute scan in
Synchronize catches the rest.

## Backend

The platform part behind the one interface: a thread that blocks on the system's handle and
reports into the owner through changed() and lost(). Linux uses inotify, macOS FSEvents on a
run loop thread, Windows ReadDirectoryChangesW with overlapped I/O; a platform without one of
them gets a backend that declares every item unwatched. The macOS and Windows backends were
written against the documented APIs without a compiler at hand - the first build there is the
first test.

## inotify-per-directory

inotify watches single directories, not trees, so a tree item adds one watch per directory
at the start and one more for every directory that appears while watching (IN_CREATE or
IN_MOVED_TO with IN_ISDIR), from the thread that sees it appear. The tree is listed in full
before the first watch is added: listing a directory updates its access time, which a watch
on it or on its parent reports as IN_ATTRIB, and a watcher that reports a change the moment
it starts would trigger a pass for nothing. A directory that appears later is listed with its
parent's watch already in place; the extra event is harmless, since its creation was one. The kernel's limit on watches
(fs.inotify.max_user_watches) turns into "cannot be watched" for the item whose directory hits
it; the items already watched keep working.

## windows-wait-limit

WaitForMultipleObjects takes at most 64 handles, one of them the stop event, so a run with more
than 63 watched directories watches the first 63 and leaves the rest to their tick. Items are
directories here, not files, so the limit is far away for a user with a few synchronisations.

## available

Whether this build has a backend that watches at all; Synchronize does not create a watcher
without one, so the run stays what it was.
