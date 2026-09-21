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

## answer

A message box is answered from inside the hook, before it is shown: on Windows wxMessageBox is
the native MessageBox with its own modal loop, in which a queued action would never run. The
hook returns the button id instead, wxWidgets then skips showing the box and reports that id
as the result. The message text is kept so the test can assert what the user would have read.

## choosePath

The native file and directory dialogs cannot be driven by the simulator, but their ShowModal()
goes through the modal hook on every platform, and a return code other than wxID_NONE skips
the native window. The path is set on the dialog object before the hook answers OK, which is
what the caller reads through GetPath() afterwards - the same as a user choosing that path.

## nameOf

A native file or directory dialog carries no window title - the text the user sees as its
caption is its "message" - so that is what an expectation for one is matched against.
