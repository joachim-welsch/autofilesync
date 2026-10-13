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
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <set>
#include "Logging.h"
#ifdef WIN32
    #include <Windows.h>
#elif defined(_MACOS)
    #include <mach-o/dyld.h>
    #include <climits>
#endif

namespace AFS_Core {

namespace {
constexpr const char* TEXT_DOMAIN = "autofilesync";

bool names_no_language(const char* locale)
{
    return locale == nullptr || std::strcmp(locale, "C") == 0 || std::strcmp(locale, "POSIX") == 0;
}

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
void init_locale([[maybe_unused]] std::span<const char* const> fallbacks)
{
    std::setlocale(LC_ALL, "");
#ifdef LC_MESSAGES  // POSIX only; MSVC keeps whatever the environment gave
    if (!names_no_language(std::setlocale(LC_MESSAGES, nullptr)))
        return;
    for (const char* candidate : fallbacks)
        if (std::setlocale(LC_ALL, candidate) != nullptr)  // [Translation.md#init_locale]
            return;
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
namespace {
std::optional<std::string> environment_language;  // [Translation.md#environment_language]

bool catalogue_bound()
{
    return *dgettext(TEXT_DOMAIN, "") != '\0';  // the empty msgid yields the catalogue header
}

void report_missing(const char* msgid)  // [Translation.md#missing-translation]
{
    static std::mutex mutex;
    static std::set<std::string> reported;
    thread_local bool reporting = false;
    if (reporting || !catalogue_bound())
        return;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if (!reported.insert(msgid).second)
            return;
    }
    reporting = true;
    log_warning(L"Übersetzung fehlt: " + util::string_to_wstring(msgid));
    reporting = false;
}

void put_language(const std::optional<std::string>& language)
{
#ifdef WIN32
    _putenv_s("LANGUAGE", language.value_or("").c_str());  // an empty value removes the variable
#else
    if (language)
        setenv("LANGUAGE", language->c_str(), 1);
    else
        unsetenv("LANGUAGE");
#endif
}

void fall_back_to_english()  // [Translation.md#fall_back_to_english]
{
    if (!catalogue_bound())
        put_language("en");
}
}

void init(const std::filesystem::path& locale_dir)
{
    const char* language = std::getenv("LANGUAGE");
    environment_language = language != nullptr ? std::optional<std::string>(language) : std::nullopt;
    if (!locale_dir.empty())
        bindtextdomain(TEXT_DOMAIN, locale_dir.string().c_str());
    bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");  // [Translation.md#catalogue-codeset]
    textdomain(TEXT_DOMAIN);
    fall_back_to_english();
}

void set_language(const std::string& code)
{
    put_language(code.empty() ? environment_language : std::optional<std::string>(code));
    textdomain(TEXT_DOMAIN);  // [Translation.md#set_language]
    fall_back_to_english();
}

std::string translate_utf8(const char* msgid)
{
    const char* text = dgettext(TEXT_DOMAIN, msgid);  // [Translation.md#dgettext-instead-of-gettext]
    if (text == msgid)  // gettext hands the msgid itself back when the catalogue has no entry
        report_missing(msgid);
    return text;
}

std::optional<std::wstring> catalogue_mismatch()
{
#ifdef AFS_CATALOGUE_STAMP
    if (!catalogue_bound() || translate_utf8("catalogue_stamp") == AFS_CATALOGUE_STAMP)  // [Translation.md#catalogue_mismatch]
        return std::nullopt;
    const char* directory = bindtextdomain(TEXT_DOMAIN, nullptr);
    const std::wstring where = util::string_to_wstring(directory != nullptr ? directory : "");
    const std::wstring message = translate("catalogue_outdated", {where});
    if (message != L"catalogue_outdated")
        return message;
    return L"The message catalogue in " + where + L" does not belong to this version of Autofilesync; install the locale files of this build.";  // [Translation.md#english-safety-net]
#else
    return std::nullopt;
#endif
}
#else
std::optional<std::wstring> catalogue_mismatch() { return std::nullopt; }
void init(const std::filesystem::path&) {}  // [Translation.md#no-libintl-fallback]
void set_language(const std::string&) {}

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
