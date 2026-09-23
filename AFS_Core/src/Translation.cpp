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
#include "Translation.h"
#include "Util.h"
#ifdef AFS_WITH_GETTEXT
    #include <libintl.h>
#endif
#include <clocale>
#include <cstring>
#ifdef WIN32
    #include <Windows.h>
#elif defined(_MACOS)
    #include <mach-o/dyld.h>
    #include <climits>
#endif

namespace AFS_Core {

namespace {
constexpr const char* TEXT_DOMAIN = "autofilesync";

std::filesystem::path executable_path()  // [Translation.md#executable_path]
{
#ifdef WIN32
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0)
            return {};
        if (length < path.size())
            return std::filesystem::path(path.c_str());
        path.resize(path.size() * 2);
    }
#elif defined(_MACOS)
    std::string path(PATH_MAX, '\0');
    auto size = static_cast<uint32_t>(path.size());
    if (_NSGetExecutablePath(path.data(), &size) != 0) {  // size then holds the length it needs
        path.resize(size);
        if (_NSGetExecutablePath(path.data(), &size) != 0)
            return {};
    }
    return std::filesystem::path(path.c_str());
#else
    std::error_code ec;
    return std::filesystem::read_symlink("/proc/self/exe", ec);  // empty on error, which locale_dir() handles
#endif
}

template<class Str>
Str with_values(const Str& text, const std::initializer_list<Str>& values)  // [Translation.md#placeholders]
{
    using Char = typename Str::value_type;
    Str retval;
    retval.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const bool digit_follows = i + 1 < text.size() && text[i + 1] >= Char('1') && text[i + 1] <= Char('9');
        const std::size_t index = digit_follows ? static_cast<std::size_t>(text[i + 1] - Char('1')) : values.size();
        if (text[i] != Char('%') || index >= values.size()) {
            retval += text[i];
            continue;
        }
        retval += *(values.begin() + index);
        ++i;
    }
    return retval;
}
}

namespace translation {
void init_locale(const char* fallback)
{
    std::setlocale(LC_ALL, "");
#ifdef LC_MESSAGES  // POSIX only; MSVC keeps whatever the environment gave
    const char* language = std::setlocale(LC_MESSAGES, nullptr);
    if (language == nullptr || std::strcmp(language, "C") == 0 || std::strcmp(language, "POSIX") == 0)
        std::setlocale(LC_ALL, fallback);  // [Translation.md#init_locale]
#endif
}

std::filesystem::path locale_dir()
{
    const std::filesystem::path executable = executable_path();
    if (executable.empty())
        return {};
    std::error_code ec;
    const std::filesystem::path candidate = executable.parent_path() / "locale";
    return std::filesystem::is_directory(candidate, ec) ? candidate : std::filesystem::path();
}

#ifdef AFS_WITH_GETTEXT
void init(const std::filesystem::path& locale_dir)
{
    if (locale_dir.empty())
        return;
    bindtextdomain(TEXT_DOMAIN, locale_dir.string().c_str());
    bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");  // [Translation.md#catalogue-codeset]
    textdomain(TEXT_DOMAIN);
}

std::string translate_utf8(const char* msgid)
{
    return dgettext(TEXT_DOMAIN, msgid);  // [Translation.md#dgettext-instead-of-gettext]
}
#else
void init(const std::filesystem::path&) {}  // [Translation.md#no-libintl-fallback]

std::string translate_utf8(const char* msgid)
{
    return msgid;
}
#endif

std::wstring translate(const char* msgid)
{
    return util::string_to_wstring(translate_utf8(msgid));
}

std::wstring translate(const char* msgid, std::initializer_list<std::wstring> values)
{
    return with_values(translate(msgid), values);
}

std::string translate_utf8(const char* msgid, std::initializer_list<std::string> values)
{
    return with_values(translate_utf8(msgid), values);
}
}

} // namespace AFS_Core
