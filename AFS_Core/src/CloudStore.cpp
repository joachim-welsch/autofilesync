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
#include "Dropbox.h"
#include "GoogleDrive.h"
#include "OneDrive.h"

namespace AFS_Core {

namespace cloud_store {

namespace {
template <typename WebDav, typename Google, typename Microsoft, typename Dropbox>
auto by_kind(const SshAccount& account, WebDav webdav, Google google, Microsoft microsoft, Dropbox dropbox)  // [CloudStore.md#by_kind]
{
    if (account.google_drive())
        return google();
    if (account.one_drive())
        return microsoft();
    return account.dropbox() ? dropbox() : webdav();
}
}

bool handles(const SshAccount& account)
{
    return account.webdav() || account.google_drive() || account.one_drive() || account.dropbox();
}

std::optional<Entry> stat(const SshAccount& account, const std::string& path)
{
    return by_kind(account, [&] { return webdav::stat(account, path); }, [&] { return google_drive::stat(account, path); },
        [&] { return one_drive::stat(account, path); }, [&] { return dropbox::stat(account, path); });
}

std::vector<Entry> list(const SshAccount& account, const std::string& path)
{
    return by_kind(account, [&] { return webdav::list(account, path); }, [&] { return google_drive::list(account, path); },
        [&] { return one_drive::list(account, path); }, [&] { return dropbox::list(account, path); });
}

void make_directory(const SshAccount& account, const std::string& path)
{
    by_kind(account, [&] { webdav::make_directory(account, path); }, [&] { google_drive::make_directory(account, path); },
        [&] { one_drive::make_directory(account, path); }, [&] { dropbox::make_directory(account, path); });
}

void remove(const SshAccount& account, const std::string& path)
{
    by_kind(account, [&] { webdav::remove(account, path); }, [&] { google_drive::remove(account, path); },
        [&] { one_drive::remove(account, path); }, [&] { dropbox::remove(account, path); });
}

void move(const SshAccount& account, const std::string& from, const std::string& to)
{
    by_kind(account, [&] { webdav::move(account, from, to); }, [&] { google_drive::move(account, from, to); },
        [&] { one_drive::move(account, from, to); }, [&] { dropbox::move(account, from, to); });
}

CopyResult download(const SshAccount& account, const std::string& path, const std::filesystem::path& local_file)
{
    return by_kind(account, [&] { return webdav::download(account, path, local_file); }, [&] { return google_drive::download(account, path, local_file); },
        [&] { return one_drive::download(account, path, local_file); }, [&] { return dropbox::download(account, path, local_file); });
}

CopyResult upload(const std::filesystem::path& local_file, const SshAccount& account, const std::string& path)
{
    return by_kind(account, [&] { return webdav::upload(local_file, account, path); }, [&] { return google_drive::upload(local_file, account, path); },
        [&] { return one_drive::upload(local_file, account, path); }, [&] { return dropbox::upload(local_file, account, path); });
}

}

} // namespace AFS_Core
