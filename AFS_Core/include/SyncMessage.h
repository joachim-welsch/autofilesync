/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
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
