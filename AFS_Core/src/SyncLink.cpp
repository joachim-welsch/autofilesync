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
#include "SyncLink.h"
#include "CopyLog.h"
#include "FileDelete.h"
#include "Logging.h"
#include "SyncPlan.h"
#include "Translation.h"
#include <SymlinkUtil.h>

namespace AFS_Core {

using std::wstring;
namespace fs = std::filesystem;
using fsu::FileInfoResult;

namespace {
constexpr wchar_t PREPARED_SUFFIX[] = L".afs-link";  // [SyncLink.md#create]
constexpr wchar_t PARKED_SUFFIX[] = L".bak";  // [SyncLink.md#park]

bool taken(const wstring& path)
{
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec));
}

wstring free_name(const wstring& base, int first_number)  // [SyncLink.md#park]
{
    for (int number = first_number;; ++number) {
        const wstring candidate = number == 0 ? base : base + std::to_wstring(number);
        if (!taken(candidate))
            return candidate;
    }
}

void report(const wstring& message)
{
    log_error(message);
    report_sync_error(message);
}
}

void LinkTally::Count::add(const wstring& link)
{
    if (links++ == 0)
        first = link;
}

SyncLink::SyncLink(const SyncItem& si, std::size_t& copy_count, bool only_count, LinkTally& tally, SyncState* state,
    const wstring& state_root, const wstring& target_root)
    : SyncBase(si, copy_count, only_count, state, state_root, target_root), tally_(tally)
{
}

bool SyncLink::same_link(const wstring& destination, const fs::path& text) const
{
    const symlink_util::LinkInfo there = symlink_util::link_info(destination);
    return there.is_link && symlink_util::portable_link_text(destination, there.text, target_root_).lexically_normal() == text.lexically_normal();
}

bool SyncLink::blocked_by_directory(const wstring& destination) const  // [SyncLink.md#blocked-by-directory]
{
    std::error_code ec;
    if (!fs::is_directory(fs::symlink_status(destination, ec)))
        return false;
    report(translation::translate("link_blocked_by_directory") + destination);
    return true;
}

void SyncLink::plan(const wstring& source, const wstring& destination)
{
    ++copy_count_;
    plan_step({sync_item_.name(), sync_item_.del_aftercopy() ? PlannedAction::MoveToTarget : PlannedAction::CopyToTarget, source, destination, 0});
}

FileInfoResult SyncLink::create_verified(const fs::path& text, const wstring& at, bool to_directory) const  // [SyncLink.md#create]
{
    fsu::remove_file(at);
    const FileInfoResult refusal = symlink_util::create_link_refusal(text, at, to_directory);
    if (refusal != FileInfoResult::OK || symlink_util::link_info(at).text == text)
        return refusal;
    fsu::remove_file(at);
    return FileInfoResult::ErrorWriteFileInfo;
}

std::optional<wstring> SyncLink::park(const wstring& destination) const  // [SyncLink.md#park]
{
    if (!taken(destination))
        return wstring();
    const wstring parked = sync_item_.father_son_principle() ? free_name(destination + FATHER_SON_SUFFIX, 1) : free_name(destination + PARKED_SUFFIX, 0);
    const FileInfoResult refusal = fsu::rename_refusal(destination, parked);
    if (refusal == FileInfoResult::OK)
        return parked;
    report(fsu::make_error_message(refusal) + L": " + destination);
    return std::nullopt;
}

bool SyncLink::put_in_place(const wstring& prepared, const wstring& destination) const
{
    const std::optional<wstring> parked = park(destination);
    if (!parked)
        return false;
    const FileInfoResult refusal = fsu::rename_refusal(prepared, destination);
    if (refusal != FileInfoResult::OK) {
        report(translation::translate("link_create_failed") + destination + L" (" + fsu::make_error_message(refusal) + L")");
        if (!parked->empty())
            fsu::rename_file(*parked, destination);
        return false;
    }
    if (!parked->empty() && !sync_item_.father_son_principle())
        fsu::remove_file(*parked);
    return true;
}

void SyncLink::refused(const wstring& destination, FileInfoResult refusal)
{
    if (refusal == FileInfoResult::LinksNotSupported)
        tally_.unsupported.add(destination);
    else
        report(translation::translate("link_create_failed") + destination + L" (" + fsu::make_error_message(refusal) + L")");
}

bool SyncLink::create(const fs::path& text, const wstring& destination, bool to_directory)  // [SyncLink.md#create]
{
    const wstring prepared = destination + PREPARED_SUFFIX;
    const FileInfoResult refusal = create_verified(text, prepared, to_directory);
    if (refusal != FileInfoResult::OK) {
        refused(destination, refusal);
        return false;
    }
    if (put_in_place(prepared, destination))
        return true;
    fsu::remove_file(prepared);
    return false;
}

void SyncLink::created(const wstring& source, const wstring& destination, const fs::path& text)
{
    ++copy_count_;
    log_info(translation::translate("link_created"), source, L"->", destination);
    if (symlink_util::leaves_root(source, text, state_root_))
        tally_.outside.add(source);
    else
        tally_.created.push_back(destination);
    if (sync_item_.del_aftercopy())
        delete_file(source);
}

void SyncLink::operator()()
{
    const wstring source = without_volume_spec(sync_item_.source());
    const wstring destination = without_volume_spec(sync_item_.target());
    if (sync_item_.ssh_source() || sync_item_.ssh_target()) {  // [SyncLink.md#local-only]
        tally_.unsupported.add(source);
        return;
    }
    const symlink_util::LinkInfo link = symlink_util::link_info(source);
    const fs::path text = symlink_util::portable_link_text(source, link.text, state_root_);
    if (!link.is_link || same_link(destination, text) || blocked_by_directory(destination))
        return;
    if (only_count_)
        plan(source, destination);
    else if (create(text, destination, link.to_directory))
        created(source, destination, text);
}

} // namespace AFS_Core
