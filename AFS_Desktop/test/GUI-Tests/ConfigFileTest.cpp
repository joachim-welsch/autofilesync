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
#include "RestoredBackgroundService.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "TextResources.h"
#include "UserActions.h"
#include "BackgroundService.h"
#include "Config.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"
#include <fstream>
#include <sstream>

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

wxString open_title()  // translated at the call, once the catalogue is bound
{
    return translation::translate("open_config_title");
}
wxString save_as_title()
{
    return translation::translate("save_as_title");
}
wxString not_saved_title()
{
    return translation::translate("not_saved_title");
}
wxString unsaved_marker()
{
    return translation::translate("title_unsaved");
}
const wxString ITEM_NAME = "guitest-konfiguration";

std::string read_file(const fs::path& pathname)
{
    std::ostringstream content;
    content << std::ifstream(pathname).rdbuf();
    return content.str();
}

class RestoredConfigFile {  // [ConfigFileTest.md#RestoredConfigFile]
public:
    explicit RestoredConfigFile(MainWindow& window)
        : window_(window), pathname_(Config::instance().pathname()), content_(read_file(GuiTestSandbox::config_file())) {}
    ~RestoredConfigFile()
    {
        std::ofstream(GuiTestSandbox::config_file()) << content_;
        Config::instance().pathname(pathname_);
        window_.set_title();
    }
private:
    MainWindow& window_;
    std::wstring pathname_;
    std::string content_;
};

SyncItemInput sandbox_sync(const wxString& name = ITEM_NAME)
{
    return SyncItemInput{name, GuiTestSandbox::source_dir().string(), GuiTestSandbox::target_dir().string()};
}

class BlockedConfigFile {  // [ConfigFileTest.md#BlockedConfigFile]
public:
    BlockedConfigFile()
        : pathname_(Config::instance().pathname()), blocked_(GuiTestSandbox::root() / "gesperrt")
    {
        fs::create_directories(blocked_);
        Config::instance().pathname(blocked_.wstring());
    }
    ~BlockedConfigFile()
    {
        Config::instance().pathname(pathname_);
        fs::remove_all(blocked_);
        fs::remove(blocked_.string() + ".tmp");
    }
private:
    std::wstring pathname_;
    fs::path blocked_;
};

bool add_unsaved_sync_item(MainWindow& window, const SyncItemInput& input)  // [ConfigFileTest.md#add_unsaved_sync_item]
{
    const BlockedConfigFile blocked;
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), fill_sync_item_dialog(input));
    ModalDialogWatcher::instance().answer(translation::translate("error"), wxID_OK);
    activate_menu_item(window, tul(id::File_new_Synchronization));
    pump();
    return Config::instance().find_by_name(input.name.ToStdWstring()).has_value() && Config::instance().changed();
}

bool save_enabled(MainWindow& window)
{
    window.UpdateWindowUI();  // the menu bar refreshes its items through update-UI events only when needed
    return window.GetMenuBar()->IsEnabled(tul(id::File_save));
}

wxString transient_status(MainWindow& window)
{
    return window.GetStatusBar()->GetStatusText(0);
}

}

TEST_F(GuiTest, AenderungWirdSofortGespeichert)  // [ConfigFileTest.md#autosave]
{
    const RestoredConfigFile restored(main_window());
    ASSERT_FALSE(save_enabled(main_window())) << "Speichern ist anwählbar, obwohl nichts aussteht.";

    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync()));

    EXPECT_NE(read_file(GuiTestSandbox::config_file()).find(ITEM_NAME.ToStdString()), std::string::npos) << "Die neue Synchronisation steht nicht sofort in der Datei.";
    EXPECT_FALSE(Config::instance().changed()) << "Nach dem automatischen Speichern gilt die Konfiguration noch als geändert.";
    EXPECT_FALSE(main_window().GetTitle().Contains(unsaved_marker())) << "Der Titel meldet Ungespeichertes, obwohl gespeichert wurde.";
    EXPECT_FALSE(save_enabled(main_window())) << "Speichern bleibt anwählbar, obwohl gespeichert wurde.";
}

