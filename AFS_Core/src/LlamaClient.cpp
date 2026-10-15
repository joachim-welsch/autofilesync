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
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <llama.h>
#include <Logging.h>
#include "Config.h"
#include "ConfigSummary.h"
#include "CpuTopology.h"
#include "ModelLocation.h"
#include "SyncProposal.h"
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
constexpr const char* HELP_INSTRUCTION = translation::msgid("assistant_help_instruction");  // [LlamaClient.md#HELP_INSTRUCTION]
constexpr int MAX_NEW_TOKENS = 2048;  // [LlamaClient.md#token-budget]

std::size_t complete_utf8_length(const string& text)  // [LlamaClient.md#complete_utf8_length]
{
    const std::size_t size = text.size();
    for (std::size_t back = 1; back <= std::min<std::size_t>(size, 4); ++back) {
        const auto byte = static_cast<unsigned char>(text[size - back]);
        if ((byte & 0xC0) == 0x80)
            continue;
        const std::size_t length = byte >= 0xF0 ? 4 : byte >= 0xE0 ? 3 : byte >= 0xC0 ? 2 : 1;
        return length > back ? size - back : size;
    }
    return size;
}

std::size_t common_prefix(const std::vector<llama_token>& a, const std::vector<llama_token>& b)
{
    return static_cast<std::size_t>(std::mismatch(a.begin(), a.end(), b.begin(), b.end()).first - a.begin());
}
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
    llama_context_params ctx_params = llama_context_default_params();  // [LlamaClient.md#batch-size]
    ctx_params.n_ctx = 8192;
    const auto threads = static_cast<int32_t>(cpu_topology::physical_cores());  // [CpuTopology.md#physical_cores]
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
    contents.push_back(translation::translate_utf8(SYSTEM_PROMPT) + "\n\n" + wstring_to_string(config_summary::current())  // [LlamaClient.md#config-in-system-prompt]
        + "\n\n" + wstring_to_string(sync_proposal::instruction()));  // [SyncProposal.md#instruction]
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

std::vector<llama_token> LlamaClient::tokenize(const wstring& prompt) const
{
    const string utf8_prompt = wstring_to_string(prompt);
    std::vector<llama_token> tokens(utf8_prompt.size() + 32);
    int32_t n_tokens = llama_tokenize(vocab_, utf8_prompt.data(), static_cast<int32_t>(utf8_prompt.size()),
        tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    if (n_tokens < 0) {
        tokens.resize(-n_tokens);
        n_tokens = llama_tokenize(vocab_, utf8_prompt.data(), static_cast<int32_t>(utf8_prompt.size()),
            tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    }
    tokens.resize(std::max(n_tokens, 0));
    return tokens;
}

void LlamaClient::forget_cache()
{
    llama_memory_clear(llama_get_memory(context_.get()), true);
    cached_tokens_.clear();
}

bool LlamaClient::decode_prompt(std::vector<llama_token>& tokens)
{
    llama_context* ctx = context_.get();
    std::size_t reused = std::min(common_prefix(cached_tokens_, tokens), tokens.size() - 1);  // [LlamaClient.md#last-token-decoded]
    if (llama_memory_seq_rm(llama_get_memory(ctx), 0, static_cast<llama_pos>(reused), -1))  // [LlamaClient.md#kv-cache-reuse]
        cached_tokens_.resize(reused);
    else {
        forget_cache();
        reused = 0;
    }
    const std::size_t batch = llama_n_batch(ctx);
    for (std::size_t pos = reused; pos < tokens.size(); pos += batch) {  // [LlamaClient.md#batch-size]
        const auto count = static_cast<int32_t>(std::min(batch, tokens.size() - pos));
        if (llama_decode(ctx, llama_batch_get_one(tokens.data() + pos, count)) != 0) {
            forget_cache();
            return false;
        }
    }
    cached_tokens_ = tokens;
    return true;
}

string LlamaClient::generate(const ChatPieceSink& on_piece, const string& grammar)
{
    std::unique_ptr<llama_sampler, void(*)(llama_sampler*)> sampler(
        llama_sampler_chain_init(llama_sampler_chain_default_params()), llama_sampler_free);
    if (!grammar.empty())
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_grammar(vocab_, grammar.c_str(), "root"));  // [LlamaClient.md#grammar-first]
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_temp(0.7f));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    string answer;
    std::size_t passed_on = 0;
    for (int i = 0; i < MAX_NEW_TOKENS; ++i) {
        llama_token token = llama_sampler_sample(sampler.get(), context_.get(), -1);  // [LlamaClient.md#sample-accepts]
        if (llama_vocab_is_eog(vocab_, token))
            break;
        char piece[256];
        const int32_t len = llama_token_to_piece(vocab_, token, piece, sizeof piece, 0, true);
        if (len > 0)
            answer.append(piece, len);
        if (const std::size_t complete = complete_utf8_length(answer); on_piece && complete > passed_on) {
            on_piece(string_to_wstring(answer.substr(passed_on, complete - passed_on)));
            passed_on = complete;
        }
        if (llama_decode(context_.get(), llama_batch_get_one(&token, 1)) != 0) {
            forget_cache();
            break;
        }
        cached_tokens_.push_back(token);
    }
    return answer;
}

