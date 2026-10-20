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
#include "SyncState.h"
#include "Config.h"
#include "Logging.h"
#include "Translation.h"
#include "Util.h"
#include <algorithm>
#include <charconv>
#include <utility>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace AFS_Core {

namespace fs = std::filesystem;
using std::wstring;
using std::wstring_view;

namespace {
    constexpr wstring_view MAGIC = L"autofilesync-sync-state";
    constexpr wstring_view SOURCE_KEY = L"source";
    constexpr wstring_view TARGET_KEY = L"target";
    constexpr wchar_t DEFAULT_FILE_NAME[] = L"state";
    constexpr wchar_t EXTENSION[] = L".tsv";
    constexpr std::size_t HEADER_LINES = 3;
    constexpr std::size_t FILE_FIELDS = 4;

    wstring escaped(wstring_view text)  // [SyncState.md#line-format]
    {
        wstring retval;
        retval.reserve(text.size());
        for (wchar_t c : text)
            switch (c) {
            case L'\\': retval += L"\\\\"; break;
            case L'\t': retval += L"\\t"; break;
            case L'\n': retval += L"\\n"; break;
            case L'\r': retval += L"\\r"; break;
            default: retval += c;
            }
        return retval;
    }

    wstring unescaped(wstring_view text)
    {
        wstring retval;
        retval.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] != L'\\' || i + 1 == text.size()) {
                retval += text[i];
                continue;
            }
            switch (text[++i]) {
            case L't': retval += L'\t'; break;
            case L'n': retval += L'\n'; break;
            case L'r': retval += L'\r'; break;
            default: retval += text[i];
            }
        }
        return retval;
    }

    wstring text_of(const SyncState& state)
    {
        wstring text = wstring(MAGIC) + L' ' + std::to_wstring(SyncState::FORMAT_VERSION) + L'\n';
        text += wstring(SOURCE_KEY) + L'\t' + escaped(state.source()) + L'\n';
        text += wstring(TARGET_KEY) + L'\t' + escaped(state.target()) + L'\n';
        for (const auto& [path, file] : state.files())
            text += std::to_wstring(file.size) + L'\t' + std::to_wstring(util::to_unix_nanoseconds(file.source_time)) + L'\t'
                + std::to_wstring(util::to_unix_nanoseconds(file.target_time)) + L'\t' + escaped(path) + L'\n';
        return text;
    }

    std::vector<wstring_view> lines_of(const wstring& text)
    {
        std::vector<wstring_view> retval;
        for (std::size_t pos = 0; pos < text.size();) {
            const std::size_t end = std::min(text.find(L'\n', pos), text.size());
            retval.emplace_back(text.data() + pos, end - pos);
            pos = end + 1;
        }
        return retval;
    }

    std::vector<wstring_view> fields_of(wstring_view line, std::size_t count)  // the last field takes the rest of the line
    {
        std::vector<wstring_view> retval;
        while (retval.size() + 1 < count) {
            const std::size_t tab = line.find(L'\t');
            if (tab == wstring_view::npos)
                throw std::runtime_error("line has too few fields");
            retval.push_back(line.substr(0, tab));
            line.remove_prefix(tab + 1);
        }
        retval.push_back(line);
        return retval;
    }

    template<class Integer> Integer integer(wstring_view field)  // [SyncState.md#integer]
    {
        std::string digits;
        for (wchar_t c : field) {
            if (c != L'-' && (c < L'0' || c > L'9'))
                throw std::runtime_error("not a number: " + util::wstring_to_string(wstring(field)));
            digits += static_cast<char>(c);
        }
        Integer value{};
        const auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (ec != std::errc() || end != digits.data() + digits.size())
            throw std::runtime_error("not a number: " + digits);
        return value;
    }

    wstring header_value(wstring_view line, wstring_view key)
    {
        const std::vector<wstring_view> fields = fields_of(line, 2);
        if (fields[0] != key)
            throw std::runtime_error("header line is not " + util::wstring_to_string(wstring(key)));
        return unescaped(fields[1]);
    }

    void check_version(wstring_view line)  // [SyncState.md#FORMAT_VERSION]
    {
        if (!line.starts_with(MAGIC) || line.size() <= MAGIC.size() || line[MAGIC.size()] != L' ')
            throw std::runtime_error("not a sync state file");
        if (integer<int>(line.substr(MAGIC.size() + 1)) > SyncState::FORMAT_VERSION)
            throw std::runtime_error("written by a newer version of the program");
    }

    SyncState state_of(const wstring& text)
    {
        const std::vector<wstring_view> lines = lines_of(text);
        if (lines.size() < HEADER_LINES)
            throw std::runtime_error("header incomplete");
        check_version(lines[0]);
        FileStates files;
        for (std::size_t i = HEADER_LINES; i < lines.size(); ++i) {
            const std::vector<wstring_view> fields = fields_of(lines[i], FILE_FIELDS);
            files[unescaped(fields[3])] = {integer<uint64_t>(fields[0]), util::from_unix_nanoseconds(integer<int64_t>(fields[1])),
                util::from_unix_nanoseconds(integer<int64_t>(fields[2]))};
        }
        return SyncState(header_value(lines[1], SOURCE_KEY), header_value(lines[2], TARGET_KEY), std::move(files));
    }
}

