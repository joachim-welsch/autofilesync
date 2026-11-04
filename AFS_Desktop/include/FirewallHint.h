/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

bool ufw_blocks_incoming(const std::filesystem::path& etc = "/etc");  // [FirewallHint.md#ufw_blocks_incoming]
std::vector<std::string> local_ipv4_networks();  // [FirewallHint.md#local_ipv4_networks]
std::string ufw_allow_command(const std::vector<std::string>& networks, uint16_t port);  // [FirewallHint.md#ufw_allow_command]
