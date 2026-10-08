# SshAccountsDialog

Module comment for SshAccountsDialog.h / SshAccountsDialog.cpp.

## utf8-list

FromUTF8: the configuration holds these fields as UTF-8; the implicit conversion would read them
in the ANSI codepage.

## deletion-warns

Unlike a rename, a deletion has no valid account left to point items at - the best this dialog
can do is warn instead of letting them silently start failing every sync pass.

## account-references-updated

Keeps sync items using this account under its old name from silently failing every pass
afterward (see Config::updateSshAccountReferences).
