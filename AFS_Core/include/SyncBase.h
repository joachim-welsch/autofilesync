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
#include "SyncItem.h"
#include "SyncState.h"
#include <stdexcept>
#include <filesystem>
#include <set>
#include "Util.h"

namespace AFS_Core {

enum class CopyResult { Undefined, OK, Error, Stopped };

class SyncBase {
protected:
	const SyncItem sync_item_;  // [SyncBase.md#sync_item_]
	std::size_t&	copy_count_;
	const bool		only_count_;
	SSHPATH			ssh_ {SSHPATH::none};
	SyncState*		state_;  // [SyncBase.md#state_]
	const std::wstring state_root_;
	const std::wstring target_root_;

	std::wstring state_key(const std::wstring& source) const;  // [SyncBase.md#state_key]
	std::wstring side_path(const std::wstring& key, bool in_target) const;  // [SyncBase.md#side_path]
	void finish_pass();  // [SyncBase.md#finish_pass]
	bool deletions_refused(const std::vector<PendingDeletion>& deletions);  // [SyncBase.md#mass-deletion]
	void create_target_dir_if_not_exist(const std::wstring& targetdir = L"");
	void create_source_dir(const std::wstring& dir);  // [SyncBase.md#create_source_dir]
	void remove_dir_if_empty(const std::wstring& dir, bool ssh);  // [SyncBase.md#remove_dir_if_empty]
	void sync_dir_entry(const std::filesystem::directory_entry& dir_entry, const std::wstring& wildcard, const std::wstring& destination);
	void sync_file(const std::wstring& source, const std::wstring& dest, const std::wstring& wildcard);  // [SyncBase.md#sync_file]
    bool drive_not_exist(const std::wstring& pathname, bool ssh);
	void sync_target_only_entries(const std::wstring& source_dir, const std::wstring& target_dir,
		const std::wstring& wildcard, const std::set<std::wstring>& source_names);  // [SyncBase.md#sync_target_only_entries]
	bool stop_requested() const;
	SyncBase() = delete;
public:
	SyncBase(const SyncItem& sync_item, std::size_t& copy_count, bool only_count, SyncState* state = nullptr,
		const std::wstring& state_root = L"", const std::wstring& target_root = L"");
	virtual void operator()() = 0;
	virtual ~SyncBase() = default;
};

class SyncException : public std::runtime_error {  // [SyncBase.md#SyncException]
	std::wstring message_;
public:
	explicit SyncException(const std::wstring& msg) : std::runtime_error(util::wstring_to_string(msg)), message_(msg) {}
	std::wstring get_message() const { return message_; }
};

constexpr wchar_t FATHER_SON_SUFFIX[] = L".bak.";  // [SyncBase.md#father_son_backup]
constexpr std::size_t MASS_DELETION_PERCENT = 50;  // [SyncBase.md#mass-deletion]
constexpr std::size_t MASS_DELETION_MIN_FILES = 10;
bool mass_deletion(std::size_t deletions, std::size_t known_files);  // [SyncBase.md#mass-deletion]
bool father_son_backup(const std::wstring& name);  // [SyncBase.md#father_son_backup]
std::wstring root_name(const std::wstring& path);
std::wstring without_volume_spec(const std::wstring& path);

} // namespace AFS_Core
