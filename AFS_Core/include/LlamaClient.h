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
#pragma once

#include <memory>
#include <mutex>
#include "ChatTypes.h"

struct llama_model;
struct llama_context;
struct llama_vocab;

namespace AFS_Core {

class LlamaClient {  // [LlamaClient.md#LlamaClient]
    struct ModelDeleter { void operator()(llama_model*) const; };
    struct ContextDeleter { void operator()(llama_context*) const; };

    std::mutex                                      mutex_;  // [LlamaClient.md#mutex_]
    std::unique_ptr<llama_model, ModelDeleter>      model_;
    std::unique_ptr<llama_context, ContextDeleter>  context_;
    const llama_vocab*                              vocab_ = nullptr;

    bool ensure_loaded(std::wstring& error);
    std::wstring build_prompt(const ChatHistory& history) const;  // [LlamaClient.md#build_prompt]
public:
    static bool available();  // [LlamaClient.md#available]
    static std::wstring missing_model_hint();  // [LlamaClient.md#missing_model_hint]
    LlamaClient() = default;
    ~LlamaClient();
    LlamaClient(const LlamaClient&) = delete;
    LlamaClient& operator=(const LlamaClient&) = delete;
    ChatReply send(const ChatHistory& history);  // [LlamaClient.md#send]
    ChatReply analyze_log(const ChatHistory& history, const std::wstring& log_excerpt);  // [LlamaClient.md#analyze_log]
};

} // namespace AFS_Core
