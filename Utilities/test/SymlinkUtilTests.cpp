/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
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
#include "pch.h"
#ifdef _UNIX
#include <SymlinkUtil.h>
#include "TestPaths.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;

class SymlinkUtilTest : public ::testing::Test {  // [SymlinkUtilTests.md#SymlinkUtilTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_symlink_util_" + std::to_string(TestPaths::process_id()));

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(root_);
        std::ofstream(root_ / "da.txt") << "da";
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
};

} // namespace

TEST_F(SymlinkUtilTest, DanglingLinkIsALinkWhoseTargetIsMissingOrRunsInACircle)  // [SymlinkUtilTests.md#DanglingLinkIsALinkWhoseTargetIsMissingOrRunsInACircle]
{
    fs::create_symlink(root_ / "da.txt", root_ / "gut");
    fs::create_symlink(root_ / "weg.txt", root_ / "tot");
    fs::create_symlink("kreis", root_ / "kreis");

    EXPECT_TRUE(symlink_util::dangling_link(root_ / "tot"));
    EXPECT_TRUE(symlink_util::dangling_link(root_ / "kreis"));
    EXPECT_FALSE(symlink_util::dangling_link(root_ / "gut"));
    EXPECT_FALSE(symlink_util::dangling_link(root_ / "da.txt")) << "a file is no link";
    EXPECT_FALSE(symlink_util::dangling_link(root_ / "weg.txt")) << "a missing file is no link either";
}
#endif
