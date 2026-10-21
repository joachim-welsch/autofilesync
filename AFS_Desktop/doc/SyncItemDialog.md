# SyncItemDialog

Module comment for SyncItemDialog.h / SyncItemDialog.cpp.

## options-grouped

The three comparison criteria form a group of their own with a frame and a title; the other
options are the synchronisation's behaviour and stand in one column. Before, the criteria sat
in a second column between unrelated options and could be read as belonging to the lines
they shared.

## tolerance-beside-time

The tolerance of the time comparison stands inside the criteria group, right below the date
criterion it qualifies, and is enabled only while that criterion is checked: it has no meaning
without it, and greying it out says so without a hint text. The range reaches 3600 s because
FAT stores local time and a DST change shifts every stored time by an hour; the default of two
seconds is the coarsest step a common file system keeps (see FileDifference.md#time-tolerance).

## option-locks

Options that make no sense together lock each other, and the whole set is re-evaluated from
the current state after every click, so no click can leave a stale lock behind:

- "Quelle nach dem Kopieren löschen" turns the sync into a move. Two-way sync would copy the
  moved files straight back, and "Gelöschte Dateien im Ziel ebenfalls löschen" would delete
  them in the target on the next pass because the source no longer has them - both are locked.
- Two-way sync and "Kopieren wenn Ziel neuer" contradict each other (the newer side wins
  versus the source always wins), so each locks the other.
- "Kopieren wenn Ziel neuer" needs a criterion that can find a newer target at all: only the
  time and the size comparison report DestModified in a one-way sync.
- The type Datei has no target-only files, so "Gelöschte Dateien im Ziel" is locked there.

A locked option is cleared as well, so the configuration never stores a forbidden pair - that
also resolves an old file that has both options set: the option earlier in the list wins. The
reverse direction (B is checked, so A is disabled) only disables, because A cannot be checked
at that point. The tolerance field follows the time criterion for the same reason
(#tolerance-beside-time).

## exclusions-one-per-line

One wildcard pattern per line; blank/whitespace-only lines are ignored so trailing newlines or
accidental extra Enters don't turn into a pattern that matches nothing (or, worse, an empty
pattern).

## empty-wildcard-allowed

An empty wildcard is a valid, meaningful choice (matches every file - see
SyncBase::syncDirEntry()'s "wildcard.empty() || wildmatch(...)"), not an incomplete form;
requiring one just blocked OK with no indication why.

## no-volume-spec-on-unix

No volume spec on Unix: mountpoints already identify drives, and a "path (device)" suffix would
not be a valid path - nothing downstream (pathContainsVolumeName expects the "Label (C:)rest"
form) understands it.

## name-compared-against-original

name_ is the original name (set in set_data(), still unchanged here since
TransferDataFromWindow() only runs after Validate() succeeds). Comparing against it - not just
checking name_.empty() - also catches renaming an existing item onto another item's name, not
just brand-new items.

## track-moves-implied

A two-way item keeps a sync state anyway and recognises renames with it
(SyncFile.md#renames), so the option is shown ticked and locked while two-way sync is on; it
is the one-way item's choice (SyncItem.md#track_moves_). A file item has nothing to rename,
so there the option is locked too.

## two-way-hint

Two-way synchronisation now carries deletions and renames across, which is more than the
option's name says and the one thing a user must not be surprised by. A line under the options
says so while the option is ticked, and hides when it is not, so the options page stays as short
as before for everyone else. The text is wrapped to a fixed width in device-independent pixels,
since a wxStaticText does not wrap by itself and the sizer would widen the dialog to the
sentence instead.

## conflict-copies-two-way-only

"Bei Konflikten beide Fassungen behalten" (SyncItem.md#conflict_copies_) only means something
when both sides may change, so it is enabled with two-way sync and greyed out without it. Its
value is left alone when it is greyed out: a user who switches two-way sync off and on again
finds the option as they had it.

## phone-buttons

"Handy…" next to "SSH…" on both sides: a paired phone is a place like an SSH server, but the
user should not have to know that, so it gets its own button and its own dialog
(PhoneDirDialog.md). Without a paired phone the button explains where to pair one instead of
opening an empty dialog.