TEST_F(GuiTest, FehlgeschlagenesSchreibenZeigtSichImTitelUndSpeichernHoltNach)  // [ConfigFileTest.md#title]
{
    const RestoredConfigFile restored(main_window());
    ASSERT_TRUE(add_unsaved_sync_item(main_window(), sandbox_sync())) << "Das gesperrte Schreiben ließ die Änderung nicht als ungespeichert zurück.";
    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Das fehlgeschlagene Schreiben blieb ohne Meldung.";
    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().front(), translation::translate("config_save_failed"));
    EXPECT_TRUE(main_window().GetTitle().Contains(unsaved_marker())) << "Der Titel verschweigt die ungespeicherte Änderung.";
    EXPECT_TRUE(save_enabled(main_window())) << "Speichern ist nach dem Fehlschlag nicht anwählbar.";

    activate_menu_item(main_window(), tul(id::File_save));
    pump();

    EXPECT_NE(read_file(GuiTestSandbox::config_file()).find(ITEM_NAME.ToStdString()), std::string::npos) << "Die Synchronisation steht nicht in der nachgespeicherten Datei.";
    EXPECT_FALSE(Config::instance().changed()) << "Nach dem Speichern gilt die Konfiguration noch als geändert.";
    EXPECT_FALSE(main_window().GetTitle().Contains(unsaved_marker())) << "Der Titel meldet nach dem Speichern noch Ungespeichertes.";
    EXPECT_EQ(transient_status(main_window()), translation::translate("config_saved"));
}

TEST_F(GuiTest, DienststartAbbrechenBeiUngespeicherterKonfigurationStartetNicht)  // [ConfigFileTest.md#background-service]
{
    ASSERT_TRUE(add_unsaved_sync_item(main_window(), sandbox_sync()));
    ModalDialogWatcher::instance().answer(not_saved_title(), wxID_CANCEL);

    activate_menu_item(main_window(), tul(id::BackgroundServiceStart));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 2u) << "Der Dienststart fragte nicht nach der ungespeicherten Konfiguration.";
    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().back(), translation::translate("bs_save_question"));
    EXPECT_TRUE(Config::instance().changed()) << "Abbrechen hat gespeichert.";
    EXPECT_EQ(read_file(GuiTestSandbox::config_file()).find(ITEM_NAME.ToStdString()), std::string::npos) << "Abbrechen hat die Datei geschrieben.";
    EXPECT_TRUE(ModalDialogWatcher::instance().unexpected_dialogs().empty()) << "Trotz Abbrechen ging der Dienststart weiter.";
}

TEST_F(GuiTest, DienststartSpeichertAufWunschVorDemStart)  // [ConfigFileTest.md#background-service]
{
    const bool installed = BackgroundService::state() != BackgroundService::State::NotInstalled;
    if (installed && !service_controllable())
        GTEST_SKIP() << "Der Hintergrunddienst ist installiert, und dieser Benutzer darf ihn nicht ohne Administratorrechte starten.";
    const RestoredConfigFile restored(main_window());
    const RestoredBackgroundService service;
    if (const std::wstring obstacle = installed ? start_obstacle() : L""; !obstacle.empty())  // [ConfigFileTest.md#background-service]
        GTEST_SKIP() << "Der Hintergrunddienst lässt sich auf dieser Maschine nicht starten: " << obstacle;
    ASSERT_TRUE(add_unsaved_sync_item(main_window(), sandbox_sync()));
    ModalDialogWatcher::instance().answer(not_saved_title(), wxID_YES);
    if (!installed)
        ModalDialogWatcher::instance().answer(translation::translate("bs_caption"), wxID_OK);

    activate_menu_item(main_window(), tul(id::BackgroundServiceStart));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), installed ? 2u : 3u) << "Die Rückfrage oder die Fehlerbox des fehlenden Dienstes blieb aus.";
    EXPECT_NE(read_file(GuiTestSandbox::config_file()).find(ITEM_NAME.ToStdString()), std::string::npos) << "Ja hat die Datei nicht vor dem Start geschrieben.";
    EXPECT_FALSE(Config::instance().changed());
    if (installed)
        EXPECT_EQ(BackgroundService::state(), BackgroundService::State::Running) << "Der installierte Dienst lief nach dem Start nicht.";
}

TEST_F(GuiTest, SpeichernUeberWerkzeugleisteHoltFehlgeschlagenesSchreibenNach)
{
    const RestoredConfigFile restored(main_window());
    ASSERT_TRUE(add_unsaved_sync_item(main_window(), sandbox_sync()));
    ASSERT_TRUE(save_enabled(main_window())) << "Speichern ist nach dem Fehlschlag nicht anwählbar.";  // refreshes the toolbar as well

    ASSERT_TRUE(activate_toolbar_tool(main_window(), tul(id::File_save)));

    EXPECT_NE(read_file(GuiTestSandbox::config_file()).find(ITEM_NAME.ToStdString()), std::string::npos) << "Der Speichern-Knopf hat nicht gespeichert.";
    EXPECT_FALSE(Config::instance().changed());
}

