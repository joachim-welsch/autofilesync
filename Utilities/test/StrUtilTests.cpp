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
#include <StrUtil.h>
#include <FileTime.h>
#include <limits>
#include <stdexcept>
#include <Logging.h>

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

TEST(StrUtilTests, TrimmedRemovesWhitespaceAtBothEnds)
{
    EXPECT_EQ(util::trimmed(" \t a b \r\n"), "a b");
    EXPECT_EQ(util::trimmed(std::wstring(L"\n ä ö\t")), L"ä ö");
    EXPECT_EQ(util::trimmed(" \r\n\t"), "");
    EXPECT_EQ(util::trimmed(""), "");
}

TEST(StrUtilTests, LowercaseLowersLetters)
{
    EXPECT_EQ(util::lowercase(std::string("X-OC-MTime")), "x-oc-mtime");
    EXPECT_EQ(util::lowercase(std::wstring(L"Sync ITEM 1")), L"sync item 1");
}

TEST(StrUtilTests, JoinedPutsSeparatorOnlyBetweenParts)
{
    EXPECT_EQ(util::joined(std::vector<std::string>{"a", "b", "c"}, ","), "a,b,c");
    EXPECT_EQ(util::joined(std::vector<std::wstring>{L"a", L"", L"c"}, L", "), L"a, , c");
    EXPECT_EQ(util::joined(std::vector<std::string>{"a"}, ","), "a");
    EXPECT_EQ(util::joined(std::vector<std::string>{}, ","), "");
}

TEST(StrUtilTests, SplitDropsEmptyParts)  // [StrUtil.md#split]
{
    EXPECT_EQ(util::split("/a//b/", '/'), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(util::split("a", '/'), (std::vector<std::string>{"a"}));
    EXPECT_TRUE(util::split("", '/').empty());
    EXPECT_TRUE(util::split("//", '/').empty());
}

TEST(StrUtilTests, UnquotedRemovesOnePairOfSurroundingQuotes)
{
    EXPECT_EQ(util::unquoted("\"a b\""), "a b");
    EXPECT_EQ(util::unquoted("a\"b"), "a\"b");
    EXPECT_EQ(util::unquoted("\""), "\"");
    EXPECT_EQ(util::unquoted("\"\""), "");
}

TEST(StrUtilTests, WithoutTrailingCharsRemovesAllOfThem)
{
    EXPECT_EQ(util::without_trailing_chars("https://host//", '/'), "https://host");
    EXPECT_EQ(util::without_trailing_chars("/a/b", '/'), "/a/b");
    EXPECT_EQ(util::without_trailing_chars("///", '/'), "");
}

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

TEST(StrUtilTests, ToUint16AcceptsTheWholeRange)
{
    EXPECT_EQ(util::to_uint16_t(L"0"), 0);
    EXPECT_EQ(util::to_uint16_t(L"22"), 22);
    EXPECT_EQ(util::to_uint16_t(L"65535"), 65535);
}

TEST(StrUtilTests, ToUint16RejectsValuesOutsideTheRange)  // [StrUtilTests.md#ToUint16RejectsValuesOutsideTheRange]
{
    EXPECT_THROW(util::to_uint16_t(L"-1"), std::out_of_range);
    EXPECT_THROW(util::to_uint16_t(L"65536"), std::out_of_range);
    EXPECT_THROW(util::to_uint16_t(L"99999999999999999999"), std::out_of_range);
}

TEST(StrUtilTests, ToUint16RejectsText)
{
    EXPECT_THROW(util::to_uint16_t(L""), std::invalid_argument);
    EXPECT_THROW(util::to_uint16_t(L"port"), std::invalid_argument);
}

TEST(StrUtilTests, ToStringOfBoolAndNumbers)
{
    EXPECT_EQ(util::to_string(true), L"true");
    EXPECT_EQ(util::to_string(false), L"false");
    EXPECT_EQ(util::to_string(uint16_t{65535}), L"65535");
    EXPECT_EQ(util::to_string(-7), L"-7");
}

TEST(StrUtilTests, FileTimeIsPrintedAsUtc)
{
    EXPECT_EQ(util::to_string(file_time::to_file_time(0)), "Thursday, 01 January 1970 00:00");
}

TEST(StrUtilTests, WildmatchIsCaseSensitiveAndAnchored)
{
    EXPECT_FALSE(util::wildmatch(L"A.TXT", L"*.txt"));
    EXPECT_FALSE(util::wildmatch(L"a.txt.bak", L"*.txt"));
    EXPECT_FALSE(util::wildmatch(L"file12.txt", L"file?.txt"));
    EXPECT_TRUE(util::wildmatch(L"", L"*"));
    EXPECT_FALSE(util::wildmatch(L"x", L""));
}