SyncState::SyncState(wstring source, wstring target, FileStates files)
    : source_(std::move(source)), target_(std::move(target)), files_(std::move(files))
{
    for (const auto& [path, file] : files_)
        by_size_.emplace(file.size, path);
}

void SyncState::unindex(const wstring& path)
{
    const auto file = files_.find(path);
    if (file == files_.end())
        return;
    for (auto it = by_size_.lower_bound(file->second.size); it != by_size_.end() && it->first == file->second.size; ++it)
        if (it->second == path) {
            by_size_.erase(it);
            return;
        }
}

std::vector<wstring> SyncState::with_size(uint64_t size) const  // [SyncState.md#by_size_]
{
    std::vector<wstring> paths;
    for (auto it = by_size_.lower_bound(size); it != by_size_.end() && it->first == size; ++it)
        paths.push_back(it->second);
    return paths;
}

void SyncState::defer(PendingDeletion deletion)  // [SyncState.md#pass-notebook]
{
    if (!renamed_.contains(deletion.key))  // [SyncState.md#renamed]
        pending_deletions_.push_back(std::move(deletion));
}

void SyncState::renamed(const wstring& key)
{
    renamed_.insert(key);
}

std::optional<PendingDeletion> SyncState::claim(const wstring& key)
{
    const auto found = std::find_if(pending_deletions_.begin(), pending_deletions_.end(), [&](const PendingDeletion& d) { return d.key == key; });
    if (found == pending_deletions_.end())
        return std::nullopt;
    PendingDeletion deletion = std::move(*found);
    pending_deletions_.erase(found);
    return deletion;
}

std::vector<PendingDeletion> SyncState::take_deletions()
{
    return std::exchange(pending_deletions_, {});
}

void SyncState::defer(PendingDirRemoval removal)
{
    pending_dir_removals_.push_back(std::move(removal));
}

std::vector<PendingDirRemoval> SyncState::take_dir_removals()
{
    return std::exchange(pending_dir_removals_, {});
}

const FileState* SyncState::find(const wstring& path) const
{
    const auto found = files_.find(path);
    return found == files_.end() ? nullptr : &found->second;
}

bool SyncState::knows_below(const wstring& dir) const  // [SyncState.md#knows_below]
{
    const wstring prefix = dir + L'/';
    const auto first = files_.lower_bound(prefix);
    return first != files_.end() && first->first.starts_with(prefix);
}

void SyncState::record(const wstring& path, const FileState& state)  // [SyncState.md#record]
{
    unindex(path);
    const auto [entry, inserted] = files_.try_emplace(path, state);
    changed_ = changed_ || inserted || entry->second != state;
    entry->second = state;
    by_size_.emplace(state.size, path);
    seen_.insert(path);
}

void SyncState::seen(const wstring& path)
{
    seen_.insert(path);
}

void SyncState::forget(const wstring& path)
{
    unindex(path);
    changed_ = files_.erase(path) > 0 || changed_;
    seen_.erase(path);
}

void SyncState::forget_unseen()  // [SyncState.md#forget_unseen]
{
    for (const auto& [path, file] : files_)
        if (!seen_.contains(path))
            unindex(path);
    changed_ = std::erase_if(files_, [this](const auto& file) { return !seen_.contains(file.first); }) > 0 || changed_;
    seen_.clear();
}

fs::path sync_state_dir()  // [SyncState.md#sync_state_dir]
{
    return fs::path(Config::get_root_pathname()).parent_path() / L"state";
}

fs::path sync_state_file_path(const fs::path& directory, const wstring& item_name)  // [SyncState.md#sync_state_file_path]
{
    const wstring name = util::as_file_name(item_name);
    return directory / ((name.empty() ? wstring(DEFAULT_FILE_NAME) : name) + EXTENSION);
}

std::optional<SyncState> read_sync_state(const fs::path& file, const wstring& source, const wstring& target)  // [SyncState.md#read_sync_state]
{
    std::error_code ec;
    if (!fs::exists(file, ec))
        return std::nullopt;
    try {
        SyncState state = state_of(util::read_utf8_file(file));  // [SyncState.md#utf8-file]
        if (state.source() != source || state.target() != target) {  // [SyncState.md#sides-must-match]
            log_info(L"sync state ignored, it belongs to other paths: " + file.wstring());
            return std::nullopt;
        }
        return state;
    }
    catch (const std::exception& ex) {
        log_warning(translation::translate("sync_state_not_readable"), file.wstring(), util::string_to_wstring(ex.what()));
        return std::nullopt;
    }
}

bool write_sync_state(const fs::path& file, const SyncState& state)  // [SyncState.md#write_sync_state]
{
    const fs::path tmp_file = file.wstring() + L".tmp";
    try {
        fs::create_directories(file.parent_path());
        util::write_utf8_file(tmp_file, text_of(state));  // [SyncState.md#utf8-file]
    }
    catch (const std::exception& ex) {
        log_error(translation::translate("sync_state_not_writable"), file.wstring(), util::string_to_wstring(ex.what()));
        util::remove_file(tmp_file.wstring());
        return false;
    }
    return util::rename_file(tmp_file.wstring(), file.wstring());
}

bool remove_sync_state(const fs::path& file)
{
    std::error_code ec;
    fs::remove(file, ec);
    return !ec;
}

} // namespace AFS_Core
