# Synchronize

Module comment for Synchronize.h / Synchronize.cpp.

## role_

Defaults keep the test-only default constructor usable; the real constructor sets both from its
role argument.

## ssh_target_dir_cache_

Only touched by the worker thread (sync_item), so no lock is needed.

## item_reachable_

Last known reachability per item name, so an item that stays unreachable is reported once
instead of once per pass. CheckPath's own lines are Info. Same thread as above.

## unreachable_

The items this pass skipped as unreachable, handed to the controller at the end next to the
plan (SyncPlan.md#UnreachableItem). An unreachable side is not an error in itself - a backup
disk is often not plugged in on purpose - so the item decides
(SyncItem.md#warn_if_unreachable_): with the warning on, the log line is a warning and the
backup proof records the run that could not happen, because the user declared the drive must
be there; with it off, an info line is all that remains and the proof stays silent. The
daemon has only the log, so the level is decided here. The list is filled where the
once-per-instance guard above lets the line through, so a continuous run does not grow it
pass after pass.

## stop

Public, not only for SyncControler: the cancel tests call it from a second thread while a
SyncFile is copying, which is exactly what the GUI's Stop button does through the controller.
It only raises the process-wide flag that every copy loop checks between two blocks; nothing
waits here.

## constructor-clears-stop

stop_ is already false by the time the constructor returns; see there.

## no-proof-for-counting-passes

Counting passes touch nothing and have nothing to prove or report; without a directory to write
to there is nowhere to put a proof either.

## record_last_run

Every real pass collects its report, with or without a proof: whether source and target were
reachable, how many errors there were and the first of them go into the live item in Config,
where the assistant reads them (ConfigSummary). Not persisted - it describes this program's
runs. The proof is written as before, only for items that ask for one.

## snapshot-vs-config

i is this pass's snapshot copy (see operator()()), not the item held by Config - update
last_sync there so the next pass's snapshot sees it.

## copied_bytes_

Bytes of the file being copied, stored by the worker after every block and read by the
progress dialog on its timer: one atomic in place of a message per block, which the message
stack would have to keep and the dialog pop. set_progress_message() resets it, so the count
always belongs to the file the latest message named.

## preview

A SyncType::Preview pass is a counting pass that records what it would have done: it
touches nothing, writes no proof and leaves last_sync alone (touches_nothing()), and
sync_items() installs plan_ for its duration so SyncFile reports its decisions into it (see
SyncPlan.md#SyncPlanGuard). The pass hands the plan to the controller next to the file count
when it ends; the GUI takes it from there on its timer, the same way it takes the count.

## preview-ignores-the-pause

A preview answers "what would a run do now", so it must not skip the items whose pause
since the last run has not elapsed: right after a run the user would see "nothing to do"
for an item that has plenty to do as soon as the pause is over. Only the real runs honour
the pause.

## sync-state

A two-way item runs with the state of its last pass (SyncState.md#SyncState): the keeper
reads it before the pass - or starts an empty one for the first pass - hands it to the sync
classes and writes it back when it goes out of scope, which is also the way out when the pass
throws, so what the pass recorded up to the exception is kept. Only passes that touch
something write; the preview reads the state so that it plans the deletions a run would make,
and the counting pass does not load it at all - the count is a range for the progress bar
and must not pay for 100 000 entries. One-way items carry no state. The number of files the
state knows goes into the item's LastRun, so the assistant can say that the two-way sync is
tracking deletions (ConfigSummary.md#known-files).

## complete-pass

forget_unseen() drops every entry the pass did not visit, and that is only right after a pass
that visited everything: a stopped pass or one that threw leaves the unvisited files in the
state, or the next pass would take every one of them for new on both sides.
