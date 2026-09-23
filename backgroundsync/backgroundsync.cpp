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
#include <string>
#include <chrono>
#include <thread>
#include "Logging.h"
#include "Translation.h"
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>
#include <syslog.h>
#include "MyDaemon.h"
#include <csignal>

std::unique_ptr<MyDaemon> myDaemon;
volatile std::sig_atomic_t stopRequested = 0;

void signalHandler(int /*signal*/) {
    stopRequested = 1;
}

int main(int argc, char *argv[])
{
    AFS_Core::translation::init_locale("de_DE.UTF-8");  // a unit file usually names no language at all
    AFS_Core::translation::init(AFS_Core::translation::locale_dir());  // before the first message; installed to /usr/sbin there is no locale dir beside it, so gettext searches the system path
    AFS_Core::Logging::instance().origin(AFS_Core::LogOrigin::Daemon);  // the GUI writes to the same file
    myDaemon = std::make_unique<MyDaemon>();
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    myDaemon->start();
    syslog(LOG_INFO, "%s", AFS_Core::translation::translate_utf8("daemon_running").c_str());
    AFS_Core::log_info(AFS_Core::translation::translate("daemon_started"), L"");
    while (!stopRequested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    myDaemon->stop();
    syslog(LOG_INFO, "%s", AFS_Core::translation::translate_utf8("daemon_stopped").c_str());
    AFS_Core::log_info(AFS_Core::translation::translate("daemon_stopped"), L"");  // after stop(), so it is the daemon's last entry
    return 0;
}

