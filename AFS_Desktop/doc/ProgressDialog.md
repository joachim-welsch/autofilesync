# ProgressDialog

Module comment for ProgressDialog.h / ProgressDialog.cpp.

## lock-released-before-wait

Released before the wait loop below: wxSafeYield() there pumps the event queue on this same
(GUI) thread, and any CallAfter()-queued call into e.g. setProgress()/canBeClosed() that also
locks mutex_ would deadlock against a still-held, non-recursive lock.

## cancel-waits-here

requestStop() only signals; the actual wait happens here, not inside SyncControler::stopSync()'s
std::this_thread::sleep_for() loop, which blocks the GUI thread outright - clicking Cancel used
to freeze the whole window for as long as the current copy step took to notice the stop request.

## gui-thread-only

gauge_ is a wx widget and may only be touched on the GUI thread; callers (e.g. the sync worker
thread) get marshalled there via CallAfter().

## no-yield-under-lock

onTimerUpdate() used to call wxYield() while holding mutex_. The timer runs on the GUI thread
from the event loop, so the yield gained nothing - but a click on Cancel dispatched inside it
ran OnCancel() on the same thread, which locks the same non-recursive mutex: a deadlock the GUI
tests hit in about every second run. The event loop pumps the queue between timer ticks anyway.
