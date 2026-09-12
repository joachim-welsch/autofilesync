#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include <gtest/gtest.h>
#include <wx/app.h>
#include <wx/evtloop.h>
#include <wx/init.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);  // [GuiTestMain.md#gtest-switches-removed-first]
    guitest::GuiTestSandbox::create();
    if (!wxEntryStart(argc, argv))
        return EXIT_FAILURE;
    guitest::ModalDialogWatcher::instance();  // [GuiTestMain.md#watcher-before-start]
    int result = EXIT_FAILURE;
    if (wxTheApp->CallOnInit()) {
        wxEventLoop mainLoop;  // [GuiTestMain.md#active-event-loop-needed]
        wxEventLoopActivator activateMainLoop(&mainLoop);
        result = RUN_ALL_TESTS();
        wxTheApp->OnExit();
    }
    wxEntryCleanup();
    guitest::GuiTestSandbox::remove();
    return result;
}
