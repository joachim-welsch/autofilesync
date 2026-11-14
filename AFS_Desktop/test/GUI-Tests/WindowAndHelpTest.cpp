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
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "BackgroundService.h"
#include "TaskBarIcon.h"
#include "Version.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"
#include <wx/html/helpctrl.h>
#include <filesystem>
#include <fstream>
#include <memory>

using utilwx::tul;

namespace guitest {
namespace {

class ShownAgain {  // [WindowAndHelpTest.md#ShownAgain]
public:
    explicit ShownAgain(MainWindow& window): window_(window) {}
    ~ShownAgain()
    {
        window_.Show();
        pump();
    }
private:
    MainWindow& window_;
};

bool activate_task_bar_menu_item(TaskBarIcon& icon, const wxString& label)  // [WindowAndHelpTest.md#activate_task_bar_menu_item]
{
    const std::unique_ptr<wxMenu> menu(icon.CreatePopupMenu());
    if (menu == nullptr)
        return false;
    for (const wxMenuItem* item : menu->GetMenuItems()) {
        if (item->GetItemLabelText() != label)
            continue;
        wxCommandEvent event(wxEVT_MENU, item->GetId());
        icon.ProcessEvent(event);
        pump();
        return true;
    }
    return false;
}

wxHtmlHelpFrame* help_frame(MainWindow& window)
{
    return dynamic_cast<wxHtmlHelpController&>(window.get_help_controller()).GetFrame();
}

class ClosedHelp {  // [WindowAndHelpTest.md#ClosedHelp]
public:
    explicit ClosedHelp(MainWindow& window): window_(window) {}
    ~ClosedHelp()
    {
        if (wxHtmlHelpFrame* frame = help_frame(window_))
            frame->Close(true);
        pump();
    }
private:
    MainWindow& window_;
};

}

TEST_F(GuiTest, AnsichtHintergrundVerstecktFensterUndTaskleisteZeigtEsWieder)
{
    if (!wxTaskBarIcon::IsAvailable())  // [WindowAndHelpTest.md#needs-a-tray]
        GTEST_SKIP() << "Ohne Infobereich wird das Fenster nicht versteckt.";
    const ShownAgain shown_again(main_window());
    ASSERT_TRUE(main_window().IsShown());
    ASSERT_NE(main_window().task_bar_icon(), nullptr) << "Es gibt kein Symbol im Infobereich.";

    activate_menu_item(main_window(), tul(id::View_Background));
    pump();
    EXPECT_FALSE(main_window().IsShown()) << "\"Ansicht → Hintergrund\" hat das Fenster nicht versteckt.";

    ASSERT_TRUE(activate_task_bar_menu_item(*main_window().task_bar_icon(), wxMenuItem::GetLabelText(AFS_Core::translation::translate("taskbar_restore"))))
        << "Das Menü des Infobereichs hat keinen Eintrag zum Wiederherstellen.";
    EXPECT_TRUE(main_window().IsShown()) << "Das Fenster kam über das Menü des Infobereichs nicht zurück.";
}

TEST_F(GuiTest, TaskbarDoppelklickZeigtFenster)
{
    const ShownAgain shown_again(main_window());
    TaskBarIcon* icon = main_window().task_bar_icon();
    ASSERT_NE(icon, nullptr);
    main_window().Show(false);
    pump();
    ASSERT_FALSE(main_window().IsShown());

    wxTaskBarIconEvent double_click(wxEVT_TASKBAR_LEFT_DCLICK, icon);
    icon->ProcessEvent(double_click);
    pump();

    EXPECT_TRUE(main_window().IsShown()) << "Der Doppelklick auf das Symbol im Infobereich zeigt das Fenster nicht.";
}

TEST_F(GuiTest, InfoDatenNennenVersionUndLizenz)  // [WindowAndHelpTest.md#about]
{
    const wxAboutDialogInfo info = MainWindow::about_info();
    EXPECT_EQ(info.GetVersion(), AFS_VERSION_STRING) << "Die Info-Daten nennen die Version nicht.";
    EXPECT_TRUE(info.GetCopyright().Contains("Joachim Welsch")) << "Die Info-Daten nennen den Rechteinhaber nicht: " << info.GetCopyright();
    EXPECT_TRUE(info.GetDescription().Contains("GPL")) << "Die Info-Daten nennen die Lizenz nicht: " << info.GetDescription();
    EXPECT_TRUE(info.GetDescription().Contains("welsch-software.com")) << "Die Info-Daten nennen die Website nicht: " << info.GetDescription();
}

TEST_F(GuiTest, InfoDialogZeigtVersion)  // [WindowAndHelpTest.md#about]
{
#ifdef __WXOSX__
    GTEST_SKIP() << "macOS zeigt das native Info-Fenster, das kein wxDialog ist.";
#endif
    ModalDialogWatcher::instance().answer(AFS_Core::translation::translate("about_title"), wxID_OK);

    activate_menu_item(main_window(), tul(id::About));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Der Info-Dialog erschien nicht.";
    const wxString& about = ModalDialogWatcher::instance().answered_messages().front();
    EXPECT_TRUE(about.Contains(AFS_VERSION_STRING)) << "Der Info-Dialog nennt die Version nicht: " << about;
    EXPECT_TRUE(about.Contains("Joachim Welsch")) << "Der Info-Dialog nennt den Rechteinhaber nicht: " << about;
    EXPECT_TRUE(about.Contains("GPL")) << "Der Info-Dialog nennt die Lizenz nicht: " << about;
    EXPECT_EQ(about, AFS_Core::translation::translate("about_text",
        {wxString(AFS_NAME_STRING).ToStdWstring(), wxString(AFS_VERSION_STRING).ToStdWstring(), wxGetOsDescription().ToStdWstring()}))
        << "Der Info-Dialog weicht vom Katalogtext ab (Umlaute?): " << about;
}

TEST_F(GuiTest, HilfeOeffnetHilfebuch)  // [WindowAndHelpTest.md#help]
{
    const ClosedHelp closed_help(main_window());
    ASSERT_TRUE(help_frame(main_window()) == nullptr || !help_frame(main_window())->IsShown());

    activate_menu_item(main_window(), tul(id::Help));
    ASSERT_TRUE(wait_until([this] { return help_frame(main_window()) != nullptr && help_frame(main_window())->IsShown(); }))
        << "Das Hilfefenster erschien nicht.";
    EXPECT_TRUE(help_frame(main_window())->GetTitle().Contains("AutoFileSync") || !help_frame(main_window())->GetTitle().empty());
}

TEST_F(GuiTest, WerkzeugleisteHilfeOeffnetHilfebuch)  // [WindowAndHelpTest.md#help]
{
    const ClosedHelp closed_help(main_window());

    ASSERT_TRUE(activate_toolbar_tool(main_window(), tul(id::Help))) << "Die Werkzeugleiste hat keinen Hilfe-Knopf.";

    EXPECT_TRUE(wait_until([this] { return help_frame(main_window()) != nullptr && help_frame(main_window())->IsShown(); }))
        << "Der Hilfe-Knopf der Werkzeugleiste öffnete die Hilfe nicht.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answered_messages().empty()) << "Der Hilfe-Knopf zeigte einen Dialog statt der Hilfe.";
}

TEST_F(GuiTest, HintergrunddienstStartOhneDienstZeigtFehlerbox)  // [WindowAndHelpTest.md#background-service]
{
    if (BackgroundService::state() != BackgroundService::State::NotInstalled)
        GTEST_SKIP() << "Der Hintergrunddienst ist installiert; Start und Stopp ohne ihn lassen sich hier nicht prüfen.";
    ModalDialogWatcher::instance().answer(AFS_Core::translation::translate("bs_caption"), wxID_OK);

    activate_menu_item(main_window(), tul(id::BackgroundServiceStart));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Der fehlende Dienst blieb ohne Meldung.";
    EXPECT_FALSE(ModalDialogWatcher::instance().answered_messages().front().empty());
}

}
