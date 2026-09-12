# ModalDialogWatcher

Module comment for ModalDialogWatcher.h / ModalDialogWatcher.cpp.

## never-destroyed

Deliberately never destroyed: unregistering happens in the destructor and would touch wxWidgets'
static hook list, whose lifetime relative to a static in this translation unit is not defined.

## queued-not-called

Queued instead of called here: the modal event loop is not running yet and the dialog is not on
screen, but wxUIActionSimulator needs a mapped window to send its events to.

## ModalDialogWatcher

The single modal hook of the test run. A dialog a test announced through expect() is driven from
inside its own modal loop; every other modal dialog is cancelled and recorded, so an unforeseen
popup fails the test instead of blocking it forever.
