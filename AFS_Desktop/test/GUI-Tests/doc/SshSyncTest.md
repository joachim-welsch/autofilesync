# SshSyncTest

Module comment for SshSyncTest.cpp.

## account-named-localhost

An SSH path is "account:path", and the account is looked up by that prefix; the account is
named after the host here so that a path reads the way the help shows it.

## status_of

The dialog exposes no accessor for its status line; it is the static text that does not start
like the connection header (ssh_connection_info up to its first placeholder). The status is what
tells the user why a directory did not open.

## start-path

The target box is given ":<probe directory>" before the ssh button is pressed: with no account
before the colon the button asks for one, and the path after the colon is where the directory
dialog starts, so the test does not have to navigate from the root.

## ssh-button

The ssh button chains three dialogs - the synchronisation's, the account selection and the
remote directory - and the expectations are announced in the order they open. The test ends by
cancelling the synchronisation dialog: the composed target in its text box is the result.

## sync-to-server

The end-to-end case of the SSH help: a local directory synchronised to a remote one through the
GUI, and the file read back through SFTP. The account is created through the dialog too, so
the test covers the way a user sets this up.
