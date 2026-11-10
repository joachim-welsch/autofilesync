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
#include <string>

namespace AFS_Core {

class SshAccount {
    std::string name_;
    std::string user_;
    std::string host_;
    std::string password_;
    uint16_t port_ = 22;
    std::string host_key_;  // [SshAccount.md#host_key_]
    std::string sealed_password_;  // [SshAccount.md#sealed_password_]
public:
    const std::string& name() const;
    const std::string& user() const;
    const std::string& host() const;
    const std::string& password() const;
    uint16_t port() const { return port_; }
    const std::string& host_key() const { return host_key_; }
    const std::string& sealed_password() const { return sealed_password_; }

    void set_name(const std::string& name);
    void set_user(const std::string& user);
    void set_host(const std::string& host);
    void set_password(const std::string& password);
    void set_port(uint16_t port) { port_ = port; }
    void set_host_key(const std::string& key) { host_key_ = key; }
    void set_sealed_password(const std::string& ciphertext) { sealed_password_ = ciphertext; }
};

} // namespace AFS_Core
