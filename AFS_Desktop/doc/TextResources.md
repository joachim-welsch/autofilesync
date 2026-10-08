# TextResources

Module comment for TextResources.h.

## txt

The titles of the dialogs that more than one place opens - the menu handlers of MainWindow, the
grid and the SSH accounts dialog - and that the GUI tests use to recognise a dialog: the
ModalDialogWatcher matches a modal dialog by its title, so title and matcher have to come from
one function, in whichever language the test runs. Functions, not constants: a title can only be
translated once the catalogue is bound, which happens in App::OnInit(), after static
initialisation. Texts that a single place uses go straight to translation::translate() there.
