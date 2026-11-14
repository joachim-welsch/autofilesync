# SshAccountsDialog

Module comment for SshAccountsDialog.h / SshAccountsDialog.cpp.

## standard-buttons

OK and Cancel come from CreateStdDialogButtonSizer, like in every other dialog: the sizer
puts them in the order the platform expects (Cancel left of OK on macOS and GTK) and gives
them the stock labels, which the hand-built buttons did not.

## utf8-list

FromUTF8: the configuration holds these fields as UTF-8; the implicit conversion would read them
in the ANSI codepage.

## deletion-warns

Unlike a rename, a deletion has no valid account left to point items at - the best this dialog
can do is warn instead of letting them silently start failing every sync pass.

## account-references-updated

Keeps sync items using this account under its old name from silently failing every pass
afterward (see Config::updateSshAccountReferences).

## webdav-accounts

The list holds SSH and WebDAV connections alike (SshAccount.md#url_). The second column shows
the host of the one and the address of the other, and editing opens the dialog that fits the
kind, since the two ask for different things.
