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

namespace AFS_Core {

class SshAccount {
    std::string name_;
    std::string user_;
    std::string host_;
    std::string password_;
public:
    const std::string& name() const;
    const std::string& user() const;
    const std::string& host() const;
    const std::string& password() const;

    void set_name(const std::string& name);
    void set_user(const std::string& user);
    void set_host(const std::string& host);
    void set_password(const std::string& password);
};

} // namespace AFS_Core
