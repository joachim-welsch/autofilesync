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
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "id.h"
#include "utilwx.h"
#include <fstream>
#include <sstream>

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

const wxString OPEN_TITLE = L"Konfigurationsdatei öffnen";
const wxString SAVE_AS_TITLE = L"Name für die Konfigurationsdatei wählen";
const wxString NOT_SAVED_TITLE = "Die Daten wurden nicht gespeichert.";
const wxString ITEM_NAME = "guitest-konfiguration";

std::string readFile(const fs::path& pathname)
{
    std::ostringstream content;
    content << std::ifstream(pathname).rdbuf();
    return content.str();
}

class RestoredConfigFile {  // [ConfigFileTest.md#RestoredConfigFile]
public:
    explicit RestoredConfigFile(MainWindow& window)
        : window_(window), pathname_(Config::instance().pathname()), content_(readFile(GuiTestSandbox::configFile())) {}
    ~RestoredConfigFile()
    {
        std::ofstream(GuiTestSandbox::configFile()) << content_;
        Config::instance().pathname(pathname_);
        window_.setTitle();
    }
private:
    MainWindow& window_;
    std::wstring pathname_;
    std::string content_;
};

SyncItemInput sandboxSync(const wxString& name = ITEM_NAME)
{
    return SyncItemInput{name, GuiTestSandbox::sourceDir().string(), GuiTestSandbox::targetDir().string()};
}

wxString transientStatus(MainWindow& window)
{
    return window.GetStatusBar()->GetStatusText(0);
}

}

TEST_F(GuiTest, SpeichernSchreibtDateiUndMeldetInStatuszeile)
{
    const RestoredConfigFile restored(mainWindow());
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync()));
    ASSERT_TRUE(Config::instance().changed());
    ASSERT_EQ(readFile(GuiTestSandbox::configFile()).find(ITEM_NAME.ToStdString()), std::string::npos) << "Die Datei kennt die Synchronisation schon vor dem Speichern.";

    activateMenuItem(mainWindow(), tul(id::File_save));
    pump();

    EXPECT_NE(readFile(GuiTestSandbox::configFile()).find(ITEM_NAME.ToStdString()), std::string::npos) << "Die Synchronisation steht nicht in der gespeicherten Datei.";
    EXPECT_FALSE(Config::instance().changed()) << "Nach dem Speichern gilt die Konfiguration noch als geändert.";
    EXPECT_EQ(transientStatus(mainWindow()), "Konfiguration gespeichert");
}

TEST_F(GuiTest, SpeichernUeberWerkzeugleisteSchreibtDatei)
{
    const RestoredConfigFile restored(mainWindow());
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync()));

    ASSERT_TRUE(activateToolbarTool(mainWindow(), tul(id::File_save)));

    EXPECT_NE(readFile(GuiTestSandbox::configFile()).find(ITEM_NAME.ToStdString()), std::string::npos) << "Der Speichern-Knopf hat nicht gespeichert.";
}

TEST_F(GuiTest, SpeichernAlsHaengtJsonAnUndFragtBeiUeberschreiben)  // [ConfigFileTest.md#save-as]
{
    const RestoredConfigFile restored(mainWindow());
    const SandboxTree tree(GuiTestSandbox::root() / "speichern-als", {{"kopie.json", "{}"}});
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync()));

    ModalDialogWatcher::instance().choosePath(SAVE_AS_TITLE, tree.path("kopie").string());
    ModalDialogWatcher::instance().answer(L"Datei überschreiben", wxID_YES);
    activateMenuItem(mainWindow(), tul(id::File_save_as));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Die vorhandene kopie.json wurde ohne Rückfrage überschrieben.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answeredMessages().front().Contains("kopie.json"));
    EXPECT_NE(tree.read("kopie.json").find(ITEM_NAME.ToStdString()), std::string::npos) << "kopie.json enthält die Konfiguration nicht.";
    EXPECT_FALSE(tree.exists("kopie")) << "Es wurde eine Datei ohne Endung geschrieben.";
    EXPECT_EQ(fs::path(Config::instance().pathname()), tree.path("kopie.json")) << "Die Konfiguration zeigt nicht auf die neue Datei.";
    EXPECT_TRUE(mainWindow().GetTitle().Contains("kopie.json")) << "Der Fenstertitel nennt die neue Datei nicht.";
}

