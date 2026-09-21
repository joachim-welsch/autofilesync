# SshAccountsTest

Module comment for SshAccountsTest.cpp.

## addAccount

The direct route Datei → Neu → SSH-Verbindung; AddSshAccountTest covers the other route
through the "SSH-Verbindungen" list dialog. No server is contacted when an account is created,
so these tests run without one.

## delete-warning

The confirmation warns when a synchronisation refers to the account by its name prefix; both
cases are tested, with and without such a synchronisation, so the warning is neither missing
nor always there.

## rename

The in-place rename in the list has to carry the name into every synchronisation that uses it
(Config::update_ssh_account_references); the target path of the item is what the user would
otherwise have to fix by hand.

## rejected-input

Validate() rejects a short name, a name with characters outside letters, digits, underscore
and dash, and a duplicate, each with a "Validierungsfehler" box that leaves the dialog open;
the driver cancels it (SshAccountDialogDriver.md#rejected-input-keeps-the-dialog-open). A
colon is caught by the character rule before the dedicated colon rule, so that is the message
the user sees.
