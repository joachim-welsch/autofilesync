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
#include <filesystem>
#include <optional>
#include <string>

struct x509_st;

namespace AFS_Core {

class DeviceIdentity {  // [DeviceIdentity.md#DeviceIdentity]
    std::string certificate_pem_;
    std::string key_pem_;
    std::string fingerprint_;
public:
    static std::optional<DeviceIdentity> load_or_create(const std::filesystem::path& dir);  // [DeviceIdentity.md#load_or_create]
    const std::string& certificate_pem() const { return certificate_pem_; }
    const std::string& key_pem() const { return key_pem_; }
    const std::string& fingerprint() const { return fingerprint_; }  // SHA-256 of the DER certificate, lowercase hex
};

std::string certificate_fingerprint(x509_st* certificate);  // [DeviceIdentity.md#certificate_fingerprint]

} // namespace AFS_Core
