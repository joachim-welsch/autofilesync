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
#include "UserObject.h"
#ifdef _WIN32
#include <windows.h>
#include <sddl.h>
#include <vector>

namespace AFS_Core {

namespace userobject {

namespace {

constexpr wchar_t NAMESPACE_ALIAS[] = L"AutoFileSync";
constexpr wchar_t BOUNDARY_NAME[] = L"AutoFileSyncUser";
constexpr int NAMESPACE_ATTEMPTS = 3;  // [UserObject.md#namespace-race]

std::vector<BYTE> user_sid()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return {};
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> user(size);
    const bool read = size > 0 && GetTokenInformation(token, TokenUser, user.data(), size, &size);
    CloseHandle(token);
    if (!read)
        return {};
    const BYTE* sid = static_cast<const BYTE*>(reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid);
    return std::vector<BYTE>(sid, sid + GetLengthSid(const_cast<BYTE*>(sid)));
}

SECURITY_ATTRIBUTES* user_only()  // [UserObject.md#user_only]
{
    static SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE};
    static const bool built = [] {
        std::vector<BYTE> sid = user_sid();
        wchar_t* sid_text = nullptr;
        if (sid.empty() || !ConvertSidToStringSidW(sid.data(), &sid_text))
            return false;
        const std::wstring sddl = L"D:P(A;;GA;;;" + std::wstring(sid_text) + L")";
        LocalFree(sid_text);
        return ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &attributes.lpSecurityDescriptor, nullptr) != FALSE;
    }();
    return built ? &attributes : nullptr;
}

HANDLE joined_namespace(HANDLE boundary)
{
    for (int attempt = 0; attempt < NAMESPACE_ATTEMPTS; ++attempt) {
        if (HANDLE created = CreatePrivateNamespaceW(user_only(), boundary, NAMESPACE_ALIAS))
            return created;
        if (HANDLE opened = OpenPrivateNamespaceW(boundary, NAMESPACE_ALIAS))
            return opened;
    }
    return nullptr;
}

HANDLE user_namespace()  // [UserObject.md#user_namespace]
{
    std::vector<BYTE> sid = user_sid();
    HANDLE boundary = sid.empty() || user_only() == nullptr ? nullptr : CreateBoundaryDescriptorW(BOUNDARY_NAME, 0);
    if (boundary == nullptr)
        return nullptr;
    HANDLE joined = AddSIDToBoundaryDescriptor(&boundary, sid.data()) ? joined_namespace(boundary) : nullptr;
    DeleteBoundaryDescriptor(boundary);
    return joined;
}

std::wstring scoped(const std::wstring& name)  // [UserObject.md#fallback]
{
    static const HANDLE held = user_namespace();
    return (held != nullptr ? std::wstring(NAMESPACE_ALIAS) + L"\\" : std::wstring(L"Global\\")) + name;
}

} // namespace

void* create_event(const std::wstring& name)
{
    return CreateEventW(user_only(), TRUE, TRUE, scoped(name).c_str());
}

bool event_exists(const std::wstring& name)
{
    HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, scoped(name).c_str());
    if (event == nullptr)
        return GetLastError() != ERROR_FILE_NOT_FOUND;
    CloseHandle(event);
    return true;
}

void* acquire_mutex(const std::wstring& name)
{
    HANDLE mutex = CreateMutexW(user_only(), TRUE, scoped(name).c_str());
    if (mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return nullptr;
    }
    return mutex;
}

} // namespace userobject

} // namespace AFS_Core
#endif
