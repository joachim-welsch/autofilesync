#include "GuiTest.h"
#include "BackgroundService.h"
#include "id.h"
#include "utilwx.h"
#include <wx/log.h>
#include <wx/menu.h>

using util::tul;

namespace guitest {
namespace {

// The menu bar owns what Menu::create() appended, so the item lookup yields the
// same wxMenu that MainWindow::onMenuOpen compares the opened menu against.
wxMenu* backgroundServiceMenu(wxFrame& frame)
{
    wxMenu* menu = nullptr;
    frame.GetMenuBar()->FindItem(tul(id::BackgroundServiceStart), &menu);
    return menu;
}

// A menu bar cannot be opened headlessly (see UserActions.cpp's
// activateMenuItem()), so this sends the event that opening it produces.
// MainWindow refreshes the two commands from there, because the service can be
// started and stopped outside of AutoFileSync as well.
void openMenu(wxFrame& frame, wxMenu* menu)
{
    // Asking for the state runs launchctl resp. systemctl. Where that command
    // is missing, the failure belongs in the returned state - not in a message
    // box that wxLogGui would flush during a later test and fail that one.
    wxLogNull noPopupOnFailedQuery;
    wxMenuEvent opening(wxEVT_MENU_OPEN, 0, menu);
    frame.GetEventHandler()->ProcessEvent(opening);
}

}

// Start and Stop describe opposite states of one service, so which of them the
// menu offers is the only clue it gives about whether the service is running.
// On macOS both were offered: the state was queried with systemctl, which does
// not exist there, and an undeterminable state deliberately offers both.
TEST_F(GuiTest, HintergrunddienstmenueZeigtObDerDienstLaeuft)
{
    using State = BackgroundService::State;
    wxMenu* menu = backgroundServiceMenu(mainWindow());
    ASSERT_NE(menu, nullptr) << "Das Menue Hintergrunddienst wurde nicht gefunden.";
    openMenu(mainWindow(), menu);
    EXPECT_FALSE(menu->IsEnabled(tul(id::BackgroundServiceStart)) && menu->IsEnabled(tul(id::BackgroundServiceStop)))
        << "Start und Stop sind gleichzeitig anwaehlbar, der Zustand des Dienstes also nicht ablesbar.";
    // Checked after the symptom so that a failing run names both: what the user
    // sees in the menu, and the undeterminable state that leads to it.
    const State state = BackgroundService::state();
    EXPECT_NE(state, State::Unknown)
        << "Der Zustand des Hintergrunddienstes ist auf dieser Plattform nicht feststellbar.";

    const bool startEnabled = menu->IsEnabled(tul(id::BackgroundServiceStart));
    const bool stopEnabled = menu->IsEnabled(tul(id::BackgroundServiceStop));
    // The state line carries the answer where neither command does: a service
    // that is not installed offers neither of them.
    const wxString stateLine = menu->GetLabel(tul(id::BackgroundServiceState));
    EXPECT_FALSE(menu->IsEnabled(tul(id::BackgroundServiceState)))
        << "Die Zustandszeile benennt nur den Zustand und darf nicht anwaehlbar sein.";
    switch (state) {
    case State::Running:
        EXPECT_FALSE(startEnabled) << "Ein laufender Dienst darf nicht noch einmal startbar sein.";
        EXPECT_TRUE(stopEnabled) << "Ein laufender Dienst muss sich stoppen lassen.";
        EXPECT_TRUE(stateLine.Contains(L"läuft")) << "Zustandszeile: " << stateLine;
        break;
    case State::Stopped:
        EXPECT_TRUE(startEnabled) << "Ein gestoppter Dienst muss sich starten lassen.";
        EXPECT_FALSE(stopEnabled) << "Ein gestoppter Dienst kann nicht noch einmal gestoppt werden.";
        EXPECT_TRUE(stateLine.Contains(L"gestoppt")) << "Zustandszeile: " << stateLine;
        break;
    case State::NotInstalled:
        EXPECT_FALSE(startEnabled) << "Ein nicht installierter Dienst ist nicht startbar.";
        EXPECT_FALSE(stopEnabled) << "Ein nicht installierter Dienst ist nicht stoppbar.";
        EXPECT_TRUE(stateLine.Contains(L"nicht installiert")) << "Zustandszeile: " << stateLine;
        break;
    case State::Unknown:
        break; // already reported above; both commands are offered on purpose
    }
}

}
