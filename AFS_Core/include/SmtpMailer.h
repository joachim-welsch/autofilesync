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
#include <cstdint>
#include <string>

namespace AFS_Core {

struct SmtpAccount {  // [SmtpMailer.md#SmtpAccount]
	std::wstring server;
	uint16_t     port = 587;	// submission port; 465 means implicit TLS
	std::wstring user;			// empty: server accepts unauthenticated relaying
	std::wstring password;
	std::wstring from;
	bool complete() const { return !server.empty() && !from.empty(); }
	bool operator==(const SmtpAccount&) const = default;
};

std::string send_mail(const SmtpAccount& account, const std::wstring& recipient,
	const std::wstring& subject, const std::string& body);  // [SmtpMailer.md#send_mail]

} // namespace AFS_Core
