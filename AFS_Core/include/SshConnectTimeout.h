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
#pragma once

#ifdef _UNIX
    #include <libssh/libssh.h>
#else
    #ifdef _WIN32
        #include <libssh2.h>
    #endif
#endif

namespace AFS_Core {

#ifdef _UNIX
void apply_ssh_connect_timeout(ssh_session session);  // [SshConnectTimeout.md#apply_ssh_connect_timeout]
#else
    #ifdef _WIN32
void apply_ssh_connect_timeout(LIBSSH2_SESSION* session);  // [SshConnectTimeout.md#windows-timeout-scope]
    #endif
#endif

} // namespace AFS_Core
