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
