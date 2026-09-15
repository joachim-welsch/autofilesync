#pragma once

#include <memory>
#include <mutex>
#include "ChatTypes.h"

struct llama_model;
struct llama_context;
struct llama_vocab;

namespace AFS_Core {

class LlamaClient {  // [LlamaClient.md#LlamaClient]
public:
    static bool available();  // [LlamaClient.md#available]
    static std::wstring missing_model_hint();  // [LlamaClient.md#missing_model_hint]

    LlamaClient() = default;
    ~LlamaClient();
    LlamaClient(const LlamaClient&) = delete;
    LlamaClient& operator=(const LlamaClient&) = delete;

    ChatReply send(const ChatHistory& history);  // [LlamaClient.md#send]

    ChatReply analyze_log(const ChatHistory& history, const std::wstring& log_excerpt);  // [LlamaClient.md#analyze_log]

private:
    bool ensure_loaded(std::wstring& error);
    std::wstring build_prompt(const ChatHistory& history) const;  // [LlamaClient.md#build_prompt]

    std::mutex mutex_;  // [LlamaClient.md#mutex_]
    struct ModelDeleter { void operator()(llama_model*) const; };
    struct ContextDeleter { void operator()(llama_context*) const; };
    std::unique_ptr<llama_model, ModelDeleter> model_;
    std::unique_ptr<llama_context, ContextDeleter> context_;
    const llama_vocab* vocab_ = nullptr;
};

} // namespace AFS_Core
