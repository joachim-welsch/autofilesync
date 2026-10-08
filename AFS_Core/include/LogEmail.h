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
#include <string>
#include <mutex>
#include "SmtpMailer.h"

namespace AFS_Core {

class LogEmail {
	mutable std::mutex 		mutex_;
	bool					notify_active_ = false;  // [Logging.md#notify_email_active_]
	std::wstring			notify_address_;
	SmtpAccount				smtp_;  // the account the notification is sent through
public:
	void notify_active(bool active);
	bool notify_active() const;
	std::wstring notify_address() const;
	void notify_address(const std::wstring& address);
	void smtp_account(const SmtpAccount& account);
	SmtpAccount smtp_account() const;
};

} // namespace AFS_Core