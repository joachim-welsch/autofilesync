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
#include "Logging.h"
#include <fstream>
#include <chrono>

namespace AFS_Core {

class LogCut {
	using TimePoint = std::chrono::system_clock::time_point;

	Logging&		log_;
	TimePoint		now_;
	std::ifstream	is_;
	std::ofstream	os_;

	bool open_files();
	void close_files();
	void cut();
	bool get_time_from_line(const std::string& line, TimePoint& tp);
public:
	LogCut();
	void operator()();
};

class DailyLogCut {  // [LogCut.md#DailyLogCut]
	using TimePoint = std::chrono::system_clock::time_point;

	TimePoint	next_cut_ = (TimePoint::min)();
public:
	void operator()(TimePoint now = std::chrono::system_clock::now());
};

} // namespace AFS_Core
