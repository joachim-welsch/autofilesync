/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#pragma once

#include "License.h"

namespace AFS_Core::license {

constexpr std::array<unsigned char, PUBLIC_KEY_SIZE> PRODUCT_PUBLIC_KEY = {  // [License.md#PRODUCT_PUBLIC_KEY]
    0xde, 0x54, 0x09, 0x6c, 0x30, 0x7d, 0x06, 0x57, 0x9e, 0xde, 0x78, 0xc3, 0x44, 0xed, 0x23, 0xda,
    0xe0, 0xce, 0xac, 0x79, 0x9f, 0xa4, 0x6c, 0x53, 0x1a, 0xb4, 0x04, 0x1a, 0x2f, 0x5a, 0x2b, 0xcd
};

} // namespace AFS_Core::license
