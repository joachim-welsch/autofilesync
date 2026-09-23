/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#include <StrUtil.h>

using namespace AFS_Core;

using util::string_to_wstring;
using util::wstring_to_string;

TEST(StrUtilTests, WstringToStringRoundTripsUmlauts)  // [StrUtilTests.md#WstringToStringRoundTripsUmlauts]
{
    const std::wstring original = L"Prüfung äöüÄÖÜß häckchen";

    const std::string utf8 = wstring_to_string(original);
    const std::wstring round_tripped = string_to_wstring(utf8);

    EXPECT_EQ(round_tripped, original);
}

TEST(StrUtilTests, WstringToStringProducesValidUtf8ForUmlauts)
{
    const std::wstring original = L"ü";

    const std::string utf8 = wstring_to_string(original);

    ASSERT_EQ(utf8.size(), 2u);  // U+00FC is encoded in UTF-8 as the two bytes 0xC3 0xBC
    EXPECT_EQ(static_cast<unsigned char>(utf8[0]), 0xC3);
    EXPECT_EQ(static_cast<unsigned char>(utf8[1]), 0xBC);
}

TEST(StrUtilTests, WstringToStringRoundTripsPlainAscii)
{
    const std::wstring original = L"plain-ascii_123";

    EXPECT_EQ(string_to_wstring(wstring_to_string(original)), original);
}

#include <Util.h>

TEST(StrUtilTests, WithTrailingCharHandlesEmptyString)  // [StrUtilTests.md#WithTrailingCharHandlesEmptyString]
{
    EXPECT_EQ(util::with_trailing_char(L"", L'/'), L"/");
}

TEST(StrUtilTests, WithTrailingCharKeepsExistingBehavior)
{
    EXPECT_EQ(util::with_trailing_char(L"/a", L'/'), L"/a/");
    EXPECT_EQ(util::with_trailing_char(L"/a/", L'/'), L"/a/");
}

TEST(StrUtilTests, WildmatchTreatsNonWildcardCharsLiterally)  // [StrUtilTests.md#WildmatchTreatsNonWildcardCharsLiterally]
{
    EXPECT_TRUE(util::wildmatch(L"a.txt", L"*.txt"));
    EXPECT_FALSE(util::wildmatch(L"abXtxt", L"*.txt"));
    EXPECT_TRUE(util::wildmatch(L"Fotos (2020) Juli", L"Fotos (2020)*"));
    EXPECT_TRUE(util::wildmatch(L"data[1].txt", L"data[1].txt"));
    EXPECT_NO_THROW(util::wildmatch(L"x", L"data["));
    EXPECT_TRUE(util::wildmatch(L"a+b.txt", L"a+b.txt"));
    EXPECT_TRUE(util::wildmatch(L"file1.txt", L"file?.txt")); // ? still works
}

TEST(StrUtilTests, RemoveFileReturnsWhetherSomethingWasActuallyRemoved)  // [StrUtilTests.md#RemoveFileReturnsWhetherSomethingWasActuallyRemoved]
{
    const std::wstring non_existent = L"/nonexistent_afx_dir/nonexistent_afx_file.txt";
    EXPECT_FALSE(util::remove_file(non_existent, LogLevel::None));
}
