# SyncState

Module comment for SyncState.h / SyncState.cpp.

## SyncState

What both sides of a synchronisation looked like after the last pass, one entry per file. It is
the third point of view a two-way sync needs: with only the two sides, a file that is missing on
one of them could as well be new on the other as deleted on this one, and the only safe reading
is "new" - which brings a deleted file back. With the state, a file that was there last time and
is gone now was deleted, and a file that was not there last time is new.

One state per sync item, kept in its own file so that a large tree does not bloat the
configuration and so that the GUI and the daemon, which share the configuration directory,
share the state as well. SyncArbiter already keeps the two from running the same pass at once.

## FileState

Size and the write time on each side, as they were observed when the file was last brought in
sync. Both times are kept rather than one, because the two sides may store them with different
resolution (FileDifference.md#time-tolerance) and each side is later compared with its own
value. The size is the same on both sides by construction, so it is stored once.

## path

Entries are keyed by the file's path relative to the item's root, with '/' as separator on
every platform, so that a state written on Windows and read on Linux (a configuration copied
along) still names the same files. For a File item the key is the file name.

## record

record() also marks the path as seen, so a pass that updates every file it meets and then calls
forget_unseen() needs no second call per file. Loading goes through the constructor instead, so
a freshly read state has seen nothing yet.

## changed

Whether anything differs from what was loaded or constructed. A pass over a large tree that
found nothing to do rewrites nothing: the state is written only when it changed, which for a
daemon cycling every minute is most of the time.

## forget_unseen

The pass that is going to prune calls seen()/record() for every file it meets, then
forget_unseen() drops the rest - files that left both sides, or that a changed wildcard or
exclusion now filters out. Only a complete pass may call it: after a stop or with an unreachable
side the unvisited files are still there, and dropping them would make the next pass take every
one of them for new.

## FORMAT_VERSION

The first line names the format and its version. A file from a newer version of the program is
refused rather than half understood: the unreadable path leaves the state out, the next pass runs
without it and writes the current format.

## sync_state_dir

The subdirectory "state" next to the configuration file, so the daemon finds the same files the
GUI wrote (Config::get_root_pathname()).

## sync_state_file_path

The same sanitised item name the backup proof uses (Util.md#as_file_name), with "state" as the
fallback for a name that has no usable character.

## read_sync_state

A missing file is the ordinary first run and no reason for a message; the caller gets no state
and proceeds as without one. An unreadable or foreign file is reported once at warning level and
treated the same way - the next complete pass overwrites it. One malformed line discards the
whole file: a state that is partly wrong is worse than none, because it would turn missing files
into deletions.

## sides-must-match

The entries describe one pair of paths. After the user points the item elsewhere, the old
entries would make every file in the new target look deleted, so the state carries its source
and target and is ignored when they differ.

## write_sync_state

Written to a temporary file and renamed into place, like the configuration and the proof: a
crash in the middle of the write leaves the previous state, not half a file. The parent
directory is created on the way, since the state directory does not exist before the first
two-way pass.

## line-format

Not JSON like the configuration and the proof: measured in a Debug build, 100 000 files took
8 s to write and 8 s to read through boost::property_tree and filled 17 MiB, for something a
daemon handles every cycle. One line per file - size, source time, target time, path, separated
by tabs - takes 1.1 s to write and 1.5 s to read for the same tree in the same build, in 7 MiB,
and is still readable in an editor. The
path comes last so it may contain tabs; tab, line break, carriage return and backslash are
escaped as \t, \n, \r and \\, because a Linux file name may contain any of them.

## utf8-file

UTF-8 through util::write_utf8_file() and util::read_utf8_file(), for the same reasons as the
configuration (Util.md#utf8-files): paths with umlauts, Cyrillic letters or emoji have to
survive the round trip.

## integer

std::from_chars has no wchar_t overload, and narrowing a wide string blindly would turn a
non-ASCII character into some byte that may well be a digit - so only ASCII digits and the sign
are let through before the conversion.

## nanoseconds

The write times are stored as nanoseconds since the Unix epoch (Util.md#file-time-conversions),
not as seconds: a file rewritten within the same second must still differ from the stored
state, or an edit shortly after a sync would pass for unchanged.

## knows_below

Whether any file below the directory was in sync last time - the question SyncTree asks about
a directory that is present on one side only (SyncTree.md#DirState): known, it was deleted on
the other side; unknown, it is new. The map is ordered, so one lower_bound() on the prefix
answers it. A directory that was empty last time has no entry and therefore counts as new;
it comes back as an empty directory, which loses nothing.

## by_size_

A renamed file is found through what did not change: its size and its write time
(SyncFile.md#renames). Scanning every entry for each new file would make a first pass over a
fresh target quadratic, so the entries are indexed by size as well, and with_size() narrows a
lookup to the handful of files that happen to share one. Kept in step with files_ by every
record() and forget().

## pass-notebook

Besides what it remembers between passes, the state carries what one pass has put off until
its end and must not persist: the deletions a two-way or move-tracking pass found
(SyncFile.md#deferred-deletions) and the directories SyncTree wants removed once they are
empty (SyncTree.md#prepare-and-settle). They live here because every SyncFile of the pass
shares this object already; claim() is how a file that turns out to be the renamed one takes
its old entry out of the deletions before they run.

## mass-deletion-approval

Part of the pass notebook, not of the stored state: whether this pass may carry out a mass
deletion (SyncBase.md#mass-deletion). Synchronize sets it for the single run the user starts
from the preview, and nothing else does, so the daemon and an ordinary run never have it.
