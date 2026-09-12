# GuiTestMain

Module comment for GuiTestMain.cpp.

## gtest-switches-removed-first

Removes the --gtest_* switches before App's own command line parser, which rejects anything it
does not know, gets to see them.

## watcher-before-start

Registered before the application starts: a message box during start-up would otherwise wait for
a user who is not there.

## active-event-loop-needed

gtest drives main(), not wxApp::MainLoop() - so wxEventLoopBase::GetActive() is null for the
whole run except while a ShowModal() dialog's own nested loop is on the stack. Without an active
loop, wxYield() (what pump() in EventPump.cpp relies on) falls back to a fresh throwaway
wxEventLoop on every single call. That is enough to drain the queue for mouse events, but
synthetic keyboard input into a non-modal Show()'d window (e.g. ChatDialog) never turned into
text - see GuiTest.LogFrageWirdImChatBeantwortet, which failed until a real, continuously active
loop existed for the run.
