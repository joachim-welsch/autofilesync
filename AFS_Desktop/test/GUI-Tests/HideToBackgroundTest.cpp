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
#include "GuiTest.h"
#include "EventPump.h"
#include "UserActions.h"
#include "MainWindow.h"
#include "id.h"
#include "utilwx.h"
#include <wx/taskbar.h>

using utilwx::tul;

namespace guitest {
namespace {

constexpr int HIDE_TIMEOUT_MILLISECONDS = 500;  // [HideToBackgroundTest.md#HIDE_TIMEOUT_MILLISECONDS]

}

TEST_F(GuiTest, ImHintergrundBleibtDasFensterOhneTrayErreichbar)  // [HideToBackgroundTest.md#ImHintergrundBleibtDasFensterOhneTrayErreichbar]
{
    activate_menu_item(main_window(), tul(id::View_Background));
    const bool hidden = wait_until([this] { return !main_window().IsShown(); }, HIDE_TIMEOUT_MILLISECONDS);
    main_window().Iconize(false);  // [HideToBackgroundTest.md#window-restored]
    main_window().Show(true);
    EXPECT_EQ(hidden, wxTaskBarIcon::IsAvailable())
        << "Das Fenster darf nur versteckt werden, wenn ein Tray-Symbol es zurückholen kann.";
}

}
