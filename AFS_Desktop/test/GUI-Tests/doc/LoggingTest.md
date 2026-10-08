# LoggingTest

Module comment for LoggingTest.cpp.

## applyLogging

A message box the dialog raises on OK has to be queued after the dialog's own expectation
and before the menu command: the watcher answers in order of appearance.

## RestoredLogging

Logging is a process-wide singleton with the log file open; GuiTest::TearDown re-reads the
configuration but the test must not leave the log pointing into a SandboxTree that is about
to disappear, so every value the dialog can change is restored explicitly.

## logLineDaysAgo

LogCut reads the timestamp at the start of every line in the format Logging writes it
("%Y-%m-%d %H:%M:%S.mmm"); a line written with a date ten days back is what a shorter
retention has to remove, one from today what it has to keep.

## KuerzereHistorieKuerztLogSofort

The dialog cuts the log only when the retention gets shorter, so the log is first pointed
at the prepared file with a long retention and then shortened in a second visit.

## email-validation

The E-Mail page is validated on OK and the dialog stays open behind the message box; the
driver cancels it (LoggingDialogDriver.md#rejected-input-keeps-the-dialog-open), so the
proof is the message and that the notification stayed off. No mail is ever sent: the
address and server are examples.

## BackupNachweisWirdNachLaufGeschrieben

The proof path is set through the logging dialog, the option through the synchronisation's
dialog, and the run through the menu - the full path a user takes. The file name is asked
from copy_log_file_path so a renaming of the file stays a single change.
