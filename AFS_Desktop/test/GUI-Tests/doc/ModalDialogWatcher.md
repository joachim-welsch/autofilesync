# ModalDialogWatcher

Module comment for ModalDialogWatcher.h / ModalDialogWatcher.cpp.

## never-destroyed

Deliberately never destroyed: unregistering happens in the destructor and would touch wxWidgets'
static hook list, whose lifetime relative to a static in this translation unit is not defined.

## queued-not-called

Queued instead of called here: the modal event loop is not running yet and the dialog is not on
screen, but wxUIActionSimulator needs a mapped window to send its events to.

## dialogs_entered

Counts every modal dialog the hook saw, expected or not, so a caller can tell whether the
action it just performed opened one - see UserActions.md#settle-after-dialog.

## ModalDialogWatcher

The single modal hook of the test run. A dialog a test announced through expect() is driven from
inside its own modal loop; every other modal dialog is cancelled and recorded, so an unforeseen
popup fails the test instead of blocking it forever.

## answer

A message box is answered from inside the hook, before it is shown: on Windows wxMessageBox is
the native MessageBox with its own modal loop, in which a queued action would never run. The
hook returns the button id instead, wxWidgets then skips showing the box and reports that id
as the result. The message text is kept so the test can assert what the user would have read.

## choose_path

The native file and directory dialogs cannot be driven by the simulator, but their ShowModal()
goes through the modal hook on every platform, and a return code other than wxID_NONE skips
the native window. The path is set on the dialog object before the hook answers OK, which is
what the caller reads through GetPath() afterwards - the same as a user choosing that path.

## name_of

A native file or directory dialog carries no window title - the text the user sees as its
caption is its "message" - so that is what an expectation for one is matched against.

## gtk-chooser-is-asynchronous

Under GTK the dialog object does not hold the path by itself. wxFileDialog::GetPath asks the
GTK file chooser, which applies a folder set through SetPath only after loading it through
GIO - a few event loop iterations later - and returns nothing until then; the hook therefore
pumps until the chooser reports the path before it answers. wxDirDialog::GetPath reads the
m_path of wxDirDialogBase, but wxGTK's SetPath override only tells the chooser and skips the
base, so the base implementation is called explicitly. On Windows and macOS both dialogs
store the path in the base class and return it right away.

## left-open

A dialog the action leaves open would block the test run forever in its modal loop: the test
code after the menu activation only runs once ShowModal has returned. Actions end up like that
when a click they do not verify goes astray, or when an ASSERT inside the action returns from
it before the closing click. After the action the hook therefore waits for the dialog to be
hidden - EndModal hides it on every platform, IsModal() stays true on Windows until the modal
loop has unwound - and cancels it when the timeout has passed, recording the dialog's name so
that TearDown reports why the test failed. The timeout is longer than any wait inside an action:
a dialog that an OK click closes a few seconds later, after the work its handler does, is not
cancelled.
