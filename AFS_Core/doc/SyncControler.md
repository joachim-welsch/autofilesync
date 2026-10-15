# SyncControler

Module comment for SyncControler.h / SyncControler.cpp.

## set_sync_finished

GUI updates (ProgressDialog) must happen on the GUI thread; onTimer() polls sync_finished()
there and closes/deletes the dialog itself.

## request_stop

Non-blocking half of stop_sync(): signals the worker thread to stop and returns immediately,
letting the caller wait however suits it (e.g. ProgressDialog::OnCancel() needs to keep pumping
the GUI event queue while waiting, which this class has no business knowing about).

## plan_

The plan the last preview pass collected, handed over by the worker thread before it marks
the run as finished and taken by the GUI thread afterwards (take_plan()), so the two never
touch it at the same time - the same hand-over as count_files_.
