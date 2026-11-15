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
#include <PercentEncoding.h>

using namespace AFS_Core;

TEST(PercentEncodingTests, EncodedKeepsUnreservedCharacters)
{
    const std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    EXPECT_EQ(percent::encoded(unreserved), unreserved);
}

TEST(PercentEncodingTests, EncodedEscapesEverythingElseWithUppercaseDigits)  // [PercentEncoding.md#encoded]
{
    EXPECT_EQ(percent::encoded("a b/c?d=e&f+g%"), "a%20b%2Fc%3Fd%3De%26f%2Bg%25");
    EXPECT_EQ(percent::encoded("Büro"), "B%C3%BCro");
    EXPECT_EQ(percent::encoded(std::string("\x00\x7F", 2)), "%00%7F");
    EXPECT_EQ(percent::encoded(""), "");
}

TEST(PercentEncodingTests, DecodedReversesEncoded)
{
    std::string every_byte;
    for (int value = 0; value < 256; ++value)
        every_byte += static_cast<char>(value);
    EXPECT_EQ(percent::decoded(percent::encoded(every_byte)), every_byte);
}

TEST(PercentEncodingTests, DecodedAcceptsLowercaseDigits)
{
    EXPECT_EQ(percent::decoded("B%c3%bcro%2f"), "Büro/");
}

TEST(PercentEncodingTests, DecodedLeavesIncompleteSequencesAlone)  // [PercentEncoding.md#decoded]
{
    EXPECT_EQ(percent::decoded("100%"), "100%");
    EXPECT_EQ(percent::decoded("%4"), "%4");
    EXPECT_EQ(percent::decoded("%zz%41"), "%zzA");
    EXPECT_EQ(percent::decoded("a+b"), "a+b");
    EXPECT_EQ(percent::decoded("%2541"), "%41");
}
