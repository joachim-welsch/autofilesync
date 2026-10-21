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
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace AFS_Core {

struct Device {  // [Device.md#Device]
    std::string id;  // chosen by the phone when it pairs, 32 lowercase hex digits
    std::string name;  // UTF-8, what the user sees
    std::string cert_fingerprint;  // SHA-256 of the phone's DER certificate, lowercase hex [Device.md#cert_fingerprint]
    std::string ssh_host_key;  // OpenSSH public key line of the phone's SFTP server
    bool operator==(const Device&) const = default;
};
using Devices = std::vector<Device>;

struct DevicePresence {  // [Device.md#DevicePresence]
    std::string address;
    uint16_t sftp_port = 0;
    std::string sftp_user;
    std::string sftp_password;  // one-time, for the pass the phone is serving [Device.md#serving]
    std::string app_version;
    std::time_t last_seen = 0;
    bool serving = false;
};

bool valid_device_id(const std::string& id);  // [Device.md#valid_device_id]

} // namespace AFS_Core
