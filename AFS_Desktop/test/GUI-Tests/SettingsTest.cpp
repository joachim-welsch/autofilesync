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
#include "RestoredSettings.h"
#include "SettingsDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "FileDelete.h"
#include "RecycleBin.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"
#include <fstream>
#include <sstream>
#ifdef _WIN32
    #include <wx/msw/registry.h>
#endif

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

void apply_settings(MainWindow& window, const SettingsInput& input)
{
    ModalDialogWatcher::instance().expect(txt::settings(), fill_settings_dialog(input));
    activate_menu_item(window, tul(id::EditSettings));
    pump();
}

#ifdef _WIN32
const wxString RUN_KEY = "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wxString RUN_VALUE = "AutoFileSync";

class RestoredRunEntry {  // [SettingsTest.md#RestoredRunEntry]
public:
    RestoredRunEntry(): key_(RUN_KEY), had_(key_.HasValue(RUN_VALUE))
    {
        if (had_)
            key_.QueryValue(RUN_VALUE, command_);
    }
    ~RestoredRunEntry()
    {
        if (had_)
            key_.SetValue(RUN_VALUE, command_);
        else if (key_.HasValue(RUN_VALUE))
            key_.DeleteValue(RUN_VALUE);
    }
    std::optional<wxString> command() const
    {
        wxString command;
        return key_.HasValue(RUN_VALUE) && key_.QueryValue(RUN_VALUE, command) ? std::optional(command) : std::nullopt;
    }
private:
    wxRegKey key_;
    bool had_;
    wxString command_;
};
#endif

}

TEST_F(GuiTest, EinstellungenKommenInDieKonfiguration)  // [SettingsTest.md#EinstellungenKommenInDieKonfiguration]
{
    const RestoredSettings restored;
    const Settings& settings = Config::instance().settings();
    ASSERT_FALSE(settings.start_sync_at_start());
    ASSERT_FALSE(settings.start_minimized());
    ASSERT_FALSE(settings.show_infos());
    SettingsInput input;
    input.start_sync_at_start = true;
    input.start_minimized = true;
    input.show_errors = true;
    input.show_warnings = true;
    input.show_infos = true;

    apply_settings(main_window(), input);

    EXPECT_TRUE(settings.start_sync_at_start());
    EXPECT_TRUE(settings.start_minimized());
    EXPECT_TRUE(settings.show_errors());
    EXPECT_TRUE(settings.show_warnings());
    EXPECT_TRUE(settings.show_infos());
    EXPECT_FALSE(Config::instance().changed()) << "Die geänderten Einstellungen wurden nicht sofort gespeichert.";
    std::ostringstream saved_file;
    saved_file << std::ifstream(GuiTestSandbox::config_file()).rdbuf();
    EXPECT_NE(saved_file.str().find("\"ShowInfos\": \"true\""), std::string::npos) << "Die Einstellung steht nicht in der Datei.";
}

TEST_F(GuiTest, SpracheSchaltetSofortUmUndKommtInDieKonfiguration)  // [SettingsTest.md#SpracheSchaltetSofortUmUndKommtInDieKonfiguration]
{
    const RestoredSettings restored;
    ASSERT_TRUE(Config::instance().settings().language().empty());
    SettingsInput english;
    english.language = "English";

    apply_settings(main_window(), english);

    EXPECT_EQ(Config::instance().settings().language(), "en");
    EXPECT_EQ(main_window().GetMenuBar()->GetMenuLabelText(0), "File") << "Das Menü wurde nicht sofort umgestellt.";
    EXPECT_EQ(main_window().GetToolBar()->GetToolShortHelp(tul(id::EditSettings)), "Global settings") << "Die Werkzeugleiste wurde nicht umgestellt.";
    std::ostringstream saved_file;
    saved_file << std::ifstream(GuiTestSandbox::config_file()).rdbuf();
    EXPECT_NE(saved_file.str().find("\"Language\": \"en\""), std::string::npos) << "Die Sprache steht nicht in der Datei.";

    SettingsInput system;
    system.language = translation::translate("language_system");
    apply_settings(main_window(), system);

    EXPECT_TRUE(Config::instance().settings().language().empty());
    EXPECT_EQ(main_window().GetMenuBar()->GetMenuLabel(0), translation::translate("menu_file")) << "Die Sprache des Betriebssystems kam nicht zurück.";
}

