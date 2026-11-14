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
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "HttpClient.h"
#include "SshAccount.h"
#include "SyncBase.h"

namespace AFS_Core {

namespace webdav {  // [WebDav.md#webdav]

using Error = http::Error;  // [WebDav.md#errors]

struct Entry {
    std::string name;
    bool is_directory = false;
    uint64_t size = 0;
    std::time_t mtime = 0;
};

bool address_allowed(const std::string& url);  // [WebDav.md#https-only]
std::optional<Entry> stat(const SshAccount& account, const std::string& path);  // [WebDav.md#stat]
std::vector<Entry> list(const SshAccount& account, const std::string& path);  // [WebDav.md#list]
void make_directory(const SshAccount& account, const std::string& path);  // [WebDav.md#make_directory]
void remove(const SshAccount& account, const std::string& path);
void move(const SshAccount& account, const std::string& from, const std::string& to);  // [WebDav.md#move]
CopyResult download(const SshAccount& account, const std::string& path, const std::filesystem::path& local_file);  // [WebDav.md#download]
CopyResult upload(const std::filesystem::path& local_file, const SshAccount& account, const std::string& path);  // [WebDav.md#upload]

}

} // namespace AFS_Core
