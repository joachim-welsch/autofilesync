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
#include <FileTime.h>
#include <chrono>
#include <filesystem>

using namespace AFS_Core;

namespace fs = std::filesystem;

TEST(FileTimeTests, FileTimeRoundTripsThroughTimeT)  // [FileTimeTests.md#FileTimeRoundTripsThroughTimeT]
{
    constexpr std::time_t MOMENT = 1700000000;

    EXPECT_EQ(file_time::to_time_t(file_time::to_file_time(MOMENT)), MOMENT);
    EXPECT_EQ(file_time::to_time_t(file_time::to_file_time(0)), 0);
}

TEST(FileTimeTests, FileTimeRoundTripsThroughUnixNanoseconds)  // [FileTimeTests.md#FileTimeRoundTripsThroughUnixNanoseconds]
{
    const fs::file_time_type now = fs::file_time_type::clock::now();
    constexpr int64_t MOMENT = 1'700'000'000'123'456'700;

    EXPECT_EQ(file_time::from_unix_nanoseconds(file_time::to_unix_nanoseconds(now)), now);
    EXPECT_EQ(file_time::to_unix_nanoseconds(file_time::from_unix_nanoseconds(MOMENT)), MOMENT);
    EXPECT_EQ(file_time::to_unix_nanoseconds(file_time::from_unix_nanoseconds(0)), 0);
}

TEST(FileTimeTests, EqualAndGreaterCompareFileTimes)
{
    const fs::file_time_type earlier = file_time::to_file_time(1000);
    const fs::file_time_type later = file_time::to_file_time(1001);

    EXPECT_TRUE(file_time::equal(earlier, earlier));
    EXPECT_FALSE(file_time::equal(earlier, later));
    EXPECT_TRUE(file_time::greater(later, earlier));
    EXPECT_FALSE(file_time::greater(earlier, later));
    EXPECT_FALSE(file_time::greater(earlier, earlier));
}
