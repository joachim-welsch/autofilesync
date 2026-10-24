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

namespace guitest {

void bring_to_foreground();  // [ForegroundApp.md#bring_to_foreground]
void disable_window_animations();  // [ForegroundApp.md#disable_window_animations]
void settle_window_server();  // [ForegroundApp.md#settle_window_server]
long window_number(void* ns_window);  // [ForegroundApp.md#window-number]
bool window_exists(long number);
bool window_at_point_is(int x, int y, long number);  // [ForegroundApp.md#window_at_point]
std::string window_at_point(int x, int y);  // [ForegroundApp.md#window_at_point]

}
