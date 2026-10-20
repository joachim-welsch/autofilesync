# PreviewDialog

Module comment for PreviewDialog.h / PreviewDialog.cpp.

## PreviewDialog

Shows what a run would do (SyncPlan.md#SyncPlan) before anything is done: one row per
planned step with the action, the synchronisation it belongs to, the file and its size, a
summary line, and the choice to run the synchronisation now or to close. The dialog itself
starts nothing; SyncInterface starts the ordinary single run when it returns wxID_OK, so the
run after a preview is exactly the run "Einmal synchronisieren" would make. The OK button
carries the run's own label, because "OK" on a dialog that is about to delete files says
nothing about what it does.

## shown_path

The first screenshot showed two columns of absolute paths cut off with "…" before the file
name, the one part the user needs. A row therefore names the synchronisation and the file
relative to that synchronisation's root - the source root for a copy or move, the target root
for a deletion or a copy back, since that is where the file lives. A file item has no root
but the file, so its name is shown. Anything that does not match - an item renamed between
pass and dialog, say - falls back to the full path rather than hiding it.

## deletions-in-red

The deletions - in the target, and in the source for a two-way sync whose state saw the
target side deleted - are the reason the preview exists: they are the only steps that lose
data, so they stand out in red and show no size - a deletion moves none.

## close-is-the-default

Return in this dialog must not start a run that deletes files; the default button is
"Schließen", and "Jetzt synchronisieren" is a deliberate click.

## summary

The counts and the volume come from the plan, which counts every step even when the list is
cut at SyncPlan.md#MAX_STEPS, so the summary is always complete. The number of deletions is
named on its own, for the same reason as above.

## notes

The messages the preview pass logged - an unreachable source, a content conflict - shown
below the list as the single run shows them in its box afterwards
(SyncInterface.md#show_run_messages). A box before the dialog would hide the context the
messages belong to.

## renames

A rename step (SyncPlan.md#PlannedAction) names both paths, old and new, relative to the root
of the side they are on, joined by an arrow, and shows no size: nothing is transferred. That is
the point of the step - the user sees that the move was recognised and no copy is coming.
