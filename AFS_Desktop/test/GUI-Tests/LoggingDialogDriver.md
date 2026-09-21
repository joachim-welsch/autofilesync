# LoggingDialogDriver

Module comment for LoggingDialogDriver.h / LoggingDialogDriver.cpp.

## LoggingInput

Every field is optional: a test names only what it wants changed, the rest keeps the values
the dialog opened with - which are the current settings, so the same driver serves a test
that changes one value and one that provokes a validation message. The log level is given
as the entry's text ("Debug" ... "FatalError"), the way the user reads it in the list.

## enterValues

Each field reports itself: without that, a failed interaction is indistinguishable from a
broken feature - both end as "not taken over".

## email-active-first

"Aktiv" comes first on the E-Mail page: it enables the other fields, and typing into a
disabled field would silently do nothing.

## cancel-instead-of-hanging

Leaving the dialog open would hang the test in its modal loop; the cancel result tells the test
that the interaction, not the feature, is what failed.

## rejected-input-keeps-the-dialog-open

OK runs Validate(), and an active E-Mail page without address, server or sender leaves the
dialog open behind a message box. A test that expects that answers the box through
ModalDialogWatcher::answer(); the driver then finds the dialog still open and cancels it, so
the test does not hang and the settings stay untouched.

## fillLoggingDialog

Fills the "Logging-Einstellungen" dialog the way a user would and confirms it with OK. Pass the
result to ModalDialogWatcher::expect().
