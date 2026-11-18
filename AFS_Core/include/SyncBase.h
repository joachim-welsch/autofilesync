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
#include <functional>
#include <map>
#include <vector>
#include <set>
#include "StrUtil.h"

namespace AFS_Core {

enum class CopyResult { Undefined, OK, Error, Stopped };

struct LinkTally {  // [SyncLink.md#LinkTally]
	struct Count {
		std::size_t links = 0;
		std::wstring first;
		void add(const std::wstring& link);
	};
	Count unsupported;
	Count system;
	Count outside;
	Count loops;
	Count into_target;
	Count too_deep;
	Count dangling;
	Count junctions;
	std::vector<std::wstring> created;
};

class SyncBase {
protected:
	const SyncItem sync_item_;  // [SyncBase.md#sync_item_]
	std::size_t&	copy_count_;
	const bool		only_count_;
	SSHPATH			ssh_ {SSHPATH::none};
	SyncState*		state_;  // [SyncBase.md#state_]
	const std::wstring state_root_;
	const std::wstring target_root_;
	LinkTally link_tally_;  // [SyncLink.md#LinkTally]
	std::map<std::wstring, std::set<std::wstring>> remote_target_links_;  // [SyncBase.md#target_usable]

	std::wstring state_key(const std::wstring& source) const;  // [SyncBase.md#state_key]
	std::wstring side_path(const std::wstring& key, bool in_target) const;  // [SyncBase.md#side_path]
	void finish_pass();  // [SyncBase.md#finish_pass]
	void carry_out_deferred();  // [SyncBase.md#finish_pass]
	void report_links();  // [SyncBase.md#report_links]
	void report_link_count(const LinkTally::Count& count, const char* message_id) const;  // [SyncBase.md#report_links]
	bool links_as_links() const;  // [SyncBase.md#links_as_links]
	bool follows_links() const;  // [SyncBase.md#follows_links]
	void report_mode_conflict() const;  // [SyncBase.md#follows_links]
	bool target_usable(const std::wstring& target);  // [SyncBase.md#target_usable]
	bool target_is_link(const std::wstring& target);  // [SyncBase.md#target_usable]
	bool link_removed(const std::wstring& target);
	bool link_filtered(const std::wstring& name, const std::wstring& path, bool ssh, bool directory, const std::wstring& wildcard) const;  // [SyncBase.md#link_filtered]
	bool system_link_skipped(const std::wstring& path);  // [SyncBase.md#system_link_skipped]
	void sync_link(const std::wstring& source, const std::wstring& dest);  // [SyncBase.md#sync_link]
	void guarded(const std::wstring& source, const std::wstring& dest, const std::function<void()>& work);  // [SyncBase.md#sync_file]
	bool deletions_refused(const std::vector<PendingDeletion>& deletions);  // [SyncBase.md#mass-deletion]
	bool create_target_dir_if_not_exist(const std::wstring& targetdir = L"");  // [SyncBase.md#create_target_dir_if_not_exist]
	bool create_source_dir(const std::wstring& dir);  // [SyncBase.md#create_source_dir]
	bool local_dir_created(const std::wstring& dir, const char* message_id);  // [SyncBase.md#local_dir_created]
	void report_failure(const std::wstring& where, const std::filesystem::filesystem_error& failure);  // [SyncBase.md#report_failure]
	void remove_dir_if_empty(const std::wstring& dir, bool ssh);  // [SyncBase.md#remove_dir_if_empty]
	bool deleted(const std::wstring& file, bool ssh, const char* message_id);  // [SyncBase.md#deleted]
	bool completes_moves() const;  // [SyncBase.md#completes_moves]
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
