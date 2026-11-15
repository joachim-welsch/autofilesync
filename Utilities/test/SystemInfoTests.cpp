/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
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
#include <SystemInfo.h>

using namespace AFS_Core;

TEST(SystemInfoTests, HostNameIsNeverEmpty)  // [SystemInfo.md#host_name]
{
    EXPECT_FALSE(system_info::host_name().empty());
}

TEST(SystemInfoTests, CurrentPidIsStableAndNotZero)
{
    EXPECT_NE(system_info::current_pid(), 0ul);
    EXPECT_EQ(system_info::current_pid(), system_info::current_pid());
}
