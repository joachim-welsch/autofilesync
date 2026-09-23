/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
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
