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
#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include "LogCut.h"

class MyDaemon {
public:
    ~MyDaemon();

    void start();
    void stop();

private:
    void run();
    void load_config();
    void reload_config();  // [MyDaemon.md#reload_config]
    void keep_device_link();  // [MyDaemon.md#device-link]

    std::atomic<bool> running_ = false;
    std::thread worker_thread_;
    AFS_Core::DailyLogCut cut_log_daily_;
    std::chrono::steady_clock::time_point next_link_try_{};
};
