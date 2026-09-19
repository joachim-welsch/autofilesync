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

#include <string>
#include <optional>
#include <filesystem>

namespace AFS_Core {

namespace passwordcrypto {  // [PasswordCrypto.md#passwordcrypto]

std::string encrypt(const std::string& plaintext, const std::filesystem::path& key_file);

bool is_encrypted(const std::string& stored);  // [PasswordCrypto.md#is_encrypted]

std::optional<std::string> decrypt(const std::string& stored, const std::filesystem::path& key_file);  // [PasswordCrypto.md#decrypt]

} // namespace passwordcrypto

} // namespace AFS_Core
