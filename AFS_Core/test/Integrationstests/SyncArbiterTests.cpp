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
#include "SyncArbiter.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace AFS_Core;

using namespace std::chrono_literals;

namespace {
std::chrono::milliseconds time_acquire(SyncArbiter& arbiter, const std::atomic_bool& abort, bool& acquired)  // [SyncArbiterTests.md#time_acquire]
{
    const auto start = std::chrono::steady_clock::now();
    acquired = arbiter.acquire(abort);
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
}
}

TEST(SyncArbiterTests, BackgroundWaitsWhileForegroundHolds)  // [SyncArbiterTests.md#BackgroundWaitsWhileForegroundHolds]
{
    std::atomic_bool abort{false};
    SyncArbiter foreground(SyncRole::Foreground);
    bool acquired = false;
    ASSERT_LT(time_acquire(foreground, abort, acquired).count(), 500);
    ASSERT_TRUE(acquired);

    std::atomic_bool background_entered{false};
    std::thread daemon([&]() {
        SyncArbiter background(SyncRole::Background);
        std::atomic_bool no_abort{false};
        background.acquire(no_abort);
        background_entered = true;
        background.release();
    });
    std::this_thread::sleep_for(600ms);
    EXPECT_FALSE(background_entered.load()) << "Der Hintergrunddienst haette pausieren muessen.";

    foreground.release();
    daemon.join();
    EXPECT_TRUE(background_entered.load()) << "Nach dem Ende der GUI-Synchronisation muss der Dienst weiterlaufen.";
}

TEST(SyncArbiterTests, ForegroundWaitsForRunningBackgroundPass)  // [SyncArbiterTests.md#ForegroundWaitsForRunningBackgroundPass]
{
    std::atomic_bool abort{false};
    SyncArbiter background(SyncRole::Background);
    bool acquired = false;
    ASSERT_TRUE(background.acquire(abort));

    std::thread releaser([&]() {
        std::this_thread::sleep_for(700ms);
        background.release();
    });
    SyncArbiter foreground(SyncRole::Foreground);
    const auto waited = time_acquire(foreground, abort, acquired);
    releaser.join();

    EXPECT_TRUE(acquired);
    EXPECT_GE(waited.count(), 500) << "Die GUI haette auf den laufenden Durchlauf warten muessen.";
    foreground.release();
}

TEST(SyncArbiterTests, AbortEndsTheWaitInsteadOfBlocking)  // [SyncArbiterTests.md#AbortEndsTheWaitInsteadOfBlocking]
{
    std::atomic_bool abort{false};
    SyncArbiter foreground(SyncRole::Foreground);
    ASSERT_TRUE(foreground.acquire(abort));

    std::atomic_bool daemon_abort{false};
    bool acquired = true;
    std::chrono::milliseconds waited{0};
    std::thread daemon([&]() {
        SyncArbiter background(SyncRole::Background);
        waited = time_acquire(background, daemon_abort, acquired);
    });
    std::this_thread::sleep_for(400ms);
    daemon_abort = true;
    daemon.join();

    EXPECT_FALSE(acquired) << "acquire() muss bei Abbruch false liefern.";
    EXPECT_LT(waited.count(), 2000);
    foreground.release();
}

TEST(SyncArbiterTests, RepeatedAcquireOfTheSameHolderReturnsAtOnce)  // [SyncArbiterTests.md#RepeatedAcquireOfTheSameHolderReturnsAtOnce]
{
    std::atomic_bool abort{false};
    SyncArbiter foreground(SyncRole::Foreground);
    ASSERT_TRUE(foreground.acquire(abort));
    bool acquired = false;
    EXPECT_LT(time_acquire(foreground, abort, acquired).count(), 100);
    EXPECT_TRUE(acquired);
    foreground.release();
}
