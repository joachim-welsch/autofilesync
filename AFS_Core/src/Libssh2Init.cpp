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
#include "Libssh2Init.h"
#ifdef _WIN32
#include <libssh2.h>
#include <mutex>
#include <string>
#include "Logging.h"

namespace AFS_Core {

void ensure_libssh2_initialized()
{
    static std::once_flag init_once;
    std::call_once(init_once, []() {
        const int rc = libssh2_init(0);
        if (rc != 0)
            log_error(std::string("libssh2_init failed: ") + std::to_string(rc));
    });
}

} // namespace AFS_Core
#endif
