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
#include "CloudStore.h"
#include "GoogleDrive.h"

namespace AFS_Core {

namespace cloud_store {

bool handles(const SshAccount& account)
{
    return account.webdav() || account.google_drive();
}

std::optional<Entry> stat(const SshAccount& account, const std::string& path)
{
    return account.google_drive() ? google_drive::stat(account, path) : webdav::stat(account, path);
}

std::vector<Entry> list(const SshAccount& account, const std::string& path)
{
    return account.google_drive() ? google_drive::list(account, path) : webdav::list(account, path);
}

void make_directory(const SshAccount& account, const std::string& path)
{
    account.google_drive() ? google_drive::make_directory(account, path) : webdav::make_directory(account, path);
}

void remove(const SshAccount& account, const std::string& path)
{
    account.google_drive() ? google_drive::remove(account, path) : webdav::remove(account, path);
}

void move(const SshAccount& account, const std::string& from, const std::string& to)
{
    account.google_drive() ? google_drive::move(account, from, to) : webdav::move(account, from, to);
}

CopyResult download(const SshAccount& account, const std::string& path, const std::filesystem::path& local_file)
{
    return account.google_drive() ? google_drive::download(account, path, local_file) : webdav::download(account, path, local_file);
}

CopyResult upload(const std::filesystem::path& local_file, const SshAccount& account, const std::string& path)
{
    return account.google_drive() ? google_drive::upload(local_file, account, path) : webdav::upload(local_file, account, path);
}

}

} // namespace AFS_Core
