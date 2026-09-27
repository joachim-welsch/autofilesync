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

#include <string>
#include <vector>
#ifdef _WXWIDGETS
    #include <wx/app.h>
#endif

namespace AFS_Core {

using StringVector = std::vector<std::wstring>;
using WStringVector = std::vector<std::wstring>;

enum class SyncType {
    None,
    CountFiles,
    Single,
    Continuous
};

using StrVect = std::vector<std::wstring>;

#ifdef WIN32
    constexpr wchar_t DIR_SEPARATOR = '\\';
#else
    constexpr wchar_t DIR_SEPARATOR = L'/';
#endif

#ifdef _WXWIDGETS
    class Global {
        wxApp*       app_;
        StringVector args_;
        Global() = default;
        Global(const Global&) = delete;
        Global& operator=(const Global&) = delete;
        Global(Global&&) = delete;
        Global& operator=(Global&&) = delete;
        ~Global() = default;
    public:
        static Global& instance();
        void set_app(wxApp* app) { app_ = app; }
        wxApp* get_app() const { return app_; }
    };
#endif

} // namespace AFS_Core
