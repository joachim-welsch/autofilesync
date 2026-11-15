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
#include <FileName.h>
#include <StrUtil.h>
#include <chrono>
#include <filesystem>
#include <regex>

using namespace AFS_Core;

namespace fs = std::filesystem;

TEST(FileNameTests, TimestampPrefixKeepsTheDirectory)
{
    const fs::path prefixed = file_name::with_timestamp_prefix(fs::path("dir") / "sub" / "data.txt");

    EXPECT_EQ(prefixed.parent_path(), fs::path("dir") / "sub");
    EXPECT_TRUE(std::regex_match(prefixed.filename().wstring(),
        std::wregex(LR"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-\d{3}_data\.txt)")));
}

TEST(FileNameTests, TimestampPrefixCarriesTheMilliseconds)  // [FileNameTests.md#TimestampPrefixCarriesTheMilliseconds]
{
    using namespace std::chrono;
    const system_clock::time_point time = system_clock::from_time_t(1700000000) + milliseconds(7);

    const std::wstring name = file_name::with_timestamp_prefix("data.txt", time).wstring();

    EXPECT_TRUE(name.ends_with(L"-007_data.txt")) << util::wstring_to_string(name);
}

TEST(FileNameTests, TimestampPrefixNeedsAFileName)  // [FileNameTests.md#TimestampPrefixNeedsAFileName]
{
    const fs::path directory = fs::path("dir") / "";

    EXPECT_EQ(file_name::with_timestamp_prefix(directory), directory);
    EXPECT_EQ(file_name::with_timestamp_prefix(fs::path()), fs::path());
}

TEST(FileNameTests, SanitizedReplacesWhatWindowsForbids)  // [FileName.md#sanitized]
{
    EXPECT_EQ(file_name::sanitized(L"Fotos: 2020/21 <neu>?"), L"Fotos_ 2020_21 _neu__");
    EXPECT_EQ(file_name::sanitized(L"a\tb"), L"a_b");
    EXPECT_EQ(file_name::sanitized(L"Urlaub. . "), L"Urlaub");
    EXPECT_EQ(file_name::sanitized(L"Büro"), L"Büro");
    EXPECT_EQ(file_name::sanitized(L"..."), L"");
}

TEST(FileNameTests, LocalDateHasTheFormOfAnIsoDate)
{
    EXPECT_TRUE(std::regex_match(file_name::local_date(), std::wregex(L"\\d{4}-\\d{2}-\\d{2}")));
}
