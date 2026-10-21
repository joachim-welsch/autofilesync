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

#include <cstdint>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include "ChatTypes.h"

struct llama_model;
struct llama_context;
struct llama_vocab;

namespace AFS_Core {

using ChatPieceSink = std::function<void(const std::wstring& piece)>;  // [LlamaClient.md#ChatPieceSink]

class LlamaClient {  // [LlamaClient.md#LlamaClient]
    struct ModelDeleter { void operator()(llama_model*) const; };
    struct ContextDeleter { void operator()(llama_context*) const; };

    std::mutex                                      mutex_;  // [LlamaClient.md#mutex_]
    mutable std::mutex                              situation_mutex_;
    std::wstring                                    situation_;  // [LlamaClient.md#situation_]
    std::unique_ptr<llama_model, ModelDeleter>      model_;
    std::unique_ptr<llama_context, ContextDeleter>  context_;
    const llama_vocab*                              vocab_ = nullptr;
    std::vector<std::int32_t>                       cached_tokens_;  // [LlamaClient.md#cached_tokens_]
    std::atomic<bool>                               cancelled_ = false;  // [LlamaClient.md#cancel]

    static bool abort_requested(void* client);

    bool ensure_loaded(std::wstring& error);
    std::wstring build_prompt(const ChatHistory& history) const;  // [LlamaClient.md#build_prompt]
    std::vector<std::int32_t> tokenize(const std::wstring& prompt) const;
    bool decode_prompt(std::vector<std::int32_t>& tokens);  // [LlamaClient.md#decode_prompt]
    std::string generate(const ChatPieceSink& on_piece, const std::string& grammar = {});  // [LlamaClient.md#generate]
    void forget_cache();
    ChatReply complete(const ChatHistory& history, const ChatPieceSink& on_piece, const std::string& grammar);  // [LlamaClient.md#complete]
    ChatReply reply_to(const ChatHistory& history, const ChatPieceSink& on_piece);
    ChatReply with_proposal(const ChatHistory& history, ChatReply reply);  // [LlamaClient.md#with_proposal]
public:
    static bool available();  // [LlamaClient.md#available]
    static std::wstring missing_model_hint();  // [LlamaClient.md#missing_model_hint]
    static ChatHistory fitting(const ChatHistory& history, const std::function<bool(const ChatHistory&)>& fits);  // [LlamaClient.md#fitting]
    LlamaClient() = default;
    ~LlamaClient();
    LlamaClient(const LlamaClient&) = delete;
    LlamaClient& operator=(const LlamaClient&) = delete;
    void situation(const std::wstring& text);  // [LlamaClient.md#situation_]
    void cancel();  // [LlamaClient.md#cancel]
    ChatReply send(const ChatHistory& history, const ChatPieceSink& on_piece = {});  // [LlamaClient.md#send]
    ChatReply analyze_log(const ChatHistory& history, const std::wstring& log_excerpt,
        const ChatPieceSink& on_piece = {});  // [LlamaClient.md#analyze_log]
    ChatReply answer_from_help(const ChatHistory& history, const std::wstring& help_excerpt,
        const ChatPieceSink& on_piece = {});  // [LlamaClient.md#answer_from_help]
};

} // namespace AFS_Core
