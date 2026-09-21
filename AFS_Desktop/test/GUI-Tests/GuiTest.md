# GuiTest

Module comment for GuiTest.h / GuiTest.cpp.

## entriesOf

Names and sizes are enough to spot every change a synchronisation could cause: a copied, deleted
or overwritten file changes one of them.

## reread-clears-unsaved-changes

Re-reading the untouched file undoes every in-memory change and clears the "unsaved changes"
flag, which would otherwise make the application ask for saving while it shuts down.

## both-branches-wxstring

Both branches have to be a wxString: mixing it with a string literal makes the conditional
ambiguous, since either side converts to the other.

## mainWindow

Typed as MainWindow, not wxFrame: the sync tests read the state of SyncInterface and the menu
through it, and it is the only top-level window the application ever creates. Tests that only
need a frame still get one - MainWindow is a wxFrame.

## GuiTest

Base of every GUI test. It enforces the rule from CLAUDE.md that after a GUI test the
configuration and the source and target file systems are in the same state as before: TearDown()
drops what the test changed in memory and then compares the whole state against the snapshot
taken in SetUp().
