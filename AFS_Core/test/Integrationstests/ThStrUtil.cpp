/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "ThStrUtil.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace tu {
    std::string to_utf8(const std::wstring& in)
    {
#ifdef _WIN32
        if (in.empty()) return {};
        const int n = ::WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in.size()),
            nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(n), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in.size()),
            out.data(), n, nullptr, nullptr);
        return out;
#else
        return std::filesystem::path(in).string();   // wchar_t(32bit) -> UTF-8
#endif
    }
}