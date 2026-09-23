/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
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
#ifdef _WIN32
    #include <wx/msw/registry.h>
#endif

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

void applySettings(MainWindow& window, const SettingsInput& input)
{
    ModalDialogWatcher::instance().expect(txt::EditSettings, fillSettingsDialog(input));
    activateMenuItem(window, tul(id::EditSettings));
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
    input.startSyncAtStart = true;
    input.startMinimized = true;
    input.showErrors = true;
    input.showWarnings = true;
    input.showInfos = true;

    applySettings(mainWindow(), input);

    EXPECT_TRUE(settings.start_sync_at_start());
    EXPECT_TRUE(settings.start_minimized());
    EXPECT_TRUE(settings.show_errors());
    EXPECT_TRUE(settings.show_warnings());
    EXPECT_TRUE(settings.show_infos());
    EXPECT_TRUE(Config::instance().changed()) << "Die geänderten Einstellungen gelten nicht als zu speichern.";
}

TEST_F(GuiTest, MinimierenStattSchliessenVerstecktAnstattZuBeenden)  // [SettingsTest.md#MinimierenStattSchliessenVerstecktAnstattZuBeenden]
{
    const RestoredSettings restored;
    SettingsInput input;
    input.minimizeInsteadOfClose = true;
    applySettings(mainWindow(), input);
    ASSERT_TRUE(Config::instance().settings().minimize_instead_of_close());
    ASSERT_TRUE(mainWindow().IsShown());

    mainWindow().Close();
    pump();

    EXPECT_FALSE(mainWindow().IsShown()) << "Das Fenster wurde beim Schließen nicht versteckt.";
    EXPECT_NE(wxTheApp->GetTopWindow(), nullptr) << "Das Fenster wurde trotz Einstellung zerstört.";
    mainWindow().Show();
    pump();
    EXPECT_TRUE(mainWindow().IsShown());
}

TEST_F(GuiTest, EndgueltigLoeschenLoeschtOhnePapierkorb)  // [SettingsTest.md#EndgueltigLoeschenLoeschtOhnePapierkorb]
{
    const RestoredSettings restored;
    const SandboxTree tree(GuiTestSandbox::root() / "endgueltig", {{"weg.txt", "x"}});
    SettingsInput input;
    input.deleteMode = DeleteMode::Permanent;
    applySettings(mainWindow(), input);
    ASSERT_EQ(Config::instance().settings().delete_mode(), DeleteMode::Permanent);
    const std::size_t itemsBefore = RecycleBin::instance().list_items().size();

    ASSERT_TRUE(delete_file(tree.path("weg.txt").wstring()));

    EXPECT_FALSE(tree.exists("weg.txt"));
    EXPECT_EQ(RecycleBin::instance().list_items().size(), itemsBefore) << "Die Datei landete trotz \"Endgültig löschen\" im Papierkorb.";
}

#ifdef _WIN32
TEST_F(GuiTest, AutostartSchreibtRunEintrag)  // [SettingsTest.md#RestoredRunEntry]
{
    const RestoredSettings restored;
    const RestoredRunEntry runEntry;
    SettingsInput input;
    input.autostart = true;
    applySettings(mainWindow(), input);

    const std::optional<wxString> command = runEntry.command();
    ASSERT_TRUE(command.has_value()) << "Der Run-Eintrag wurde nicht angelegt.";
    EXPECT_TRUE(command->Lower().Contains("guitests.exe")) << "Der Run-Eintrag zeigt nicht auf das laufende Programm: " << *command;

    input.autostart = false;
    applySettings(mainWindow(), input);
    EXPECT_FALSE(runEntry.command().has_value()) << "Der Run-Eintrag blieb nach dem Abwählen stehen.";
}
#endif

}
