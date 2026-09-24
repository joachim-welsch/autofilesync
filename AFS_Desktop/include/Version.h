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

#define afsVERSION_MAJOR 0  // [Version.md#macros]
#define afsVERSION_MINOR 1
#define afsVERSION_PATCH 0
#define afsVERSION_TEXT "0.1.0"

#ifndef RC_INVOKED
constexpr char afsNAME_STRING[] = "AutoFileSync";
constexpr char afsVERSION_STRING[] = afsVERSION_TEXT;
#endif
