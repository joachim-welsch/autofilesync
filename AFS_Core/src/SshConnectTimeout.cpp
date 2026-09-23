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
#include "SshConnectTimeout.h"

namespace AFS_Core {

namespace {
    constexpr long SSH_CONNECT_TIMEOUT_SECONDS = 15;
}

#ifdef _UNIX

void apply_ssh_connect_timeout(ssh_session session)
{
    long timeout_seconds = SSH_CONNECT_TIMEOUT_SECONDS;
    ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout_seconds);
}

#else
    #ifdef _WIN32

void apply_ssh_connect_timeout(LIBSSH2_SESSION* session)
{
    libssh2_session_set_timeout(session, SSH_CONNECT_TIMEOUT_SECONDS * 1000L);
}

    #endif
#endif

} // namespace AFS_Core
