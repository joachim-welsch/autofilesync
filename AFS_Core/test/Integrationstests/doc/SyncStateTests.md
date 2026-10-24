# SyncStateTests

Module comment for SyncStateTests.cpp.

## config-dir-in-temp

sync_state_dir() derives from the configuration path, which ScopedConfigHome points at the
test directory, so the test never touches the user's real configuration directory. The log goes
to the same directory, so it disappears with it; the logging guard is reset in TearDown before
the directory is removed, because Windows refuses to remove an open log file.

## RoundTripKeepsEveryEntry

The names cover umlauts, Cyrillic letters and an emoji outside the BMP, the sizes an empty file
and one beyond 4 GiB, the times a nanosecond fraction, a time before the epoch and two times a
single nanosecond apart - every one of which a looser format (seconds, UTF-16 clamping,
32-bit sizes) would quietly change. Compared through FileState::operator==, so the round trip
has to be exact.

## TabsBackslashesAndLineBreaksInPathsSurvive

Everything the line format has to escape (SyncState.md#line-format), in the paths and in the two
header values alike, including a path that is nothing but a backslash and an escape sequence
written out literally ("\\n" as two characters), which must not be mistaken for a line break on
the way back.

## StateOfOtherPathsIsIgnored

See SyncState.md#sides-must-match: a state written for one pair of paths must not be applied
to another pair, in either direction.

## UnreadableFileMeansNoState

Each malformed file is answered with "no state", never with a partial one
(SyncState.md#read_sync_state): a JSON file, a header missing its target line, a file line with
too few fields and a number containing an Arabic-Indic digit that narrows to an ASCII digit's
byte (SyncState.md#integer). The well-formed line at the end shows the fixture itself is valid.
The file stays where it is; the next write replaces it.

## FailedWriteLeavesNoTempFileAndNoHalfFile

The parent "directory" is a regular file, so the state cannot be created at all. Neither a
".tmp" file nor a broken state file may remain, and the blocking file must be untouched - the
writer only ever renames over its own target.

## ChangedOnlyWhenSomethingDiffers

A pass that re-records every file with the values it already has, and forgets nothing that was
there, must leave the state unchanged, or the daemon would rewrite the file every cycle
(SyncState.md#changed). forget_unseen() on an empty state is the same case.

## ForgetUnseenDropsOnlyTheUntouched

A freshly read state has seen nothing, so seen() and record() are the only two ways an entry
survives forget_unseen(); after the pruning the set of seen paths starts empty again, so a second
forget_unseen() without any visit drops everything.

## ALargeTreeLoadsInReasonableTime

The measurement that decided the file format (SyncState.md#line-format): a tree of 100 000
files is written and read back, the durations are printed, and the test only fails when the
whole round trip takes longer than a daemon cycle should spend on bookkeeping. The bound is
generous because the suite runs as a Debug build, where the JSON version took 16 s and the
line format takes under 3 s.

## KnowsBelowSeesOnlyFilesUnderTheDirectory

A directory is known through the files below it and nothing else (SyncState.md#knows_below):
a name that is merely a prefix of a directory's name, or a file's own path, must not count,
and the last file below a directory takes the directory's knowledge with it.

## SizeIndexFollowsRecordAndForget

The index behind with_size() (SyncState.md#by_size_) has to follow every way an entry changes:
a record() that changes a file's size moves it, forget() and forget_unseen() drop it, and a
state read from the file is indexed on the way in. The pruning is tried on a state read back
from the file, since record() marks what it records as seen and a built state has nothing
unseen to prune.

## PassNotebookHandsOutWhatWasPutOff

The deferred deletions and directory removals (SyncState.md#pass-notebook): a claim takes a
deletion out once, a deletion for a renamed key is dropped, take_*() hands everything over and
empties the notebook, and none of it marks the state as changed.
