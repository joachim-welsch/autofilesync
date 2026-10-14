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
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "RestoredBackgroundService.h"
#include "UserActions.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"
#include <wx/log.h>
#include <wx/menu.h>

using utilwx::tul;

namespace guitest {
namespace {

wxMenu* background_service_menu(wxFrame& frame)  // [BackgroundServiceMenuTest.md#background_service_menu]
{
    wxMenu* menu = nullptr;
    frame.GetMenuBar()->FindItem(tul(id::BackgroundServiceStart), &menu);
    return menu;
}

void open_menu(wxFrame& frame, wxMenu* menu)  // [BackgroundServiceMenuTest.md#open_menu]
{
    wxLogNull no_popup_on_failed_query;  // [BackgroundServiceMenuTest.md#no-message-box-on-failed-query]
    wxMenuEvent opening(wxEVT_MENU_OPEN, 0, menu);
    frame.GetEventHandler()->ProcessEvent(opening);
}

}

TEST_F(GuiTest, HintergrunddienstmenueZeigtObDerDienstLaeuft)  // [BackgroundServiceMenuTest.md#HintergrunddienstmenueZeigtObDerDienstLaeuft]
{
    using State = BackgroundService::State;
    wxMenu* menu = background_service_menu(main_window());
    ASSERT_NE(menu, nullptr) << "Das Menue Hintergrunddienst wurde nicht gefunden.";
    open_menu(main_window(), menu);
    EXPECT_FALSE(menu->IsEnabled(tul(id::BackgroundServiceStart)) && menu->IsEnabled(tul(id::BackgroundServiceStop)))
        << "Start und Stop sind gleichzeitig anwaehlbar, der Zustand des Dienstes also nicht ablesbar.";
    const State state = BackgroundService::state();  // [BackgroundServiceMenuTest.md#state-checked-after-symptom]
    EXPECT_NE(state, State::Unknown)
        << "Der Zustand des Hintergrunddienstes ist auf dieser Plattform nicht feststellbar.";

    const bool start_enabled = menu->IsEnabled(tul(id::BackgroundServiceStart));
    const bool stop_enabled = menu->IsEnabled(tul(id::BackgroundServiceStop));
    const wxString state_line = menu->GetLabel(tul(id::BackgroundServiceState));  // [BackgroundServiceMenuTest.md#state-line]
    EXPECT_FALSE(menu->IsEnabled(tul(id::BackgroundServiceState)))
        << "Die Zustandszeile benennt nur den Zustand und darf nicht anwaehlbar sein.";
    switch (state) {
    case State::Running:
        EXPECT_FALSE(start_enabled) << "Ein laufender Dienst darf nicht noch einmal startbar sein.";
        EXPECT_TRUE(stop_enabled) << "Ein laufender Dienst muss sich stoppen lassen.";
        EXPECT_EQ(state_line, AFS_Core::translation::translate("bs_state_running"));
        break;
    case State::Stopped:
        EXPECT_TRUE(start_enabled) << "Ein gestoppter Dienst muss sich starten lassen.";
        EXPECT_FALSE(stop_enabled) << "Ein gestoppter Dienst kann nicht noch einmal gestoppt werden.";
        EXPECT_EQ(state_line, AFS_Core::translation::translate("bs_state_stopped"));
        break;
    case State::NotInstalled:
        EXPECT_FALSE(start_enabled) << "Ein nicht installierter Dienst ist nicht startbar.";
        EXPECT_FALSE(stop_enabled) << "Ein nicht installierter Dienst ist nicht stoppbar.";
        EXPECT_EQ(state_line, AFS_Core::translation::translate("bs_state_not_installed"));
        break;
    case State::Unknown:
        break; // already reported above; both commands are offered on purpose
    }
}

TEST_F(GuiTest, HintergrunddienstLaesstSichOhneAdministratorrechteStoppenUndStarten)  // [BackgroundServiceMenuTest.md#HintergrunddienstLaesstSichOhneAdministratorrechteStoppenUndStarten]
{
    using State = BackgroundService::State;
    if (!service_controllable())
        GTEST_SKIP() << "Kein installierter Hintergrunddienst, den dieser Benutzer ohne Administratorrechte steuern darf.";
    const RestoredBackgroundService restored;
    const bool was_running = BackgroundService::state() == State::Running;

    activate_menu_item(main_window(), tul(was_running ? id::BackgroundServiceStop : id::BackgroundServiceStart));
    pump();
    EXPECT_EQ(BackgroundService::state(), was_running ? State::Stopped : State::Running) << "Der erste Befehl hat den Dienst nicht umgeschaltet.";

    activate_menu_item(main_window(), tul(was_running ? id::BackgroundServiceStart : id::BackgroundServiceStop));
    pump();
    EXPECT_EQ(BackgroundService::state(), was_running ? State::Running : State::Stopped) << "Der zweite Befehl hat den Dienst nicht zurückgeschaltet.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answered_messages().empty()) << "Start oder Stopp zeigten eine Meldung.";
}

}
