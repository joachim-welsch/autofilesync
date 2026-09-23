/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
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
