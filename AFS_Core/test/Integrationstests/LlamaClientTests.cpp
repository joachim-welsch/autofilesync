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
#include "LlamaClient.h"

using namespace AFS_Core;

namespace {

ChatHistory conversation()
{
    return {{true, L"F1"}, {false, L"A1"}, {true, L"F2"}, {false, L"A2"}, {true, L"F3"}};
}

std::vector<std::wstring> texts(const ChatHistory& history)
{
    std::vector<std::wstring> retval;
    for (const ChatTurn& turn : history)
        retval.push_back(turn.text);
    return retval;
}

}

TEST(LlamaClientTests, ConversationThatFitsIsKeptWhole)
{
    EXPECT_EQ(texts(LlamaClient::fitting(conversation(), [](const ChatHistory&) { return true; })), texts(conversation()));
}

TEST(LlamaClientTests, OldestRoundsGoFirst)  // [LlamaClientTests.md#OldestRoundsGoFirst]
{
    const ChatHistory recent = LlamaClient::fitting(conversation(), [](const ChatHistory& h) { return h.size() <= 3; });

    EXPECT_EQ(texts(recent), (std::vector<std::wstring>{L"F2", L"A2", L"F3"}));
}

TEST(LlamaClientTests, ShortenedConversationStartsWithAQuestion)
{
    const ChatHistory recent = LlamaClient::fitting(conversation(), [](const ChatHistory& h) { return h.size() <= 4; });

    ASSERT_FALSE(recent.empty());
    EXPECT_TRUE(recent.front().from_user) << "Das gekürzte Gespräch beginnt mit einer Antwort.";
    EXPECT_EQ(texts(recent), (std::vector<std::wstring>{L"F2", L"A2", L"F3"}));
}

TEST(LlamaClientTests, QuestionThatDoesNotFitAloneLeavesNothing)
{
    EXPECT_TRUE(LlamaClient::fitting(conversation(), [](const ChatHistory&) { return false; }).empty());
}
