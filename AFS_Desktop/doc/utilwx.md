# utilwx

Module comment for utilwx.h.

## confirm

One confirmation dialog for every destructive command (delete a synchronisation or an SSH
account, overwrite a file). The buttons name the action and "Abbrechen" instead of Yes/No,
and the cancelling button is the default, so Return never destroys anything: that is what
the platform guidelines ask for, and it keeps the dialogs alike. The return codes remain
wxID_YES/wxID_NO, so the GUI tests answer the dialog as before. The cancel label comes from
the program's own catalogue rather than the wx stock label: the program's language can
differ from the system locale the wx catalogue follows.

## field_label

Field labels end with a colon, as the platform guidelines put it, and the colon comes from
the catalogue ("label_colon") because French sets a space before it. The bare texts stay
without it: the same keys ("name", "host", ...) are also column headers in the lists, where
a colon would be wrong.

## place_of

What the lists of connections show beside the name: the host of an SSH connection, the address
of a WebDAV connection, "Google Drive" for a Google account, whose address is the same for
everyone.
