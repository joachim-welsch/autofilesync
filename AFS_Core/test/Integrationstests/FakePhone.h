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
#include "DeviceIdentity.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tu {

struct SftpOffer {  // [FakePhone.md#SftpOffer] outside the class: a nested struct's default initialisers are not usable in the class's own default arguments
    uint16_t port = 52382;
    std::string user = "afs";
    std::string password = "one-time";
    bool serving = false;
};

class FakePhone {  // [FakePhone.md#FakePhone]
public:
    struct Reply {
        bool connected = false;
        std::vector<std::string> lines;
        std::string server_fingerprint;
        std::string type(size_t line = 0) const;  // [FakePhone.md#type]
        std::string field(const std::string& path, size_t line = 0) const;
    };
    explicit FakePhone(const std::filesystem::path& identity_dir);
    const std::string& fingerprint() const { return identity_.fingerprint(); }
    Reply send(uint16_t port, const std::string& line, bool with_certificate = true) const;  // [FakePhone.md#send]
    static std::string pair_line(const std::string& token, const std::string& id, const std::string& name, const std::string& host_key = "ecdsa-sha2-nistp256 AAAAE2VjZHNh");
    static std::string hello_line(const std::string& id, const SftpOffer& sftp = SftpOffer());
private:
    AFS_Core::DeviceIdentity identity_;
};

} // namespace tu
