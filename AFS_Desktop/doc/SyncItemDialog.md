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