ChatHistory LlamaClient::fitting(const ChatHistory& history, const std::function<bool(const ChatHistory&)>& fits)
{
    ChatHistory recent = history;
    while (!recent.empty() && !fits(recent)) {
        recent.erase(recent.begin());
        while (!recent.empty() && !recent.front().from_user)  // [LlamaClient.md#fitting]
            recent.erase(recent.begin());
    }
    return recent;
}

ChatReply LlamaClient::complete(const ChatHistory& history, const ChatPieceSink& on_piece, const string& grammar)
{
    if (build_prompt(history).empty())
        return { false, translation::translate("assistant_unknown_chat_format") };
    std::vector<llama_token> tokens;
    const ChatHistory recent = fitting(history, [this, &tokens](const ChatHistory& candidate) {  // [LlamaClient.md#prompt-size-check]
        tokens = tokenize(build_prompt(candidate));
        return !tokens.empty() && tokens.size() + MAX_NEW_TOKENS <= llama_n_ctx(context_.get());
    });
    if (recent.empty()) {
        log_error(L"LlamaClient: Frage passt nicht ins Kontextfenster, Tokens: ", std::to_wstring(tokens.size()));
        return { false, translation::translate("assistant_conversation_too_long") };
    }
    if (recent.size() < history.size())
        log_info(L"LlamaClient: ältere Gesprächsrunden weggelassen: ", std::to_wstring(history.size() - recent.size()));
    if (!decode_prompt(tokens))
        return { false, translation::translate("assistant_prompt_failed") };
    const string answer = generate(on_piece, grammar);
    if (answer.empty())
        return { false, translation::translate("assistant_empty_answer") };
    return { true, string_to_wstring(answer) };
}

ChatReply LlamaClient::with_proposal(const ChatHistory& history, ChatReply reply)
{
    const SyncItems items = Config::instance().syncitems();
    const wstring& question = history.back().text;
    if (!reply.ok || sync_proposal::parse(reply.text)
        || !(sync_proposal::names_two_paths(question) || sync_proposal::names_configured_item(question, items)))  // [SyncProposal.md#names_two_paths]
        return reply;
    ChatHistory extraction = history;
    extraction.push_back({ false, reply.text });
    extraction.push_back({ true, sync_proposal::extraction_question() });  // [SyncProposal.md#extraction]
    const ChatReply json = complete(extraction, {}, sync_proposal::grammar(items));
    if (json.ok && sync_proposal::parse(json.text))
        reply.text += L"\n" + sync_proposal::as_block(json.text);
    return reply;
}

ChatReply LlamaClient::reply_to(const ChatHistory& history, const ChatPieceSink& on_piece)
{
    wstring error;
    if (!ensure_loaded(error))
        return { false, error };
    if (history.empty())
        return { false, translation::translate("assistant_nothing_to_send") };
    return complete(history, on_piece, {});
}

ChatReply LlamaClient::send(const ChatHistory& history, const ChatPieceSink& on_piece)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return with_proposal(history, reply_to(history, on_piece));
}

ChatReply LlamaClient::analyze_log(const ChatHistory& history, const wstring& log_excerpt, const ChatPieceSink& on_piece)
{
    if (history.empty())
        return { false, translation::translate("assistant_nothing_to_send") };
    if (log_excerpt.empty())
        return { false, translation::translate("assistant_log_file_unreadable") };
    ChatHistory with_excerpt = history;
    with_excerpt.back().text = translation::translate(LOG_ANALYSIS_INSTRUCTION) + L"\n\n" + log_excerpt +
        L"\n\n" + translation::translate("assistant_question_label") + with_excerpt.back().text;
    std::lock_guard<std::mutex> lock(mutex_);
    return reply_to(with_excerpt, on_piece);  // [LlamaClient.md#no-proposal-from-log]
}

ChatReply LlamaClient::answer_from_help(const ChatHistory& history, const wstring& help_excerpt, const ChatPieceSink& on_piece)
{
    if (history.empty())
        return { false, translation::translate("assistant_nothing_to_send") };
    ChatHistory with_excerpt = history;
    with_excerpt.back().text = translation::translate(HELP_INSTRUCTION) + L"\n\n" + help_excerpt +
        L"\n\n" + translation::translate("assistant_question_label") + with_excerpt.back().text;
    std::lock_guard<std::mutex> lock(mutex_);
    return with_proposal(history, reply_to(with_excerpt, on_piece));  // [LlamaClient.md#answer_from_help]
}

} // namespace AFS_Core
