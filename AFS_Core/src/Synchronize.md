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

Counting passes touch nothing and have nothing to prove; without a directory to write to there
is nowhere to put a proof either.

## snapshot-vs-config

i is this pass's snapshot copy (see operator()()), not the item held by Config - update
last_sync there so the next pass's snapshot sees it.
