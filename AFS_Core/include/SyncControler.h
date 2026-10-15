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
#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include "Global.h"
#include "SyncMessage.h"
#include "SyncPlan.h"

namespace AFS_Core {

class Synchronize;

enum class SyncCtrlState { Waiting, SyncActive, SyncFinished };

class SyncControler {
	using SyncCtrlPtr = std::unique_ptr<SyncControler>;

	static SyncCtrlPtr  instance_;
	Synchronize*		synchronize_ = nullptr;
	std::atomic<SyncCtrlState>	state_ = SyncCtrlState::Waiting;
	std::size_t			count_files_ = 0;
	SyncPlan			plan_;  // [SyncControler.md#plan_]

protected:
	void set_sync_finished();
	SyncControler() = default;
public:
	~SyncControler() = default;
	static SyncControler& instance();
	bool start_sync(SyncType sync_type);
	void request_stop();
	void stop_sync();
	bool sync_finished() const;
	void reset_state();
	void destroy_synchronize();
	SyncMessage get_message() const;
	uint64_t copied_bytes() const;
	void count_files(std::size_t);
	std::size_t count_files() const;
	void plan(SyncPlan plan) { plan_ = std::move(plan); }
	SyncPlan take_plan() { return std::exchange(plan_, SyncPlan()); }
	friend class Synchronize;
};

inline
void SyncControler::count_files(std::size_t count_files)
{
	count_files_ = count_files;
}

inline
std::size_t SyncControler::count_files() const
{
	return count_files_;
}

inline
void SyncControler::reset_state()
{
	state_ = SyncCtrlState::Waiting;
}

} // namespace AFS_Core
