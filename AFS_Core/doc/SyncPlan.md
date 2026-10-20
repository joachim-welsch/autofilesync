# SyncPlan

Module comment for SyncPlan.h / SyncPlan.cpp.

## SyncPlan

What a synchronisation would do right now, without doing it: the preview the user asks for
before a run that may delete. A preview pass (SyncType::Preview, see
Synchronize.md#preview) walks the items exactly like a counting pass, which already
touches nothing, but lets SyncFile decide per file as a real run would and records the
decision here instead of carrying it out. The counts per action and the volume cover every
step, so the summary is right even when the list is cut.

## PlannedAction

The five things a run does to a file, as the user needs to tell them apart: a copy into the
target, a move (copy, then the source is deleted - del_aftercopy), a copy back into the
source (two_way_sync), a deletion in the target (delete_in_target, or a two-way sync whose
state says the source side was deleted) and a deletion in the source (a two-way sync whose
state says the target side was deleted, see SyncFile.md#state). The deletions are the steps
that lose data, so they are kinds of their own rather than flags, and deletion() names them
for the preview. A rename in the target or in the source (SyncFile.md#renames) is a sixth and
seventh kind: the step's source is the old path and its target the new one, both on the same
side, and it moves no bytes, which rename() tells the volume count and the preview.

## UnreachableItem

An item a pass skipped because its source or target was not reachable - an external disk that
is not plugged in is the usual case - as every pass hands it to the controller next to the
plan (Synchronize.md#unreachable_). The preview needs the name: without it an empty plan would
read as "source and target match", which is the opposite of the truth. The run needs the log
line and whether the item asks for a warning (SyncItem.md#warn_if_unreachable_), so the box
after a single run can show the warning in the same words as the log, whatever the message
filter in the settings says.

## MAX_STEPS

A preview over a fresh target lists every file of the tree. A list control with hundreds of
thousands of rows is no use to anyone and takes long to fill, so the list stops at
MAX_STEPS while the counts go on; not_listed() tells the dialog how many it does not show.

## SyncPlanGuard

Same mechanism as CopyLog.md#SyncReportGuard: the worker thread installs the plan it is
filling in a thread-local pointer for the duration of the pass, and SyncFile reports through
plan_step() without knowing who collects. The guard restores the previous pointer, so a plan
can never outlive its pass.

## planning

Whether a plan is being collected on this thread. SyncBase::sync_file counts files blindly
when it only counts - the count is a range for the progress bar and need not be exact - but
a preview must compare every file, so it uses this to take the full path through SyncFile.
