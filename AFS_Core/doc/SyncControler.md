# SyncControler

Module comment for SyncControler.h / SyncControler.cpp.

## set_sync_finished

GUI updates (ProgressDialog) must happen on the GUI thread; onTimer() polls sync_finished()
there and closes/deletes the dialog itself.

## request_stop

Non-blocking half of stop_sync(): signals the worker thread to stop and returns immediately,
letting the caller wait however suits it (e.g. ProgressDialog::OnCancel() needs to keep pumping
the GUI event queue while waiting, which this class has no business knowing about).
