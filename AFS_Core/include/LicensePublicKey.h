/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "License.h"

namespace AFS_Core::license {

constexpr std::array<unsigned char, PUBLIC_KEY_SIZE> PRODUCT_PUBLIC_KEY = {  // [License.md#PRODUCT_PUBLIC_KEY]
    0xde, 0x54, 0x09, 0x6c, 0x30, 0x7d, 0x06, 0x57, 0x9e, 0xde, 0x78, 0xc3, 0x44, 0xed, 0x23, 0xda,
    0xe0, 0xce, 0xac, 0x79, 0x9f, 0xa4, 0x6c, 0x53, 0x1a, 0xb4, 0x04, 0x1a, 0x2f, 0x5a, 0x2b, 0xcd
};

} // namespace AFS_Core::license
