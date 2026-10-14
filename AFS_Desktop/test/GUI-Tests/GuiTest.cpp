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
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "ViewActions.h"
#include "Config.h"
#include "id.h"
#include "ListCtrl.h"
#include "Util.h"
#include "utilwx.h"
#include <wx/app.h>
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

std::string read_file(const fs::path& pathname)
{
    std::ostringstream content;
    content << std::ifstream(pathname).rdbuf();
    return content.str();
}

std::vector<std::string> entries_of(const fs::path& directory)  // [GuiTest.md#entries_of]
{
    std::vector<std::string> entries;
    for (const auto& entry : fs::recursive_directory_iterator(directory))
        entries.push_back(entry.path().lexically_relative(directory).string()
            + (entry.is_regular_file() ? ":" + std::to_string(entry.file_size()) : ""));
    std::sort(entries.begin(), entries.end());
    return entries;
}

ApplicationState capture_state()
{
    ApplicationState state;
    for (const auto& item : Config::instance().syncitems())
        state.sync_item_names.push_back(util::wstring_to_string(item.name()));
    for (const auto& account : Config::instance().ssh_accounts_snapshot())
        state.ssh_account_names.push_back(account.name());
    state.config_file = read_file(GuiTestSandbox::config_file());
    state.source_entries = entries_of(GuiTestSandbox::source_dir());
    state.target_entries = entries_of(GuiTestSandbox::target_dir());
    return state;
}

}

void GuiTest::SetUp()
{
    ModalDialogWatcher::instance().reset();
    before_ = capture_state();
}

void GuiTest::TearDown()
{
    if (read_file(GuiTestSandbox::config_file()) != before_.config_file)  // [GuiTest.md#autosave-undone]
        std::ofstream(GuiTestSandbox::config_file()) << before_.config_file;
    Config::instance().read(GuiTestSandbox::config_file().wstring());
    show_view(main_window(), id::View_List);  // [GuiTest.md#list-view-restored]
    if (ListCtrl* list = sync_item_list())
        list->update();
    const ApplicationState after = capture_state();
    EXPECT_EQ(before_.sync_item_names, after.sync_item_names) << "Die Konfiguration wurde verändert.";
    EXPECT_EQ(before_.ssh_account_names, after.ssh_account_names) << "Die SSH-Verbindungen wurden verändert.";
    EXPECT_EQ(before_.source_entries, after.source_entries) << "Das Quellverzeichnis wurde verändert.";
    EXPECT_EQ(before_.target_entries, after.target_entries) << "Das Zielverzeichnis wurde verändert.";
    ModalDialogWatcher& watcher = ModalDialogWatcher::instance();
    const std::vector<wxString>& unexpected = watcher.unexpected_dialogs();  // [GuiTest.md#both-branches-wxstring]
    EXPECT_TRUE(unexpected.empty())
        << "Unerwarteter Dialog: " << (unexpected.empty() ? wxString() : unexpected.front());
    EXPECT_FALSE(watcher.has_pending_expectations()) << "Ein erwarteter Dialog wurde nie geöffnet.";
    watcher.reset();
}

MainWindow& GuiTest::main_window() const
{
    return *static_cast<MainWindow*>(wxTheApp->GetTopWindow());
}

ListCtrl* GuiTest::sync_item_list() const
{
    return dynamic_cast<ListCtrl*>(find(main_window(), tul(id::ListCtrl)));
}

bool GuiTest::list_shows(const std::wstring& sync_item_name) const
{
    ListCtrl* list = sync_item_list();
    if (list == nullptr)
        return false;
    for (long i = 0; i < list->GetItemCount(); ++i) {
        if (list->GetItemText(i).ToStdWstring() == sync_item_name)
            return true;
    }
    return false;
}

}
