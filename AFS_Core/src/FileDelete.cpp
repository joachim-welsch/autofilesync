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
#include "pch.h"
#include "FileDelete.h"
#include "Config.h"
#include <RecycleBin.h>
#include <FileName.h>
#include <FilesystemUtil.h>
#include <Logging.h>
#include <Translation.h>
#include <chrono>
#include <filesystem>

namespace AFS_Core {

namespace fs = std::filesystem;

static fs::path free_bin_item(const fs::path& origin)  // [FileDelete.md#free_bin_item]
{
    auto time = std::chrono::system_clock::now();
    fs::path item = file_name::with_timestamp_prefix(origin, time);
    while (RecycleBin::instance().has_item(item))
        item = file_name::with_timestamp_prefix(origin, time += std::chrono::milliseconds(1));
    return item;
}

bool delete_file(const std::wstring& pathname)
{
    return delete_refusal(pathname) == fsu::FileInfoResult::OK;
}

fsu::FileInfoResult delete_refusal(const std::wstring& pathname)  // [FileDelete.md#delete_refusal]
{
    if (Config::instance().settings().delete_mode() == DeleteMode::Permanent)
        return fsu::remove_refusal(pathname);
    const fs::path source(pathname);
    const fs::path item = free_bin_item(source.relative_path());  // [FileDelete.md#recycle-bin-layout]
    const fsu::FileInfoResult refusal = RecycleBin::instance().add_refusal(source, item);
    if (refusal == fsu::FileInfoResult::OK)
        log_info(translation::translate("file_moved_to_recycle_bin"), pathname);
    else if (refusal != fsu::FileInfoResult::FileNotExist)  // [FileDelete.md#delete_refusal]
        log_error(translation::translate("file_move_to_recycle_bin_failed"), pathname);
    return refusal;
}

} // namespace AFS_Core
