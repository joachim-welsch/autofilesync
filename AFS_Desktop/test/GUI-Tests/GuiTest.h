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
#pragma once

#include "MainWindow.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

class ListCtrl;

namespace guitest {

struct ApplicationState {  // snapshot of everything a GUI test is required to leave untouched
    std::vector<std::string> sync_item_names;
    std::vector<std::string> ssh_account_names;
    std::vector<std::string> device_ids;
    std::string config_file;
    std::vector<std::string> source_entries;
    std::vector<std::string> target_entries;
};

class GuiTest: public ::testing::Test {  // [GuiTest.md#GuiTest]
protected:
    void SetUp() override;
    void TearDown() override;
    MainWindow& main_window() const;  // [GuiTest.md#main_window]
    ListCtrl* sync_item_list() const;
    bool list_shows(const std::wstring& sync_item_name) const;
private:
    ApplicationState before_;
};

class DISABLED_SlowGuiTest: public GuiTest {};  // [GuiTest.md#SlowGuiTest]
class DISABLED_ChatGuiTest: public GuiTest {};  // [GuiTest.md#ChatGuiTest]

}
