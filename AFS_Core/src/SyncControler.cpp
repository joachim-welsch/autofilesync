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
#include "pch.h"
#include "SyncControler.h"
#include "Synchronize.h"
#include "Logging.h"

namespace AFS_Core {

std::unique_ptr<SyncControler> SyncControler::instance_ = nullptr;

SyncControler& SyncControler::instance()
{
	if (instance_ == nullptr)
		instance_ = SyncCtrlPtr(new SyncControler());
	return *instance_;
}

void SyncControler::register_progress_dialog(ProgressDialog* pdlg)
{
	log_debug(L"SyncControler::register_progress_dialog()");
	progress_dialog_ = pdlg;
}

std::optional<ProgressDialog*> SyncControler::progress_dialog()
{
	log_debug(L"SyncControler::progress_dialog()");
	ProgressDialog* pdlg = progress_dialog_.load();
	if (pdlg == nullptr)
		return std::nullopt;
	return pdlg;
}

bool SyncControler::start_sync(SyncType sync_type)
{
	log_debug(L"SyncControler::start_sync()");
	if (synchronize_ != nullptr)
		return false;
	state_ = SyncCtrlState::SyncActive;
    synchronize_ = Synchronize::create(sync_type, this);
	return true;
}

bool SyncControler::sync_finished() const
{
	return state_ == SyncCtrlState::SyncFinished;
}

void SyncControler::set_sync_finished() // called by Workerthread
{
	log_debug(L"SyncControler::set_sync_finished()");  // [SyncControler.md#set_sync_finished]
	state_ = SyncCtrlState::SyncFinished;
}

void SyncControler::destroy_synchronize()
{
	Synchronize::destroy();
	synchronize_ = nullptr;
}

void SyncControler::request_stop()
{
	log_debug(L"SyncControler::request_stop()");  // [SyncControler.md#request_stop]
	if (state_ != SyncCtrlState::SyncActive)
		return;
	Synchronize::stop();
}

void SyncControler::stop_sync()
{
	log_debug(L"SyncControler::stop_sync()");
	if (state_ != SyncCtrlState::SyncActive)
		return;
	Synchronize::stop();
	while (!sync_finished())
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

SyncMessage SyncControler::get_message() const
{
	if (synchronize_ == nullptr)
		return SyncMessage();
	return synchronize_->get_message();
}

} // namespace AFS_Core
