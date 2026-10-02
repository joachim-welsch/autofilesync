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
#define _CRT_SECURE_NO_WARNINGS
#include <string>
#include "gtest/gtest.h"
#ifndef test_suite_name
/* Compat: older GoogleTest versions provide test_case_name(), newer ones test_suite_name().
   Map the newer name to the older one when only the old API exists. */
#define test_suite_name test_case_name
#endif

#ifndef GTEST_SKIP
/* Minimal fallback for environments where GTEST_SKIP() is not available.
   This allows tests to compile; it does not mark tests as skipped in the
   test runner. */
namespace testing {
	struct GTestSkipFallback {
		template<typename T>
		GTestSkipFallback& operator<<(T const&) { return *this; }
	};
}
#define GTEST_SKIP() ::testing::GTestSkipFallback()
#endif
#include <iostream>
