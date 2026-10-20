# ProgressDialog

Module comment for ProgressDialog.h / ProgressDialog.cpp.

## lock-released-before-wait

Released before the wait loop below: wxSafeYield() there pumps the event queue on this same
(GUI) thread, and any CallAfter()-queued call into can_be_closed() that also locks mutex_ would
deadlock against a still-held, non-recursive lock.

## cancel-waits-here

requestStop() only signals; the actual wait happens here, not inside SyncControler::stop_sync()'s
std::this_thread::sleep_for() loop, which blocks the GUI thread outright - clicking Cancel used
to freeze the whole window for as long as the current copy step took to notice the stop request.

## gui-thread-only

gauge_ is a wx widget and may only be touched on the GUI thread; callers (e.g. the sync worker
thread) get marshalled there via CallAfter().

## no-yield-under-lock

on_timer_update() used to call wxYield() while holding mutex_. The timer runs on the GUI thread
from the event loop, so the yield gained nothing - but a click on Cancel dispatched inside it
ran on_cancel() on the same thread, which locks the same non-recursive mutex: a deadlock the GUI
tests hit in about every second run. The event loop pumps the queue between timer ticks anyway.

## file-progress

The bar runs over all files of the run with FILE_STEPS steps per file: the files done before
the current one (the filenumber of the latest message, counted before that file) plus the share
of the current file already copied (SyncControler::copied_bytes() against the size the message
carried). One bar thus moves inside a huge file and across many small ones without switching
scales. A file that produces no message, because it is unchanged and skipped, leaves the bar
where it was until the next copy, and the end of the run fills it (can_be_closed).

## reuse

reset() returns the dialog to its freshly-opened state - full range, empty bar and label, timer
running, not yet closable - so one instance serves every run. The dialog is kept alive and reused
instead of destroyed and recreated because on macOS destroying a top-level window and showing the
next one right after leaves that next window off the screen: AppKit holds it as visible and key,
but the window server never lists it, and from then on every later window stays off screen too.
See SyncInterface.md#reuse-progress-dialog.

## gauge-never-overflows

The range comes from the counting pass, the value from the run, and the run may do more than
the count foresaw: files that appeared between the two passes, or a conflict that copies two
files for one counted (SyncFile.md#conflict-copies). A wxGauge value past its range trips a
wxWidgets assertion, and the assertion's dialog processes events while on_timer_update() holds
mutex_ - SyncInterface's timer then fires, finishes the run and asks can_be_closed(), which
takes the same mutex: a deadlock that showed as a run that never ended. The value is therefore
clamped; a bar that stands at full while the last extra file copies is the honest picture.
