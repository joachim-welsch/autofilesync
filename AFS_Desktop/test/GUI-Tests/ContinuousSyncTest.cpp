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
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

constexpr int PASS_TIMEOUT_MILLISECONDS = 10000;  // a pass plus the 1 s pause plus the 1 s timer tick, with room for a slow disk
constexpr int GRACE_MILLISECONDS = 3000;  // longer than pause plus tick: a stopped sync that still ran would have copied by then

class RunningSync {  // [ContinuousSyncTest.md#RunningSync]
public:
    explicit RunningSync(MainWindow& window): window_(window) {}
    ~RunningSync()
    {
        if (window_.sync_interface().is_continuously_syncing()) {
            activate_menu_item(window_, tul(id::StopSync));
            wait_until_sync_finished(window_);
        }
    }
private:
    MainWindow& window_;
};

SyncItemInput dir_sync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

wxString sync_state(MainWindow& window)
{
    return window.GetStatusBar()->GetStatusText(1);
}

bool copied(const SandboxTree& tree, const char* file)
{
    return wait_until([&tree, file] { return tree.exists(file); }, PASS_TIMEOUT_MILLISECONDS);
}

}

TEST_F(GuiTest, StartUndStoppLaufenMehrerePassesUndStoppenSauber)  // [ContinuousSyncTest.md#StartUndStoppLaufenMehrerePassesUndStoppenSauber]
{
    const SandboxTree tree(GuiTestSandbox::root() / "fortlaufend", {{"quelle/erste.txt", "1"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-fortlaufend")));
    const RunningSync running(main_window());

    activate_menu_item(main_window(), tul(id::StartSync));
    ASSERT_TRUE(copied(tree, "ziel/erste.txt")) << "Der erste Durchlauf hat nicht kopiert.";
    tree.write("quelle/zweite.txt", "2");
    ASSERT_TRUE(copied(tree, "ziel/zweite.txt")) << "Kein zweiter Durchlauf: die nachgelegte Datei wurde nicht kopiert.";

    activate_menu_item(main_window(), tul(id::StopSync));
    ASSERT_TRUE(wait_until_sync_finished(main_window())) << "Nach Stopp kam die Verarbeitung nicht zur Ruhe.";
    tree.write("quelle/dritte.txt", "3");
    pump(GRACE_MILLISECONDS);

    EXPECT_FALSE(tree.exists("ziel/dritte.txt")) << "Nach Stopp wurde weiter kopiert.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::StartSync)));
    EXPECT_FALSE(sync_menu_item_enabled(main_window(), tul(id::StopSync)));
}

TEST_F(GuiTest, StatuszeileZeigtZustandDerSynchronisation)  // [ContinuousSyncTest.md#StatuszeileZeigtZustandDerSynchronisation]
{
    const SandboxTree tree(GuiTestSandbox::root() / "statuszeile", {{"quelle/erste.txt", "1"}, {"ziel/", ""}});
    SyncItemInput input = dir_sync(tree, "guitest-statuszeile");
    input.options.sync_cycle_seconds = 60;
    input.options.watch_changes = false;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    const RunningSync running(main_window());
    ASSERT_EQ(sync_state(main_window()), translation::translate("state_stopped"));

    activate_menu_item(main_window(), tul(id::StartSync));
    ASSERT_TRUE(copied(tree, "ziel/erste.txt"));
    const wxString paused = translation::translate("state_paused", {L"*"});  // the pause length is a wildcard
    ASSERT_TRUE(wait_until([this, &paused] { return sync_state(main_window()).Matches(paused); }, PASS_TIMEOUT_MILLISECONDS))
        << "Nach dem Durchlauf zeigt die Statuszeile nicht die Pause, sondern: " << sync_state(main_window());

    activate_menu_item(main_window(), tul(id::StopSync));
    ASSERT_TRUE(wait_until_sync_finished(main_window()));
    EXPECT_EQ(sync_state(main_window()), translation::translate("state_stopped"));
}

TEST_F(GuiTest, StatuszeileZeigtWartenAufAenderungen)  // [ContinuousSyncTest.md#StatuszeileZeigtWartenAufAenderungen]
{
    const SandboxTree tree(GuiTestSandbox::root() / "beobachtet", {{"quelle/erste.txt", "1"}, {"ziel/", ""}});
    SyncItemInput input = dir_sync(tree, "guitest-beobachtet");
    input.options.watch_changes = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    const RunningSync running(main_window());

    activate_menu_item(main_window(), tul(id::StartSync));
    ASSERT_TRUE(copied(tree, "ziel/erste.txt"));
    const wxString watching = translation::translate("state_watching");
    EXPECT_TRUE(wait_until([this, &watching] { return sync_state(main_window()) == watching; }, PASS_TIMEOUT_MILLISECONDS))
        << "Nach dem Durchlauf zeigt die Statuszeile nicht das Warten auf Änderungen, sondern: " << sync_state(main_window());
}

TEST_F(GuiTest, StartMenueIstGesperrtWaehrendDesLaufs)
{
    const SandboxTree tree(GuiTestSandbox::root() / "gesperrt", {{"quelle/erste.txt", "1"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-gesperrt")));
    const RunningSync running(main_window());
    ASSERT_TRUE(sync_menu_item_enabled(main_window(), tul(id::StartSync)));
    ASSERT_TRUE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce)));
    ASSERT_FALSE(sync_menu_item_enabled(main_window(), tul(id::StopSync)));

    activate_menu_item(main_window(), tul(id::StartSync));
    ASSERT_TRUE(copied(tree, "ziel/erste.txt"));

    EXPECT_FALSE(sync_menu_item_enabled(main_window(), tul(id::StartSync))) << "\"Start\" ist während des Laufs nicht gesperrt.";
    EXPECT_FALSE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce))) << "\"Einmal synchronisieren\" ist während des Laufs nicht gesperrt.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::StopSync)));
}

