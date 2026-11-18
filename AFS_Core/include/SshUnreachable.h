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
#include <stdexcept>
#include <string>
#include <FilesystemUtil.h>

namespace AFS_Core {

struct RemoteFailure : std::runtime_error {  // [SftpConnection.md#RemoteFailure]
    Utilities::FilesystemUtil::FileInfoResult reason;
    RemoteFailure(Utilities::FilesystemUtil::FileInfoResult why, const std::string& message) : std::runtime_error(message), reason(why) {}
};

struct SshUnreachable : RemoteFailure {  // [SftpConnection.md#SshUnreachable]
    explicit SshUnreachable(const std::string& message, Utilities::FilesystemUtil::FileInfoResult why = Utilities::FilesystemUtil::FileInfoResult::ServerUnreachable)
        : RemoteFailure(why, message) {}
};

} // namespace AFS_Core
