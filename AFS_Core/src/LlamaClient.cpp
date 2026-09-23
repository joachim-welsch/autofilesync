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
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <vector>
#include <llama.h>
#include <Logging.h>
#include "ModelLocation.h"
#include "Translation.h"
#include "Util.h"

namespace AFS_Core {

using std::string;
using std::wstring;
using util::string_to_wstring;
using util::wstring_to_string;

namespace {
constexpr const char* SYSTEM_PROMPT = translation::msgid("assistant_system_prompt");  // [LlamaClient.md#SYSTEM_PROMPT]
constexpr const char* LOG_ANALYSIS_INSTRUCTION = translation::msgid("assistant_log_analysis_instruction");  // [LlamaClient.md#LOG_ANALYSIS_INSTRUCTION]
}

void LlamaClient::ModelDeleter::operator()(llama_model* model) const { llama_model_free(model); }
void LlamaClient::ContextDeleter::operator()(llama_context* ctx) const { llama_free(ctx); }

LlamaClient::~LlamaClient() = default;

bool LlamaClient::available()
{
    std::error_code ec;
    return std::filesystem::is_regular_file(model_location::find(), ec);
}

wstring LlamaClient::missing_model_hint()
{
    return translation::translate("assistant_no_model_configured",
        {model_location::directory().wstring(), string_to_wstring(model_location::PATH_VARIABLE)});
}

bool LlamaClient::ensure_loaded(wstring& error)
{
    if (context_)
        return true;
    const std::filesystem::path path = model_location::find();
    if (path.empty()) {
        error = missing_model_hint();
        return false;
    }
    static const bool BACKEND_READY = (llama_backend_init(), true);  // [LlamaClient.md#backend-init]
    (void)BACKEND_READY;
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0; // CPU only: no assumption about the user's GPU/driver setup
    const std::u8string utf8_path = path.u8string();  // [LlamaClient.md#utf8-path]
    llama_model* model = llama_model_load_from_file(reinterpret_cast<const char*>(utf8_path.c_str()), model_params);
    if (model == nullptr) {
        error = translation::translate("assistant_model_not_loadable") + path.wstring();
        return false;
    }
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 8192;
    const auto threads = static_cast<int32_t>(std::max(1u, std::thread::hardware_concurrency()));  // [LlamaClient.md#batch-size]
    ctx_params.n_threads = threads;
    ctx_params.n_threads_batch = threads;
    llama_context* ctx = llama_init_from_model(model, ctx_params);
    if (ctx == nullptr) {
        llama_model_free(model);
        error = translation::translate("assistant_model_context_failed");
        return false;
    }
    model_.reset(model);
    context_.reset(ctx);
    vocab_ = llama_model_get_vocab(model);
    return true;
}

wstring LlamaClient::build_prompt(const ChatHistory& history) const
{
    std::vector<string> contents;  // [LlamaClient.md#contents-lifetime]
    contents.reserve(history.size() + 1);
    contents.push_back(translation::translate_utf8(SYSTEM_PROMPT));
    for (const ChatTurn& turn : history)
        contents.push_back(wstring_to_string(turn.text));
    std::vector<llama_chat_message> messages;
    messages.reserve(history.size() + 1);
    messages.push_back({ "system", contents[0].c_str() });
    for (std::size_t i = 0; i < history.size(); ++i)
        messages.push_back({ history[i].from_user ? "user" : "assistant", contents[i + 1].c_str() });

    const char* tmpl = llama_model_chat_template(model_.get(), nullptr);
    std::vector<char> buf(4096);
    for (;;) {
        const int32_t needed = llama_chat_apply_template(tmpl, messages.data(), messages.size(),
            true, buf.data(), static_cast<int32_t>(buf.size()));
        if (needed < 0)
            return {};
        if (needed <= static_cast<int32_t>(buf.size()))
            return string_to_wstring(string(buf.data(), needed));
        buf.resize(needed);
    }
}

ChatReply LlamaClient::send(const ChatHistory& history)
{
    std::lock_guard<std::mutex> lock(mutex_);
    wstring error;
    if (!ensure_loaded(error))
        return { false, error };
    if (history.empty())
        return { false, translation::translate("assistant_nothing_to_send") };
    const wstring prompt = build_prompt(history);
    if (prompt.empty())
        return { false, translation::translate("assistant_unknown_chat_format") };

    llama_context* ctx = context_.get();
    llama_memory_clear(llama_get_memory(ctx), true);  // [LlamaClient.md#kv-cache-cleared]

    const string utf8_prompt = wstring_to_string(prompt);
    std::vector<llama_token> tokens(utf8_prompt.size() + 32);
    int32_t n_tokens = llama_tokenize(vocab_, utf8_prompt.data(), static_cast<int32_t>(utf8_prompt.size()),
        tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    if (n_tokens < 0) {
        tokens.resize(-n_tokens);
        n_tokens = llama_tokenize(vocab_, utf8_prompt.data(), static_cast<int32_t>(utf8_prompt.size()),
            tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    }
    tokens.resize(n_tokens);
    if (tokens.empty() || static_cast<uint32_t>(tokens.size()) >= llama_n_ctx(ctx)  // [LlamaClient.md#prompt-size-check]
        || static_cast<uint32_t>(tokens.size()) >= llama_n_batch(ctx)) {
        log_error(L"LlamaClient: Prompt zu lang, Tokens: ", std::to_wstring(tokens.size()));
        return { false, translation::translate("assistant_conversation_too_long") };
    }
    if (llama_decode(ctx, llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()))) != 0)
        return { false, translation::translate("assistant_prompt_failed") };

    std::unique_ptr<llama_sampler, void(*)(llama_sampler*)> sampler(
        llama_sampler_chain_init(llama_sampler_chain_default_params()), llama_sampler_free);
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_temp(0.7f));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    string answer;
    constexpr int MAX_NEW_TOKENS = 2048;  // [LlamaClient.md#token-budget]
    for (int i = 0; i < MAX_NEW_TOKENS; ++i) {
        llama_token token = llama_sampler_sample(sampler.get(), ctx, -1);
        llama_sampler_accept(sampler.get(), token);
        if (llama_vocab_is_eog(vocab_, token))
            break;
        char piece[256];
        const int32_t len = llama_token_to_piece(vocab_, token, piece, sizeof piece, 0, true);
        if (len > 0)
            answer.append(piece, len);
        if (llama_decode(ctx, llama_batch_get_one(&token, 1)) != 0)
            break;
    }
    if (answer.empty())
        return { false, translation::translate("assistant_empty_answer") };
    return { true, string_to_wstring(answer) };
}

ChatReply LlamaClient::analyze_log(const ChatHistory& history, const wstring& log_excerpt)
{
    if (history.empty())
        return { false, translation::translate("assistant_nothing_to_send") };
    if (log_excerpt.empty())
        return { false, translation::translate("assistant_log_file_unreadable") };
    ChatHistory with_excerpt = history;
    with_excerpt.back().text = translation::translate(LOG_ANALYSIS_INSTRUCTION) + L"\n\n" + log_excerpt +
        L"\n\n" + translation::translate("assistant_question_label") + with_excerpt.back().text;
    return send(with_excerpt);
}

} // namespace AFS_Core
