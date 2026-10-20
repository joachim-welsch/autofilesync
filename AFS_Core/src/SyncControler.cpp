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

bool SyncControler::start_sync(SyncType sync_type, bool mass_deletion_approved)
{
	log_debug(L"SyncControler::start_sync()");
	if (synchronize_ != nullptr)
		return false;
	state_ = SyncCtrlState::SyncActive;
    synchronize_ = Synchronize::create(sync_type, this, SyncRole::Foreground, mass_deletion_approved);
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

uint64_t SyncControler::copied_bytes() const
{
	return synchronize_ == nullptr ? 0 : synchronize_->copied_bytes();
}

} // namespace AFS_Core
