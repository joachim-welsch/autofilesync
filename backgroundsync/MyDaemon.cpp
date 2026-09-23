/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of backgroundsync, the background service of AutoFileSync.
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
#include "MyDaemon.h"
#include <ctime>
#include <Synchronize.h>
#include <Config.h>
#include "Logging.h"

using namespace AFS_Core;

MyDaemon::MyDaemon()
    : running_(false) {
}

MyDaemon::~MyDaemon() {
    stop();
}

void MyDaemon::start() {
    if (running_.load()) {
        return;
    }
    running_.store(true);
    const std::wstring path_name = Config::get_root_pathname();  // the GUI's file, so both sync the same items
    Config::instance().pathname(path_name);
    Config::instance().read(path_name);
    Logging::instance().open_file();  // read() leaves the file closed when the configured name equals the default name
    log_info(L"Daemon is running...");
    workerThread_ = std::thread(&MyDaemon::run, this);
}

void MyDaemon::stop() {
    if (!running_.load()) {
        return;
    }
    running_.store(false);
    if (workerThread_.joinable())
        workerThread_.join();
}

void MyDaemon::run() {
    Synchronize::create(SyncType::Continuous, nullptr, SyncRole::Background);
    while (running_.load()) {
        cut_log_daily_();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    Synchronize::destroy();
}
