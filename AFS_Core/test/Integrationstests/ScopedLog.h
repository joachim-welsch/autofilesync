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
#include <Logging.h>
#include <filesystem>

namespace tu {

class ScopedLog {  // [ScopedLog.md#ScopedLog]
    std::filesystem::path path_;
    std::wstring previous_path_ = AFS_Core::Logging::instance().get_log_file_path_name();
    AFS_Core::LogLevel previous_level_ = AFS_Core::Logging::instance().get_log_level();
public:
    explicit ScopedLog(std::filesystem::path path, AFS_Core::LogLevel level = AFS_Core::LogLevel::Info): path_(std::move(path))
    {
        std::filesystem::remove(path_);
        AFS_Core::Logging::instance().set_log_file_path_name(path_.wstring());
        AFS_Core::Logging::instance().set_log_level(level);
    }
    ScopedLog(const ScopedLog&) = delete;
    ScopedLog& operator=(const ScopedLog&) = delete;
    ~ScopedLog()
    {
        AFS_Core::Logging::instance().set_log_level(previous_level_);
        AFS_Core::Logging::instance().set_log_file_path_name(previous_path_);
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    const std::filesystem::path& path() const { return path_; }
};

} // namespace tu
