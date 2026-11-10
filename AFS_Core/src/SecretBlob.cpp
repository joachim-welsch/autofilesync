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
#include "SecretBlob.h"
#ifdef _WIN32
    #include <windows.h>
    #include <wincrypt.h>
    #include <dpapi.h>
#endif

namespace AFS_Core {

namespace secretblob {

#ifdef _WIN32
namespace {

const std::string MARKER = "AFSDPAPI1\n";  // [SecretBlob.md#marker]

bool marked(const std::string& stored)
{
    return stored.rfind(MARKER, 0) == 0;
}

std::optional<std::string> taken(DATA_BLOB& blob)
{
    std::string content(reinterpret_cast<const char*>(blob.pbData), blob.cbData);
    SecureZeroMemory(blob.pbData, blob.cbData);
    LocalFree(blob.pbData);
    return content;
}

DATA_BLOB blob_of(const std::string& text)
{
    return {static_cast<DWORD>(text.size()), reinterpret_cast<BYTE*>(const_cast<char*>(text.data()))};
}

} // namespace

std::optional<std::string> protect(const std::string& plain)
{
    DATA_BLOB in = blob_of(plain), out{};
    if (!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return std::nullopt;
    return MARKER + *taken(out);
}

std::optional<std::string> unprotect(const std::string& stored)
{
    if (!marked(stored))
        return stored;
    const std::string sealed = stored.substr(MARKER.size());
    DATA_BLOB in = blob_of(sealed), out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return std::nullopt;
    return taken(out);
}

bool outdated(const std::string& stored)
{
    return !marked(stored);
}
#else
std::optional<std::string> protect(const std::string& plain)
{
    return plain;
}

std::optional<std::string> unprotect(const std::string& stored)
{
    return stored;
}

bool outdated(const std::string&)
{
    return false;
}
#endif

} // namespace secretblob

} // namespace AFS_Core