TEST_F(GuiTest, SpeichernAlsHaengtJsonAnUndFragtBeiUeberschreiben)  // [ConfigFileTest.md#save-as]
{
    const RestoredConfigFile restored(main_window());
    const SandboxTree tree(GuiTestSandbox::root() / "speichern-als", {{"kopie.json", "{}"}});
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync()));

    ModalDialogWatcher::instance().choose_path(save_as_title(), tree.path("kopie").string());
    ModalDialogWatcher::instance().answer(translation::translate("overwrite_title"), wxID_YES);
    activate_menu_item(main_window(), tul(id::File_save_as));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Die vorhandene kopie.json wurde ohne Rückfrage überschrieben.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answered_messages().front().Contains("kopie.json"));
    EXPECT_NE(tree.read("kopie.json").find(ITEM_NAME.ToStdString()), std::string::npos) << "kopie.json enthält die Konfiguration nicht.";
    EXPECT_FALSE(tree.exists("kopie")) << "Es wurde eine Datei ohne Endung geschrieben.";
    EXPECT_EQ(fs::path(Config::instance().pathname()), tree.path("kopie.json")) << "Die Konfiguration zeigt nicht auf die neue Datei.";
    EXPECT_TRUE(main_window().GetTitle().Contains("kopie.json")) << "Der Fenstertitel nennt die neue Datei nicht.";
}

TEST_F(GuiTest, SpeichernAlsNeinBeiUeberschreibenLaesstDateiUnberuehrt)  // [ConfigFileTest.md#save-as]
{
    const RestoredConfigFile restored(main_window());
    const SandboxTree tree(GuiTestSandbox::root() / "nicht-ueberschreiben", {{"kopie.json", "{}"}});
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync()));

    ModalDialogWatcher::instance().choose_path(save_as_title(), tree.path("kopie").string());
    ModalDialogWatcher::instance().answer(translation::translate("overwrite_title"), wxID_NO);
    activate_menu_item(main_window(), tul(id::File_save_as));
    pump();

    EXPECT_EQ(tree.read("kopie.json"), "{}") << "Trotz Nein wurde überschrieben.";
    EXPECT_EQ(Config::instance().pathname(), GuiTestSandbox::config_file().wstring());
}

TEST_F(GuiTest, OeffnenMitUngespeichertenAenderungenFragtNach)  // [ConfigFileTest.md#open]
{
    const RestoredConfigFile restored(main_window());
    const SandboxTree tree(GuiTestSandbox::root() / "oeffnen");
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync("guitest-andere")));
    ASSERT_TRUE(Config::instance().write(tree.path("andere.json").wstring()));  // [ConfigFileTest.md#second-file]
    Config::instance().read(GuiTestSandbox::config_file().wstring());
    ASSERT_TRUE(add_unsaved_sync_item(main_window(), sandbox_sync("guitest-ungespeichert")));

    ModalDialogWatcher::instance().answer(not_saved_title(), wxID_NO);
    ModalDialogWatcher::instance().choose_path(open_title(), tree.path("andere.json").string());
    activate_menu_item(main_window(), tul(id::File_open));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 2u) << "Die ungespeicherte Änderung wurde nicht angesprochen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().back(), translation::translate("save_question"));
    EXPECT_TRUE(Config::instance().find_by_name(L"guitest-andere").has_value()) << "Die geöffnete Datei wurde nicht geladen.";
    EXPECT_FALSE(Config::instance().find_by_name(L"guitest-ungespeichert").has_value()) << "Die ungespeicherte Synchronisation überlebte das Öffnen.";
    EXPECT_EQ(read_file(GuiTestSandbox::config_file()).find("guitest-ungespeichert"), std::string::npos) << "Trotz Nein wurde gespeichert.";
    EXPECT_TRUE(list_shows(L"guitest-andere")) << "Die Liste zeigt die geöffnete Konfiguration nicht.";
    EXPECT_TRUE(main_window().GetTitle().Contains("andere.json"));
}

TEST_F(GuiTest, OeffnenEinerDefektenDateiBehaeltAlteKonfiguration)  // [ConfigFileTest.md#open]
{
    const RestoredConfigFile restored(main_window());
    const SandboxTree tree(GuiTestSandbox::root() / "defekt", {{"kaputt.json", "{ \"SyncItems\": [ oops"}});
    ASSERT_TRUE(add_sync_item(main_window(), sandbox_sync()));
    ASSERT_FALSE(Config::instance().changed());

    ModalDialogWatcher::instance().choose_path(open_title(), tree.path("kaputt.json").string());
    ModalDialogWatcher::instance().answer("Message", wxID_OK);
    ModalDialogWatcher::instance().answer("Message", wxID_OK);
    activate_menu_item(main_window(), tul(id::File_open));
    pump();

    EXPECT_GE(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Die defekte Datei wurde ohne Meldung geladen.";
    EXPECT_TRUE(Config::instance().find_by_name(ITEM_NAME.ToStdWstring()).has_value()) << "Die alte Konfiguration ist nach dem Fehlschlag weg.";
    EXPECT_EQ(Config::instance().pathname(), GuiTestSandbox::config_file().wstring()) << "Die Konfiguration zeigt auf die defekte Datei.";
    EXPECT_TRUE(list_shows(ITEM_NAME.ToStdWstring()));
}

}
