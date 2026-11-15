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
#pragma once
#include <UtilitiesConfig.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Utilities {

namespace base64 {  // [Base64.md#alphabets]

    std::string encoded(std::span<const unsigned char> bytes);
    std::string encoded(std::string_view bytes);
    std::string encoded_unpadded(std::span<const unsigned char> bytes);
    std::string url_encoded(std::span<const unsigned char> bytes);  // [Base64.md#url_encoded]
    std::vector<unsigned char> decoded(std::string_view text);  // [Base64.md#decoded]
}

} // namespace Utilities
