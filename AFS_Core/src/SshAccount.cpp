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
#include "pch.h"
#include "SshAccount.h"

namespace AFS_Core {

const std::string& SshAccount::name() const
{
    return name_;
}

const std::string& SshAccount::user() const
{
    return user_;
}

const std::string& SshAccount::host() const
{
    return host_;
}

const std::string& SshAccount::password() const
{
    return password_;
}

void SshAccount::set_name(const std::string& name)
{
    name_ = name;
}

void SshAccount::set_user(const std::string& user)
{
    user_ = user;
}

void SshAccount::set_host(const std::string& host)
{
    host_ = host;
}

void SshAccount::set_password(const std::string& password)
{
    password_ = password;
}

} // namespace AFS_Core
