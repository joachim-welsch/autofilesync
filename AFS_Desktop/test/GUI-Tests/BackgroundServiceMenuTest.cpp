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
#include "BackgroundService.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"
#include <wx/log.h>
#include <wx/menu.h>

using utilwx::tul;

namespace guitest {
namespace {

wxMenu* backgroundServiceMenu(wxFrame& frame)  // [BackgroundServiceMenuTest.md#backgroundServiceMenu]
{
    wxMenu* menu = nullptr;
    frame.GetMenuBar()->FindItem(tul(id::BackgroundServiceStart), &menu);
    return menu;
}

void openMenu(wxFrame& frame, wxMenu* menu)  // [BackgroundServiceMenuTest.md#openMenu]
{
    wxLogNull noPopupOnFailedQuery;  // [BackgroundServiceMenuTest.md#no-message-box-on-failed-query]
    wxMenuEvent opening(wxEVT_MENU_OPEN, 0, menu);
    frame.GetEventHandler()->ProcessEvent(opening);
}

}

TEST_F(GuiTest, HintergrunddienstmenueZeigtObDerDienstLaeuft)  // [BackgroundServiceMenuTest.md#HintergrunddienstmenueZeigtObDerDienstLaeuft]
{
    using State = BackgroundService::State;
    wxMenu* menu = backgroundServiceMenu(mainWindow());
    ASSERT_NE(menu, nullptr) << "Das Menue Hintergrunddienst wurde nicht gefunden.";
    openMenu(mainWindow(), menu);
    EXPECT_FALSE(menu->IsEnabled(tul(id::BackgroundServiceStart)) && menu->IsEnabled(tul(id::BackgroundServiceStop)))
        << "Start und Stop sind gleichzeitig anwaehlbar, der Zustand des Dienstes also nicht ablesbar.";
    const State state = BackgroundService::state();  // [BackgroundServiceMenuTest.md#state-checked-after-symptom]
    EXPECT_NE(state, State::Unknown)
        << "Der Zustand des Hintergrunddienstes ist auf dieser Plattform nicht feststellbar.";

    const bool startEnabled = menu->IsEnabled(tul(id::BackgroundServiceStart));
    const bool stopEnabled = menu->IsEnabled(tul(id::BackgroundServiceStop));
    const wxString stateLine = menu->GetLabel(tul(id::BackgroundServiceState));  // [BackgroundServiceMenuTest.md#state-line]
    EXPECT_FALSE(menu->IsEnabled(tul(id::BackgroundServiceState)))
        << "Die Zustandszeile benennt nur den Zustand und darf nicht anwaehlbar sein.";
    switch (state) {
    case State::Running:
        EXPECT_FALSE(startEnabled) << "Ein laufender Dienst darf nicht noch einmal startbar sein.";
        EXPECT_TRUE(stopEnabled) << "Ein laufender Dienst muss sich stoppen lassen.";
        EXPECT_EQ(stateLine, AFS_Core::translation::translate("bs_state_running"));
        break;
    case State::Stopped:
        EXPECT_TRUE(startEnabled) << "Ein gestoppter Dienst muss sich starten lassen.";
        EXPECT_FALSE(stopEnabled) << "Ein gestoppter Dienst kann nicht noch einmal gestoppt werden.";
        EXPECT_EQ(stateLine, AFS_Core::translation::translate("bs_state_stopped"));
        break;
    case State::NotInstalled:
        EXPECT_FALSE(startEnabled) << "Ein nicht installierter Dienst ist nicht startbar.";
        EXPECT_FALSE(stopEnabled) << "Ein nicht installierter Dienst ist nicht stoppbar.";
        EXPECT_EQ(stateLine, AFS_Core::translation::translate("bs_state_not_installed"));
        break;
    case State::Unknown:
        break; // already reported above; both commands are offered on purpose
    }
}

}
