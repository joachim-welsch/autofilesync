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
#include <Base64.h>

using namespace AFS_Core;

namespace {
std::vector<unsigned char> bytes_of(std::string_view text)
{
    return {text.begin(), text.end()};
}
}

TEST(Base64Tests, EncodesTheRfc4648Vectors)
{
    EXPECT_EQ(base64::encoded(""), "");
    EXPECT_EQ(base64::encoded("f"), "Zg==");
    EXPECT_EQ(base64::encoded("fo"), "Zm8=");
    EXPECT_EQ(base64::encoded("foo"), "Zm9v");
    EXPECT_EQ(base64::encoded("foob"), "Zm9vYg==");
    EXPECT_EQ(base64::encoded("fooba"), "Zm9vYmE=");
    EXPECT_EQ(base64::encoded("foobar"), "Zm9vYmFy");
}

TEST(Base64Tests, DecodesTheRfc4648Vectors)
{
    EXPECT_TRUE(base64::decoded("").empty());
    EXPECT_EQ(base64::decoded("Zg=="), bytes_of("f"));
    EXPECT_EQ(base64::decoded("Zm8="), bytes_of("fo"));
    EXPECT_EQ(base64::decoded("Zm9v"), bytes_of("foo"));
    EXPECT_EQ(base64::decoded("Zm9vYg=="), bytes_of("foob"));
    EXPECT_EQ(base64::decoded("Zm9vYmE="), bytes_of("fooba"));
    EXPECT_EQ(base64::decoded("Zm9vYmFy"), bytes_of("foobar"));
}

TEST(Base64Tests, RoundTripsEveryByteValue)
{
    std::vector<unsigned char> bytes;
    for (int value = 0; value < 256; ++value)
        bytes.push_back(static_cast<unsigned char>(value));
    for (std::size_t length = 0; length <= bytes.size(); ++length) {
        const std::vector<unsigned char> part(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(length));
        EXPECT_EQ(base64::decoded(base64::encoded(part)), part);
    }
}

TEST(Base64Tests, EncodesBytesAboveSevenBitsFromText)
{
    EXPECT_EQ(base64::encoded("\xFF\xFE\xFD"), "//79");
    EXPECT_EQ(base64::encoded("ü"), "w7w=");
}

TEST(Base64Tests, UnpaddedLeavesOnlyThePaddingAway)
{
    EXPECT_EQ(base64::encoded_unpadded(bytes_of("f")), "Zg");
    EXPECT_EQ(base64::encoded_unpadded(bytes_of("fo")), "Zm8");
    EXPECT_EQ(base64::encoded_unpadded(bytes_of("foo")), "Zm9v");
}

TEST(Base64Tests, UrlEncodedUsesTheUrlAlphabetWithoutPadding)
{
    const std::vector<unsigned char> bytes = {0xFB, 0xFF, 0xFE, 0x3E};
    EXPECT_EQ(base64::encoded(bytes), "+//+Pg==");
    EXPECT_EQ(base64::url_encoded(bytes), "-__-Pg");
}

TEST(Base64Tests, DecodedRejectsInvalidText)  // [Base64.md#decoded]
{
    EXPECT_TRUE(base64::decoded("Zm9").empty());
    EXPECT_TRUE(base64::decoded("Zm9v\n").empty());
    EXPECT_TRUE(base64::decoded("Zm 9").empty());
    EXPECT_TRUE(base64::decoded("Zm-_").empty());
    EXPECT_TRUE(base64::decoded("Z===").empty());
    EXPECT_TRUE(base64::decoded("Zg=v").empty());
    EXPECT_TRUE(base64::decoded("====").empty());
}