TEST_F(GuiTest, WerkzeugleisteZahnradOeffnetEinstellungen)  // [SettingsTest.md#WerkzeugleisteZahnradOeffnetEinstellungen]
{
    ModalDialogWatcher::instance().answer(txt::settings(), wxID_CANCEL);

    ASSERT_TRUE(activate_toolbar_tool(main_window(), tul(id::EditSettings))) << "Die Werkzeugleiste hat kein Zahnrad.";
    pump();

    EXPECT_FALSE(ModalDialogWatcher::instance().has_pending_expectations()) << "Das Zahnrad öffnete die globalen Einstellungen nicht.";
    EXPECT_TRUE(ModalDialogWatcher::instance().unexpected_dialogs().empty());
}

TEST_F(GuiTest, MinimierenStattSchliessenVerstecktAnstattZuBeenden)  // [SettingsTest.md#MinimierenStattSchliessenVerstecktAnstattZuBeenden]
{
    const RestoredSettings restored;
    SettingsInput input;
    input.minimize_instead_of_close = true;
    apply_settings(main_window(), input);
    ASSERT_TRUE(Config::instance().settings().minimize_instead_of_close());
    ASSERT_TRUE(main_window().IsShown());

    main_window().Close();
    pump();

    EXPECT_FALSE(main_window().IsShown()) << "Das Fenster wurde beim Schließen nicht versteckt.";
    EXPECT_NE(wxTheApp->GetTopWindow(), nullptr) << "Das Fenster wurde trotz Einstellung zerstört.";
    main_window().Show();
    pump();
    EXPECT_TRUE(main_window().IsShown());
}

TEST_F(GuiTest, EndgueltigLoeschenLoeschtOhnePapierkorb)  // [SettingsTest.md#EndgueltigLoeschenLoeschtOhnePapierkorb]
{
    const RestoredSettings restored;
    const SandboxTree tree(GuiTestSandbox::root() / "endgueltig", {{"weg.txt", "x"}});
    SettingsInput input;
    input.delete_mode = DeleteMode::Permanent;
    apply_settings(main_window(), input);
    ASSERT_EQ(Config::instance().settings().delete_mode(), DeleteMode::Permanent);
    const std::size_t items_before = RecycleBin::instance().list_items().size();

    ASSERT_TRUE(delete_file(tree.path("weg.txt").wstring()));

    EXPECT_FALSE(tree.exists("weg.txt"));
    EXPECT_EQ(RecycleBin::instance().list_items().size(), items_before) << "Die Datei landete trotz \"Endgültig löschen\" im Papierkorb.";
}

#ifdef _WIN32
TEST_F(GuiTest, AutostartSchreibtRunEintrag)  // [SettingsTest.md#RestoredRunEntry]
{
    const RestoredSettings restored;
    const RestoredRunEntry run_entry;
    SettingsInput input;
    input.autostart = true;
    apply_settings(main_window(), input);

    const std::optional<wxString> command = run_entry.command();
    ASSERT_TRUE(command.has_value()) << "Der Run-Eintrag wurde nicht angelegt.";
    EXPECT_TRUE(command->Lower().Contains("guitests.exe")) << "Der Run-Eintrag zeigt nicht auf das laufende Programm: " << *command;

    input.autostart = false;
    apply_settings(main_window(), input);
    EXPECT_FALSE(run_entry.command().has_value()) << "Der Run-Eintrag blieb nach dem Abwählen stehen.";
}
#endif

}
