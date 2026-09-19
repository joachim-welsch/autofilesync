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
#pragma once

#include <string>
#include <cstdint>

namespace AFS_Core {

class SyncMessage {
	std::wstring	msg_;
	std::wstring	file_;
	uint64_t		filenumber_ = 0;
	uint64_t		filesize_ = 0;
public:
	SyncMessage() = default;
	SyncMessage(const std::wstring& msg, const std::wstring& file, uint64_t filenumber, uint64_t filesize);
	std::wstring msg() const { return msg_; }
	std::wstring file() const { return file_; }  // empty unless a file is being copied
	uint64_t    filenumber() const { return filenumber_; }
	uint64_t	filesize() const { return filesize_; }
};

} // namespace AFS_Core
