/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "EventPump.h"
#include "ForegroundApp.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include <gtest/gtest.h>
#include <wx/app.h>
#include <wx/evtloop.h>
#include <wx/init.h>
#include <wx/stopwatch.h>

namespace {

constexpr int PLACEMENT_QUIET_MILLISECONDS = 500;
constexpr int PLACEMENT_TIMEOUT_MILLISECONDS = 3000;

void wait_until_placed(wxWindow& window)  // [GuiTestMain.md#wait_until_placed]
{
    wxStopWatch total;
    wxRect placed;
    for (wxStopWatch quiet; quiet.Time() < PLACEMENT_QUIET_MILLISECONDS && total.Time() < PLACEMENT_TIMEOUT_MILLISECONDS; guitest::pump(50)) {
        const wxRect current(window.GetScreenPosition(), window.GetSize());
        if (current != placed)
            quiet.Start();
        placed = current;
    }
}

}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);  // [GuiTestMain.md#gtest-switches-removed-first]
    guitest::GuiTestSandbox::create();
    if (!wxEntryStart(argc, argv))
        return EXIT_FAILURE;
    guitest::GuiTestSandbox::isolate_preferences();  // [GuiTestSandbox.md#isolate_preferences]
    guitest::ModalDialogWatcher::instance();  // [GuiTestMain.md#watcher-before-start]
    int result = EXIT_FAILURE;
    if (wxTheApp->CallOnInit()) {
        wxEventLoop main_loop;  // [GuiTestMain.md#active-event-loop-needed]
        wxEventLoopActivator activate_main_loop(&main_loop);
#ifdef _MACOS
        guitest::disable_window_animations();  // [ForegroundApp.md#disable_window_animations]
        guitest::bring_to_foreground();  // [ForegroundApp.md#bring_to_foreground]
#endif
        wait_until_placed(*wxTheApp->GetTopWindow());
        result = RUN_ALL_TESTS();
        wxTheApp->OnExit();
    }
    wxEntryCleanup();
    guitest::GuiTestSandbox::remove();
    return result;
}
