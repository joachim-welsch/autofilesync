/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <optional>
#include <atomic>
#include <memory>
#include "Global.h"
#include "SyncMessage.h"

class ProgressDialog;

namespace AFS_Core {

class Synchronize;

enum class SyncCtrlState { Waiting, SyncActive, SyncFinished };

class SyncControler {
	using SyncCtrlPtr = std::unique_ptr<SyncControler>;

	static SyncCtrlPtr  instance_;
	std::atomic<ProgressDialog*> progress_dialog_ = nullptr;
	Synchronize*		synchronize_ = nullptr;
	bool				display_errormessages_ = false;
	std::atomic<SyncCtrlState>	state_ = SyncCtrlState::Waiting;
	std::size_t			count_files_ = 0;

protected:
	void set_sync_finished();
	SyncControler() = default;
public:
	~SyncControler() = default;
	static SyncControler& instance();
	void register_progress_dialog(ProgressDialog*);
	std::optional<ProgressDialog*> progress_dialog();
	bool start_sync(SyncType sync_type);
	void request_stop();
	void stop_sync();
	bool sync_finished() const;
	void reset_state();
	void destroy_synchronize();
	void display_errormessages(bool);
	bool display_errormessages() const;
	SyncMessage get_message() const;
	void count_files(std::size_t);
	std::size_t count_files() const;
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