TEST_F(GuiTest, ToolbarStartUndStoppSchaltenUm)  // [ContinuousSyncTest.md#ToolbarStartUndStoppSchaltenUm]
{
    const SandboxTree tree(GuiTestSandbox::root() / "werkzeugleiste", {{"quelle/erste.txt", "1"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-werkzeugleiste")));
    const RunningSync running(main_window());
    wxToolBar* toolbar = main_window().GetToolBar();
    ASSERT_NE(toolbar, nullptr);
    const int toggle = tul(id::StartSync);  // one persistent tool toggles between start and stop [Toolbar.md#single-toggle-tool]
    ASSERT_NE(toolbar->FindById(toggle), nullptr) << "Die Werkzeugleiste hat keinen Start-Stopp-Knopf.";
    ASSERT_EQ(toolbar->FindById(tul(id::StopSync)), nullptr) << "Die Werkzeugleiste legt keinen zweiten Knopf an.";
    const wxString start_help = toolbar->GetToolShortHelp(toggle);

    ASSERT_TRUE(activate_toolbar_tool(main_window(), toggle));
    ASSERT_TRUE(copied(tree, "ziel/erste.txt")) << "Der Start-Knopf der Werkzeugleiste hat nichts gestartet.";
    const wxString stop_help = toolbar->GetToolShortHelp(toggle);
    EXPECT_NE(stop_help, start_help) << "Der Knopf wechselte während des Laufs nicht auf Stopp.";
    EXPECT_EQ(toolbar->FindById(tul(id::StopSync)), nullptr) << "Der Knopf wurde ausgetauscht statt umgeschaltet.";

    ASSERT_TRUE(activate_toolbar_tool(main_window(), toggle));
    ASSERT_TRUE(wait_until_sync_finished(main_window()));
    EXPECT_EQ(toolbar->GetToolShortHelp(toggle), start_help) << "Nach Stopp wechselte der Knopf nicht auf Start zurück.";
}

}
