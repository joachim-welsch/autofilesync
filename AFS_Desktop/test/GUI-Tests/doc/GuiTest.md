# GuiTest

Module comment for GuiTest.h / GuiTest.cpp.

## entries_of

Names and sizes are enough to spot every change a synchronisation could cause: a copied, deleted
or overwritten file changes one of them.

## autosave-undone

The application writes every change to the sandbox's configuration file at once, so a test
that adds an item or changes a setting has changed the file by the time it ends. TearDown puts
the content captured in SetUp back and re-reads it: that undoes the changes in the file and in
memory alike and clears the "unsaved changes" flag that a failed write would leave behind, which
would otherwise make the application ask for saving while it shuts down.

## list-view-restored

TearDown switches back to the list view: a grid or tree test that fails in the middle, or
that just does not switch back, would otherwise leave every later test without the list
they read, and the fixture is the one place that reaches each test whether it passed or
not.

## both-branches-wxstring

Both branches have to be a wxString: mixing it with a string literal makes the conditional
ambiguous, since either side converts to the other.

## main_window

Typed as MainWindow, not wxFrame: the sync tests read the state of SyncInterface and the menu
through it, and it is the only top-level window the application ever creates. Tests that only
need a frame still get one - MainWindow is a wxFrame.

## GuiTest

Base of every GUI test. It enforces the rule from CLAUDE.md that after a GUI test the
configuration and the source and target file systems are in the same state as before: TearDown()
drops what the test changed in memory and then compares the whole state against the snapshot
taken in SetUp().

## SlowGuiTest

Same fixture, different name: a test that waits for the local language model takes a minute
or two. Every one of them is a chat test, so the fixture is disabled like ChatGuiTest.

## ChatGuiTest

The assistant's quick tests - menu, help sections, empty input, new conversation, hiding the
window - under a name of their own; every SlowGuiTest is a chat test, and these are the rest.
Both fixtures carry gtest's `DISABLED_` prefix, so no run - neither run_gui_tests.sh nor ctest,
whatever filter it is given - executes the chat: it needs the language model (ModelLocation.md)
and a developer working on the synchronisation should not wait for it or fail without it.
`--gtest_also_run_disabled_tests --gtest_filter='DISABLED_ChatGuiTest.*:DISABLED_SlowGuiTest.*'`
runs the chat alone.

## left-open-reported

A dialog the watcher had to cancel after its timeout (ModalDialogWatcher.md#left-open) has
usually already failed the test with the message of the action that went wrong; this report
adds the dialog's name and says that the dialog was abandoned, which the earlier message does
not tell.

## settle-between-tests

TearDown settles the window server (ForegroundApp.md#settle_window_server) before it restores the
state, so the create-and-destroy of modal dialogs in one test does not leave the next test's dialog
stuck off screen on macOS. The progress dialog is handled separately, by reuse in the application
(SyncInterface.md#reuse-progress-dialog), because it is destroyed through wx's deferred deletion,
which a native cycle here does not flush.

## sync-state-removed

A two-way or move-tracking item writes its sync state next to the sandbox configuration
(SyncState.md#sync_state_dir). Restoring the configuration file does not remove it, and a later
test that creates an item of the same name over the same paths would inherit the earlier
test's view of the sides - with a state, a missing file reads as a deletion. The whole state
directory is therefore removed after every test; the sandbox's own configuration has no item
that keeps one.
