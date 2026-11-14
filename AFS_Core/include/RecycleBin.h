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
#include <filesystem>
#include <mutex>
#include <vector>

namespace AFS_Core {

class RecycleBin {  // [RecycleBin.md#RecycleBin]
public:
	using ItemList = std::vector<std::filesystem::path>;
private:
	mutable std::mutex mutex_;
	std::filesystem::path path_;
	RecycleBin();
	~RecycleBin();
	RecycleBin(const RecycleBin&) = delete;
	RecycleBin& operator=(const RecycleBin&) = delete;
	bool check_relative(const std::filesystem::path& item) const;  // the private helpers below assume a held mutex_
	bool move(const std::filesystem::path& from, const std::filesystem::path& to) const;
public:
	static RecycleBin& instance();
	void bin_path(const std::filesystem::path& path);
	std::filesystem::path bin_path() const;
	bool add_item(const std::filesystem::path& item);
	bool add_item(const std::filesystem::path& source, const std::filesystem::path& item);  // [RecycleBin.md#add_item-outside-cwd]
	bool has_item(const std::filesystem::path& item) const;
	bool restore_item(const std::filesystem::path& item);
	bool take_item(const std::filesystem::path& item, const std::filesystem::path& destination);  // [RecycleBin.md#take_item]
	ItemList list_items() const;  // by value: a reference would outlive the lock that protects the bin
};

} // namespace AFS_Core
