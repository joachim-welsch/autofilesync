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

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace AFS_Core::license {  // [License.md#license]

constexpr char LICENSE_FILE_NAME[] = "autofilesync.lic";
constexpr std::size_t PUBLIC_KEY_SIZE = 32;
constexpr std::size_t SIGNATURE_SIZE = 64;
using PublicKey = std::span<const unsigned char, PUBLIC_KEY_SIZE>;

struct License {
    std::wstring licensee;  // empty: a signed licence for private use [License.md#private-use]
    bool private_use() const { return licensee.empty(); }
};

std::optional<License> read(const std::filesystem::path& file, PublicKey public_key);  // [License.md#read]
std::optional<License> find(const std::vector<std::filesystem::path>& directories, PublicKey public_key);  // [License.md#find]
bool verify(std::span<const unsigned char> payload, std::span<const unsigned char> signature, PublicKey public_key);  // [License.md#verify]
std::string sign_for_test(std::span<const unsigned char> payload, std::array<unsigned char, PUBLIC_KEY_SIZE>& public_key);  // [License.md#sign_for_test]

} // namespace AFS_Core::license
