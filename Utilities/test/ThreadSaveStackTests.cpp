/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
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
#include <ThreadSaveStack.h>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace Utilities;

TEST(ThreadSaveStackTests, NewStackIsEmpty)
{
    const ThreadSaveStack<int> stack;

    EXPECT_TRUE(stack.empty());
}

TEST(ThreadSaveStackTests, PopsInReverseOrderOfPush)
{
    ThreadSaveStack<std::string> stack;
    stack.push("first");
    stack.push("second");
    std::string value;

    stack.pop(value);

    EXPECT_EQ(value, "second");
    EXPECT_EQ(*stack.pop(), "first");
    EXPECT_TRUE(stack.empty());
}

TEST(ThreadSaveStackTests, PopOfAnEmptyStackThrowsAndLeavesTheValueAlone)
{
    ThreadSaveStack<int> stack;
    int value = 7;

    EXPECT_THROW(stack.pop(value), EmptyStack);
    EXPECT_THROW(stack.pop(), EmptyStack);
    EXPECT_EQ(value, 7);
    EXPECT_STREQ(EmptyStack().what(), "empty stack");
}

TEST(ThreadSaveStackTests, CopyIsIndependentOfTheOriginal)
{
    ThreadSaveStack<int> original;
    original.push(1);
    original.push(2);

    ThreadSaveStack<int> copy(original);
    copy.push(3);

    EXPECT_EQ(*copy.pop(), 3);
    EXPECT_EQ(*copy.pop(), 2);
    EXPECT_EQ(*original.pop(), 2);
    EXPECT_EQ(*original.pop(), 1);
    EXPECT_TRUE(original.empty());
    EXPECT_FALSE(copy.empty());
}

TEST(ThreadSaveStackTests, ConcurrentPushAndPopLoseAndDuplicateNothing)  // [ThreadSaveStackTests.md#ConcurrentPushAndPopLoseAndDuplicateNothing]
{
    constexpr int THREADS = 4;
    constexpr int VALUES_PER_THREAD = 5000;
    ThreadSaveStack<int> stack;
    std::atomic<long long> popped_sum{0};
    std::atomic<int> popped_count{0};
    std::atomic<int> producers_running{THREADS};
    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t)
        threads.emplace_back([&stack, &producers_running, t] {
            for (int i = 0; i < VALUES_PER_THREAD; ++i)
                stack.push(t * VALUES_PER_THREAD + i);
            --producers_running;
        });
    for (int t = 0; t < THREADS; ++t)
        threads.emplace_back([&] {
            for (;;) {
                const bool last_round = producers_running == 0;
                try {
                    popped_sum += *stack.pop();
                    ++popped_count;
                }
                catch (const EmptyStack&) {
                    if (last_round)
                        return;
                    std::this_thread::yield();
                }
            }
        });
    for (std::thread& thread : threads)
        thread.join();

    const long long total = static_cast<long long>(THREADS) * VALUES_PER_THREAD;
    EXPECT_EQ(popped_count, total);
    EXPECT_EQ(popped_sum, total * (total - 1) / 2);
    EXPECT_TRUE(stack.empty());
}
