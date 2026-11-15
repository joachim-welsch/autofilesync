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
#include <Hex.h>

using namespace AFS_Core;

TEST(HexTests, EncodesTwoLowercaseDigitsPerByte)  // [Hex.md#encoded]
{
    const std::vector<unsigned char> bytes = {0x00, 0x0F, 0x10, 0xAB, 0xFF};
    EXPECT_EQ(hex::encoded(bytes), "000f10abff");
}

TEST(HexTests, EncodesNothingAsEmptyText)
{
    EXPECT_EQ(hex::encoded(std::vector<unsigned char>{}), "");
    EXPECT_EQ(hex::encoded(""), "");
}

TEST(HexTests, EncodesTheBytesOfText)
{
    EXPECT_EQ(hex::encoded("AZ"), "415a");
    EXPECT_EQ(hex::encoded(std::string("\x00\xFE", 2)), "00fe");
}
