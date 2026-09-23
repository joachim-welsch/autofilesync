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
#include "FileDelete.h"
#include "Config.h"
#include <RecycleBin.h>
#include <Util.h>
#include <Logging.h>
#include <Translation.h>
#include <filesystem>

namespace AFS_Core {

namespace fs = std::filesystem;

bool delete_file(const std::wstring& pathname)
{
    if (Config::instance().settings().delete_mode() == DeleteMode::Permanent)
        return util::remove_file(pathname, LogLevel::Error);
    const fs::path source(pathname);
    const fs::path item = util::with_timestamp_prefix(source.relative_path());  // [FileDelete.md#recycle-bin-layout]
    if (!RecycleBin::instance().add_item(source, item)) {
        log_error(translation::translate("file_move_to_recycle_bin_failed"), pathname);
        return false;
    }
    log_info(translation::translate("file_moved_to_recycle_bin"), pathname);
    return true;
}

} // namespace AFS_Core
