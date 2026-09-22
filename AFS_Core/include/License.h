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
