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
