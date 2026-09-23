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
        if (window_.syncInterface().isContinuouslySyncing()) {
            activateMenuItem(window_, tul(id::StopSync));
            waitUntilSyncFinished(window_);
        }
    }
private:
    MainWindow& window_;
};

SyncItemInput dirSync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

wxString syncState(MainWindow& window)
{
    return window.GetStatusBar()->GetStatusText(1);
}

bool copied(const SandboxTree& tree, const char* file)
{
    return waitUntil([&tree, file] { return tree.exists(file); }, PASS_TIMEOUT_MILLISECONDS);
}

}

TEST_F(GuiTest, StartUndStoppLaufenMehrerePassesUndStoppenSauber)  // [ContinuousSyncTest.md#StartUndStoppLaufenMehrerePassesUndStoppenSauber]
{
    const SandboxTree tree(GuiTestSandbox::root() / "fortlaufend", {{"quelle/erste.txt", "1"}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-fortlaufend")));
    const RunningSync running(mainWindow());

    activateMenuItem(mainWindow(), tul(id::StartSync));
    ASSERT_TRUE(copied(tree, "ziel/erste.txt")) << "Der erste Durchlauf hat nicht kopiert.";
    tree.write("quelle/zweite.txt", "2");
    ASSERT_TRUE(copied(tree, "ziel/zweite.txt")) << "Kein zweiter Durchlauf: die nachgelegte Datei wurde nicht kopiert.";

    activateMenuItem(mainWindow(), tul(id::StopSync));
    ASSERT_TRUE(waitUntilSyncFinished(mainWindow())) << "Nach Stopp kam die Verarbeitung nicht zur Ruhe.";
    tree.write("quelle/dritte.txt", "3");
    pump(GRACE_MILLISECONDS);

    EXPECT_FALSE(tree.exists("ziel/dritte.txt")) << "Nach Stopp wurde weiter kopiert.";
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::StartSync)));
    EXPECT_FALSE(syncMenuItemEnabled(mainWindow(), tul(id::StopSync)));
}

TEST_F(GuiTest, StatuszeileZeigtZustandDerSynchronisation)  // [ContinuousSyncTest.md#StatuszeileZeigtZustandDerSynchronisation]
{
    const SandboxTree tree(GuiTestSandbox::root() / "statuszeile", {{"quelle/erste.txt", "1"}, {"ziel/", ""}});
    SyncItemInput input = dirSync(tree, "guitest-statuszeile");
    input.options.syncCycleSeconds = 60;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));
    const RunningSync running(mainWindow());
    ASSERT_EQ(syncState(mainWindow()), "Synchronisation gestoppt");

    activateMenuItem(mainWindow(), tul(id::StartSync));
    ASSERT_TRUE(copied(tree, "ziel/erste.txt"));
    ASSERT_TRUE(waitUntil([this] { return syncState(mainWindow()).StartsWith("Pausiert"); }, PASS_TIMEOUT_MILLISECONDS))
        << "Nach dem Durchlauf zeigt die Statuszeile nicht die Pause, sondern: " << syncState(mainWindow());
    EXPECT_TRUE(syncState(mainWindow()).Matches(L"Pausiert * in * s")) << syncState(mainWindow());

    activateMenuItem(mainWindow(), tul(id::StopSync));
    ASSERT_TRUE(waitUntilSyncFinished(mainWindow()));
    EXPECT_EQ(syncState(mainWindow()), "Synchronisation gestoppt");
}

TEST_F(GuiTest, StartMenueIstGesperrtWaehrendDesLaufs)
{
    const SandboxTree tree(GuiTestSandbox::root() / "gesperrt", {{"quelle/erste.txt", "1"}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-gesperrt")));
    const RunningSync running(mainWindow());
    ASSERT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::StartSync)));
    ASSERT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::SyncOnce)));
    ASSERT_FALSE(syncMenuItemEnabled(mainWindow(), tul(id::StopSync)));

    activateMenuItem(mainWindow(), tul(id::StartSync));
    ASSERT_TRUE(copied(tree, "ziel/erste.txt"));

    EXPECT_FALSE(syncMenuItemEnabled(mainWindow(), tul(id::StartSync))) << "\"Start\" ist während des Laufs nicht gesperrt.";
    EXPECT_FALSE(syncMenuItemEnabled(mainWindow(), tul(id::SyncOnce))) << "\"Einmal synchronisieren\" ist während des Laufs nicht gesperrt.";
    EXPECT_TRUE(syncMenuItemEnabled(mainWindow(), tul(id::StopSync)));
}

TEST_F(GuiTest, ToolbarStartUndStoppSchaltenUm)  // [ContinuousSyncTest.md#ToolbarStartUndStoppSchaltenUm]
{
    const SandboxTree tree(GuiTestSandbox::root() / "werkzeugleiste", {{"quelle/erste.txt", "1"}, {"ziel/", ""}});
    ASSERT_TRUE(addSyncItem(mainWindow(), dirSync(tree, "guitest-werkzeugleiste")));
    const RunningSync running(mainWindow());
    wxToolBar* toolbar = mainWindow().GetToolBar();
    ASSERT_NE(toolbar, nullptr);
    ASSERT_NE(toolbar->FindById(tul(id::StartSync)), nullptr) << "Die Werkzeugleiste hat vor dem Start keinen Start-Knopf.";
    ASSERT_EQ(toolbar->FindById(tul(id::StopSync)), nullptr);

    ASSERT_TRUE(activateToolbarTool(mainWindow(), tul(id::StartSync)));
    ASSERT_TRUE(copied(tree, "ziel/erste.txt")) << "Der Start-Knopf der Werkzeugleiste hat nichts gestartet.";
    EXPECT_EQ(toolbar->FindById(tul(id::StartSync)), nullptr) << "Der Start-Knopf blieb während des Laufs stehen.";
    ASSERT_NE(toolbar->FindById(tul(id::StopSync)), nullptr) << "Die Werkzeugleiste zeigt während des Laufs keinen Stopp-Knopf.";

    ASSERT_TRUE(activateToolbarTool(mainWindow(), tul(id::StopSync)));
    ASSERT_TRUE(waitUntilSyncFinished(mainWindow()));
    EXPECT_NE(toolbar->FindById(tul(id::StartSync)), nullptr) << "Nach Stopp kam der Start-Knopf nicht zurück.";
    EXPECT_EQ(toolbar->FindById(tul(id::StopSync)), nullptr);
}

}
