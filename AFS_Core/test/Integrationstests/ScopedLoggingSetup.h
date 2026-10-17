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
#include "Logging.h"
#include <string>

namespace tu {
class ScopedLoggingSetup {  // [ScopedLoggingSetup.md#ScopedLoggingSetup]
    AFS_Core::Logging& log_ = AFS_Core::Logging::instance();
    const AFS_Core::LogLevel level_ = log_.get_log_level();
    const std::wstring file_ = log_.get_log_file_path_name();
    const std::wstring editor_ = log_.get_editor_path_name();
    const uint16_t retention_ = log_.retention_days();
    const bool notify_active_ = log_.email().notify_active();
    const std::wstring notify_address_ = log_.email().notify_address();
    const AFS_Core::SmtpAccount smtp_ = log_.email().smtp_account();
public:
    ScopedLoggingSetup() = default;
    ScopedLoggingSetup(const ScopedLoggingSetup&) = delete;
    ScopedLoggingSetup& operator=(const ScopedLoggingSetup&) = delete;
    ~ScopedLoggingSetup()
    {
        log_.set_log_level(level_);
        log_.set_log_file_path_name(file_);
        log_.set_editor_path_name(editor_);
        log_.retention_days(retention_);
        log_.email().notify_active(notify_active_);
        log_.email().notify_address(notify_address_);
        log_.email().smtp_account(smtp_);
    }
};
} // namespace tu
