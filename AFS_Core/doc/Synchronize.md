# Synchronize

Module comment for Synchronize.h / Synchronize.cpp.

## role_

Defaults keep the test-only default constructor usable; the real constructor sets both from its
role argument.

## ssh_target_dir_cache_

Only touched by the worker thread (sync_item), so no lock is needed.

## item_reachable_

Last known reachability per item name, so an item that stays unreachable is reported once
instead of once per pass - to the log as an error and to the backup proof. The log line is what
the GUI's summary box after a single run picks up; CheckPath's own lines are Info. Same thread
as above.

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
