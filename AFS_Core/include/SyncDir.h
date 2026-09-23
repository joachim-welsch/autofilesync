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

#include "SyncBase.h"
#include "SyncFileSsh.h"
#include <filesystem>

namespace AFS_Core {

class SyncDir: public SyncBase {
private:
    void sync_ssh_dir_entry(const SshDirEntry& entry, const std::wstring& source_path, const std::wstring& wildcard, const std::wstring& destination);
public:
    SyncDir(const SyncItem& si, std::size_t& copy_count, bool only_count);
    void operator()() override;
};

} // namespace AFS_Core
