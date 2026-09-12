# FileDifferenceTests

Module comment for FileDifferenceTests.cpp.

## FileDifferenceSshTest

Registers an SSH account whose host name can never resolve (RFC 2606 reserves the ".invalid" TLD
for exactly this), so every connection attempt fails fast and deterministically without
depending on a real SSH server - this exercises the same failure path as a genuine
"SSH-Verbindungsfehler" (Fehler 2) portably, in any environment.

## UnreachableSshSourceIsNotReportedAsSourceNotExists

Regression test for Fehler 2: read_ssh_file_info() returns false on a connection/auth failure
(invalid host, timeout, wrong credentials) without ever setting drive_mounted=false, so
FileDifference cannot tell "the SSH source is unreachable" apart from "the source file was
genuinely deleted" (Result::SourceNotExits) - the latter is exactly what SyncFile treats as safe
to act on when delete_in_target() is enabled.

## UnreachableSshSourceDoesNotDeleteExistingTarget

Same defect, but asserting the actual user-visible consequence: with delete_in_target enabled, a
transient SSH failure on the source side must not delete the (perfectly intact) destination
file.

## TwoWaySyncContentComparisonTest

Local-filesystem fixture for Fehler 5: content-only comparison cannot tell which side changed,
only that they differ. With two-way sync, blindly reporting SourceModified silently discards a
destination-only edit; the fix falls back to mtime as a tie-breaker, but only when two_way_sync
is set, since one-way sync must keep always reporting SourceModified (SyncFile does not act on
DestModified there unless copy_when_target_newer() is set, so anything else would stop the file
from syncing at all).

## references-must-outlive

FileDifference stores source/dest as references: they must outlive fd(), so bind them to stable
named locals rather than passing temporaries.

## one-way-keeps-sourcemodified

Destination is newer, but this is one-way sync (two_way_sync defaults to false): must keep
reporting SourceModified, or SyncFile would stop syncing this file entirely (see class comment
above).

## TwoWaySyncPreservesNewerDestinationEditInsteadOfOverwritingIt

End-to-end version of the first test: with the destination genuinely newer, two-way sync must
copy destination -> source, preserving the edit, instead of silently overwriting the destination
with the older source content.

## DestSizeIsReadEvenWhenSourceIsMissing

Fehler 1/round 7 follow-up: FileDifference's "else if" short-circuited the destination read
entirely whenever the source was missing, so dest_size() silently stayed at 0 for a
SourceNotExits result - even though the destination file genuinely exists with real content.
copy_dest_to_source() only fires when fd.dest_size() > 0, so two_way_sync picking up a file that
exists only in the destination silently did nothing (logged "Leere Zieldatei wurde nicht
kopiert" for a file that was never actually empty).
