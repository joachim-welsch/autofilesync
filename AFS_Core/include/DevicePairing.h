/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

namespace AFS_Core {

class DevicePairing {  // [DevicePairing.md#DevicePairing]
    mutable std::mutex mutex_;
    std::string token_;
    std::chrono::steady_clock::time_point deadline_{};
    std::time_t expires_ = 0;
    int failures_ = 0;
    bool open_unlocked() const;
public:
    static constexpr std::chrono::minutes TOKEN_LIFETIME{10};
    static constexpr int FAILURE_LIMIT = 5;  // [DevicePairing.md#FAILURE_LIMIT]
    std::string begin();  // [DevicePairing.md#begin]
    void end();
    bool open() const;
    bool redeem(const std::string& token);  // [DevicePairing.md#redeem]
    std::string token() const;
    std::time_t expires() const;
};

struct PairingOffer {  // [DevicePairing.md#PairingOffer]
    std::string pc_name;
    std::vector<std::string> addresses;
    uint16_t port = 0;
    std::string fingerprint;
    std::string token;
    std::time_t expires = 0;
};

inline constexpr const char* PAIRING_URL = "https://welsch-software.com/pair";  // [DevicePairing.md#qr_content]
std::string qr_content(const PairingOffer& offer);
std::string random_hex(size_t bytes);
std::string pc_name();
std::vector<std::string> local_ipv4_addresses();  // [DevicePairing.md#local_ipv4_addresses]

} // namespace AFS_Core
