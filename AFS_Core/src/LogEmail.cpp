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
#include "LogEmail.h"

namespace AFS_Core {

void LogEmail::notify_active(bool active)
{
	std::lock_guard<std::mutex> lock(mutex_);
	notify_active_ = active;
}

bool LogEmail::notify_active() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return notify_active_;
}

std::wstring LogEmail::notify_address() const
{
	std::lock_guard<std::mutex> lock(mutex_); return notify_address_;
}

void  LogEmail::notify_address(const std::wstring& address)
{
	std::lock_guard<std::mutex> lock(mutex_);
	notify_address_ = address;
}

SmtpAccount LogEmail::smtp_account() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return smtp_;
}

void LogEmail::smtp_account(const SmtpAccount& account)
{
	std::lock_guard<std::mutex> lock(mutex_);
	smtp_ = account;
}

} // namespace AFS_Core