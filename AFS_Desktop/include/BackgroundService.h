/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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

namespace BackgroundService {  // [BackgroundService.md#BackgroundService]

enum class State { NotInstalled, Stopped, Running, Unknown };  // [BackgroundService.md#State]

State state();  // [BackgroundService.md#state]

std::wstring start();  // [BackgroundService.md#command-result]
std::wstring stop();
bool logon_failed();  // [BackgroundService.md#logon_failed]
std::wstring account();
std::wstring set_password(const std::wstring& password);  // [BackgroundService.md#set_password]

}
