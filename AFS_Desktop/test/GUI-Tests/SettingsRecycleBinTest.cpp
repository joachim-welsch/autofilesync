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
    RestoredState(): binPath_(RecycleBin::instance().bin_path()) {}
    ~RestoredState() {
        RecycleBin::instance().bin_path(binPath_);
    }
private:
    fs::path binPath_;
};

std::string readFile(const fs::path& pathname)
{
    std::ostringstream content;
    content << std::ifstream(pathname).rdbuf();
    return content.str();
}

}

TEST_F(GuiTest, NeuerPapierkorbPfadWirdUebernommenUndDateiDorthinVerschoben)
{
    const RestoredState restoredState;
    const fs::path binPath = GuiTestSandbox::root() / "papierkorb";
    const fs::path deletedFile = GuiTestSandbox::sourceDir() / "beispiel.txt";
    const std::string contentBefore = readFile(deletedFile);
    ASSERT_FALSE(fs::exists(binPath)) << "Der Papierkorb existiert schon vor dem Test.";

    ModalDialogWatcher::instance().expect(txt::EditSettings,
        fillSettingsDialog(binPath.wstring(), DeleteMode::RecycleBin));
    activateMenuItem(mainWindow(), tul(id::EditSettings));
    pump();

    ASSERT_EQ(RecycleBin::instance().bin_path(), binPath) << "Der eingegebene Pfad wurde nicht übernommen.";
    ASSERT_EQ(Config::instance().settings().delete_mode(), DeleteMode::RecycleBin);

    ASSERT_TRUE(delete_file(deletedFile.wstring())) << "Die Datei wurde nicht in den Papierkorb verschoben.";

    EXPECT_FALSE(fs::exists(deletedFile));
    EXPECT_TRUE(fs::exists(binPath)) << "Der Papierkorb wurde nicht angelegt.";
    const RecycleBin::ItemList items = RecycleBin::instance().list_items();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(std::regex_match(items[0].filename().wstring(),
        std::wregex(LR"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-\d{3}_beispiel\.txt)")))
        << "Der Zeitstempel fehlt im Namen im Papierkorb.";
    EXPECT_EQ(readFile(binPath / items[0]), contentBefore) << "Der Inhalt hat das Verschieben nicht überlebt.";

    fs::rename(binPath / items[0], deletedFile);  // [SettingsRecycleBinTest.md#manual-restore]
    fs::remove_all(binPath);
}

}
