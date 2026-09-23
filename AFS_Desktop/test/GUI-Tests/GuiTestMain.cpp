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
