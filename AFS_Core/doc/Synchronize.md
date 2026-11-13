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
and must not pay for 100 000 entries. One-way items carry no state between passes; one that
deletes in the target gets a state for the pass only (pass-only-state). The number of files a
kept state knows goes into the item's LastRun, so the assistant can say that the two-way sync is
tracking deletions (ConfigSummary.md#known-files); a pass-only state tracks nothing between
passes and leaves LastRun without that number.

## pass-only-state

A one-way item with delete_in_target empties its target as readily as a two-way item when the
source turns up empty - a music folder on a disk that is not mounted, mirrored to the phone. So
it runs with an empty state that is never read or written (SyncState.md#pass-only): its
deletions are put off to the end of the pass like those of a two-way item
(SyncFile.md#deferred-deletions) and pass the same mass-deletion check, approval from the
preview included (SyncBase.md#mass-deletion). Without delete_in_target a one-way pass deletes
nothing and needs no state.

## complete-pass

forget_unseen() drops every entry the pass did not visit, and that is only right after a pass
that visited everything: a stopped pass or one that threw leaves the unvisited files in the
state, or the next pass would take every one of them for new on both sides.

## mass-deletion-approved

The one run that may carry out a mass deletion (SyncBase.md#mass-deletion) is the single run
the user starts from the preview, and the GUI says so when it creates that Synchronize. The
flag is per instance and reaches the pass through the state's notebook
(SyncState.md#mass-deletion-approval); the daemon creates its Synchronize without it.

## notes

What a pass has to tell the user whatever the message filter in the settings says - so far the
refusal of a mass deletion (SyncBase.md#mass-deletion). The log and the proof get the line as
well, but the box after "Einmal synchronisieren" shows log lines only through the user's
Fehler/Warnung/Info filters, and a user who switched the errors off would never learn why
nothing was deleted. The notes travel to the controller next to the plan and the unreachable
items; note() is static like report_copied_bytes(), since the sync classes know no instance.

## ssh-sessions

Every SSH operation of the sync classes used to open its own session - connect, host key,
login, SFTP channel - so a tree of a thousand files cost thousands of logins per pass, and a
server with fail2ban could take the daemon for an attacker. A pass now runs inside an
SshSessionScope (SftpConnection.md#SshSessionScope): the first operation on an account logs
in, every later one of the same pass on this thread shares that session, and the scope's end
closes it. One scope per pass rather than per item or per process: a pass is the unit the
user thinks in, and a session that is reopened every minute heals from a dropped line by
itself. The scope sits next to the plan guard, so the check of the paths and the inspectors
it uses share the session too.

## watcher_

A continuous run watches the local directories of every enabled item that asks for it
(SyncItem.md#watch_changes_) with a ChangeWatcher built once at the start of the run: the
source and the target as long as they are local, the parent directory for a file item, the
whole tree for a tree item. SSH sides cannot be watched and stay with the tick. Items that
appear in the configuration while the run goes on are not watched - the GUI adds items to a
running configuration, the daemon restarts the run on a reload - and run on their tick, which
is what they did before. The log names every watched item once, so a user who wonders why a
change arrived at once, or did not, finds the answer there.

## due

An unwatched item is due when its pause has elapsed (pause_elapsed), as it always was. A
watched item is due when a change was seen and has settled - no further event for
SETTLE_TIME, so that a program writing a file in several steps is not caught halfway - or
when WATCHED_SCAN_INTERVAL has passed since its last pass, the safety net for the events a
watcher can miss: an overflowed queue, a network drive that reports nothing, a change that
came in during the pass. The pause does not apply to a watched item: "at once" is what the
option promises, and the loop's one-second tick plus the settle time already keep a file that
is saved every few seconds from running a pass per keystroke. After the pass the watcher
forgets only the changes from before the pass started. A change that arrives during the pass
cannot be told apart from the pass's own copies into the watched target, and the pass may have
listed its directory before it came, so it leads to one more pass once it has settled. That
pass finds nothing to do, writes nothing and so ends the chain; forgetting everything instead
would leave a file saved during a pass to the ten-minute scan. An unwatched item - no local
side, the option off, the platform's limit reached - is due by its pause alone, as before.

## device-items

An item whose source or target is a phone (Config.md#device-accounts) runs only while the
phone is serving its SFTP server (DeviceLink.md#serve): at any other time the phone is
unreachable by design, not by fault, so the item is skipped silently rather than reported as
unreachable on every tick. While the phone serves, the item ignores its pause - the phone
decides when a pass happens, and it asked for one. After each such item the link is told
(DeviceLink::item_passed), so that it can report progress and send the phone its "done".

## device-filter

A run created for one device (DeviceLink.md#serve, when no other run is active) passes that
device's items only: the phone is waiting with its server open, and a full pass over a slow
backup disk would make it wait for nothing of its own.

## finished

Set at the very end of operator()(): a run without a controller - the device pass - has no
other way to tell its owner that the thread is done, and joining it would block the progress
messages the link sends meanwhile.