TEST_F(GuiTest, SpeichernAlsNeinBeiUeberschreibenLaesstDateiUnberuehrt)  // [ConfigFileTest.md#save-as]
{
    const RestoredConfigFile restored(mainWindow());
    const SandboxTree tree(GuiTestSandbox::root() / "nicht-ueberschreiben", {{"kopie.json", "{}"}});
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync()));

    ModalDialogWatcher::instance().choosePath(SAVE_AS_TITLE, tree.path("kopie").string());
    ModalDialogWatcher::instance().answer(L"Datei überschreiben", wxID_NO);
    activateMenuItem(mainWindow(), tul(id::File_save_as));
    pump();

    EXPECT_EQ(tree.read("kopie.json"), "{}") << "Trotz Nein wurde überschrieben.";
    EXPECT_EQ(Config::instance().pathname(), GuiTestSandbox::configFile().wstring());
}

TEST_F(GuiTest, OeffnenMitUngespeichertenAenderungenFragtNach)  // [ConfigFileTest.md#open]
{
    const RestoredConfigFile restored(mainWindow());
    const SandboxTree tree(GuiTestSandbox::root() / "oeffnen");
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync("guitest-andere")));
    ASSERT_TRUE(Config::instance().write(tree.path("andere.json").wstring()));  // [ConfigFileTest.md#second-file]
    Config::instance().read(GuiTestSandbox::configFile().wstring());
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync("guitest-ungespeichert")));
    ASSERT_TRUE(Config::instance().changed());

    ModalDialogWatcher::instance().answer(NOT_SAVED_TITLE, wxID_NO);
    ModalDialogWatcher::instance().choosePath(OPEN_TITLE, tree.path("andere.json").string());
    activateMenuItem(mainWindow(), tul(id::File_open));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Die ungespeicherte Änderung wurde nicht angesprochen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), "Speichern?");
    EXPECT_TRUE(Config::instance().find_by_name(L"guitest-andere").has_value()) << "Die geöffnete Datei wurde nicht geladen.";
    EXPECT_FALSE(Config::instance().find_by_name(L"guitest-ungespeichert").has_value()) << "Die ungespeicherte Synchronisation überlebte das Öffnen.";
    EXPECT_EQ(readFile(GuiTestSandbox::configFile()).find("guitest-ungespeichert"), std::string::npos) << "Trotz Nein wurde gespeichert.";
    EXPECT_TRUE(listShows(L"guitest-andere")) << "Die Liste zeigt die geöffnete Konfiguration nicht.";
    EXPECT_TRUE(mainWindow().GetTitle().Contains("andere.json"));
}

TEST_F(GuiTest, OeffnenEinerDefektenDateiBehaeltAlteKonfiguration)  // [ConfigFileTest.md#open]
{
    const RestoredConfigFile restored(mainWindow());
    const SandboxTree tree(GuiTestSandbox::root() / "defekt", {{"kaputt.json", "{ \"SyncItems\": [ oops"}});
    ASSERT_TRUE(addSyncItem(mainWindow(), sandboxSync()));
    activateMenuItem(mainWindow(), tul(id::File_save));
    ASSERT_FALSE(Config::instance().changed());

    ModalDialogWatcher::instance().choosePath(OPEN_TITLE, tree.path("kaputt.json").string());
    ModalDialogWatcher::instance().answer("Message", wxID_OK);
    ModalDialogWatcher::instance().answer("Message", wxID_OK);
    activateMenuItem(mainWindow(), tul(id::File_open));
    pump();

    EXPECT_GE(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Die defekte Datei wurde ohne Meldung geladen.";
    EXPECT_TRUE(Config::instance().find_by_name(ITEM_NAME.ToStdWstring()).has_value()) << "Die alte Konfiguration ist nach dem Fehlschlag weg.";
    EXPECT_EQ(Config::instance().pathname(), GuiTestSandbox::configFile().wstring()) << "Die Konfiguration zeigt auf die defekte Datei.";
    EXPECT_TRUE(listShows(ITEM_NAME.ToStdWstring()));
}

}
