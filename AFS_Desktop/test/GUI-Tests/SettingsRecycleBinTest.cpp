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
#include "GuiTestSandbox.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "SettingsDialogDriver.h"
#include "UserActions.h"
#include "FileDelete.h"
#include "Config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"
#include <RecycleBin.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

class RestoredState {  // [SettingsRecycleBinTest.md#RestoredState]
public:
    RestoredState(): bin_path_(RecycleBin::instance().bin_path()) {}
    ~RestoredState() {
        RecycleBin::instance().bin_path(bin_path_);
    }
private:
    fs::path bin_path_;
};

std::string read_file(const fs::path& pathname)
{
    std::ostringstream content;
    content << std::ifstream(pathname).rdbuf();
    return content.str();
}

}

TEST_F(GuiTest, NeuerPapierkorbPfadWirdUebernommenUndDateiDorthinVerschoben)
{
    const RestoredState restored_state;
    const fs::path bin_path = GuiTestSandbox::root() / "papierkorb";
    const fs::path deleted_file = GuiTestSandbox::source_dir() / "beispiel.txt";
    const std::string content_before = read_file(deleted_file);
    ASSERT_FALSE(fs::exists(bin_path)) << "Der Papierkorb existiert schon vor dem Test.";

    ModalDialogWatcher::instance().expect(txt::settings(),
        fill_settings_dialog(bin_path.wstring(), DeleteMode::RecycleBin));
    activate_menu_item(main_window(), tul(id::EditSettings));
    pump();

    ASSERT_EQ(RecycleBin::instance().bin_path(), bin_path) << "Der eingegebene Pfad wurde nicht übernommen.";
    ASSERT_EQ(Config::instance().settings().delete_mode(), DeleteMode::RecycleBin);

    ASSERT_TRUE(delete_file(deleted_file.wstring())) << "Die Datei wurde nicht in den Papierkorb verschoben.";

    EXPECT_FALSE(fs::exists(deleted_file));
    EXPECT_TRUE(fs::exists(bin_path)) << "Der Papierkorb wurde nicht angelegt.";
    const RecycleBin::ItemList items = RecycleBin::instance().list_items();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(std::regex_match(items[0].filename().wstring(),
        std::wregex(LR"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-\d{3}_beispiel\.txt)")))
        << "Der Zeitstempel fehlt im Namen im Papierkorb.";
    EXPECT_EQ(read_file(bin_path / items[0]), content_before) << "Der Inhalt hat das Verschieben nicht überlebt.";

    fs::rename(bin_path / items[0], deleted_file);  // [SettingsRecycleBinTest.md#manual-restore]
    fs::remove_all(bin_path);
}

}
