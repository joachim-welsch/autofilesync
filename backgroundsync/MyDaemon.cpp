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
#include "Config.h"
#include "Logging.h"
#include "Synchronize.h"
#include "Translation.h"

using namespace AFS_Core;

MyDaemon::~MyDaemon()
{
    stop();
}

void MyDaemon::start()
{
    if (running_.load())
        return;
    running_.store(true);
    Config::instance().pathname(Config::get_root_pathname());  // the GUI's file, so both sync the same items
    load_config();
    worker_thread_ = std::thread(&MyDaemon::run, this);
}

void MyDaemon::stop()
{
    if (!running_.load())
        return;
    running_.store(false);
    if (worker_thread_.joinable())
        worker_thread_.join();
}

void MyDaemon::load_config()
{
    Config::instance().read(Config::instance().pathname());
    translation::set_language(Config::instance().settings().language());  // the language chosen in the GUI, also after a reload
    Logging::instance().open_file();  // read() leaves the file closed when the configured name equals the default name
    if (const auto mismatch = translation::catalogue_mismatch())  // [MyDaemon.md#catalogue-check]
        log_warning(*mismatch);
}

void MyDaemon::reload_config()
{
    Synchronize::destroy();  // joins the running pass, so no item of the old configuration outlives the reload
    load_config();
    log_info(translation::translate("config_reloaded"), Config::instance().pathname());
    Synchronize::create(SyncType::Continuous, nullptr, SyncRole::Background);
}

void MyDaemon::run()
{
    Synchronize::create(SyncType::Continuous, nullptr, SyncRole::Background);
    while (running_.load()) {
        cut_log_daily_();
        if (Config::instance().file_changed_on_disk())
            reload_config();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    Synchronize::destroy();
}
