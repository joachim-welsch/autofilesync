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
    std::vector<std::string> syncItemNames;
    std::vector<std::string> sshAccountNames;
    std::string configFile;
    std::vector<std::string> sourceEntries;
    std::vector<std::string> targetEntries;
};

class GuiTest: public ::testing::Test {  // [GuiTest.md#GuiTest]
protected:
    void SetUp() override;
    void TearDown() override;
    MainWindow& mainWindow() const;  // [GuiTest.md#mainWindow]
    ListCtrl* syncItemList() const;
    bool listShows(const std::wstring& syncItemName) const;
private:
    ApplicationState before_;
};

class SlowGuiTest: public GuiTest {};  // [GuiTest.md#SlowGuiTest]

}
