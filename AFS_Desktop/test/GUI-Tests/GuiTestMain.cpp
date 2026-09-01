#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include <gtest/gtest.h>
#include <wx/app.h>
#include <wx/evtloop.h>
#include <wx/init.h>

int main(int argc, char** argv)
{
    // Removes the --gtest_* switches before App's own command line parser, which
    // rejects anything it does not know, gets to see them.
    ::testing::InitGoogleTest(&argc, argv);
    guitest::GuiTestSandbox::create();
    if (!wxEntryStart(argc, argv))
        return EXIT_FAILURE;
    // Registered before the application starts: a message box during start-up
    // would otherwise wait for a user who is not there.
    guitest::ModalDialogWatcher::instance();
    int result = EXIT_FAILURE;
    if (wxTheApp->CallOnInit()) {
        // gtest drives main(), not wxApp::MainLoop() - so wxEventLoopBase::GetActive()
        // is null for the whole run except while a ShowModal() dialog's own nested
        // loop is on the stack. Without an active loop, wxYield() (what pump() in
        // EventPump.cpp relies on) falls back to a fresh throwaway wxEventLoop on
        // every single call. That is enough to drain the queue for mouse events, but
        // synthetic keyboard input into a non-modal Show()'d window (e.g. ChatDialog)
        // never turned into text - see GuiTest.LogFrageWirdImChatBeantwortet, which
        // failed until a real, continuously active loop existed for the run.
        wxEventLoop mainLoop;
        wxEventLoopActivator activateMainLoop(&mainLoop);
        result = RUN_ALL_TESTS();
        wxTheApp->OnExit();
    }
    wxEntryCleanup();
    guitest::GuiTestSandbox::remove();
    return result;
}
