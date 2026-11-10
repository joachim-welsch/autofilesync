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
#include <cstdint>
#include <string>

namespace AFS_Core {

struct SmtpAccount {  // [SmtpMailer.md#SmtpAccount]
	std::wstring server;
	uint16_t     port = 587;	// submission port; 465 means implicit TLS
	std::wstring user;			// empty: server accepts unauthenticated relaying
	std::wstring password;
	std::wstring sealed_password;	// [SmtpMailer.md#sealed_password]
	std::wstring from;
	bool complete() const { return !server.empty() && !from.empty(); }
	bool operator==(const SmtpAccount&) const = default;
};

struct MailResult {  // [SmtpMailer.md#send_mail]
	std::string error;	// empty when the mail was accepted
	bool unencrypted = false;	// accepted over a plain connection [SmtpMailer.md#plaintext-without-password]
};

MailResult send_mail(const SmtpAccount& account, const std::wstring& recipient,
	const std::wstring& subject, const std::string& body);  // [SmtpMailer.md#send_mail]

} // namespace AFS_Core
