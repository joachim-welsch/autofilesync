#pragma once

#include <memory>
#include <mutex>
#include "ChatTypes.h"

struct llama_model;
struct llama_context;
struct llama_vocab;

// Runs a GGUF model locally through llama.cpp - no network, no API key. The
// model file comes from AUTOFILESYNC_LLAMA_MODEL; nothing is loaded until the
// first send(), so an unset variable costs nothing at startup.
class LlamaClient {
public:
    static constexpr char modelPathVariable[] = "AUTOFILESYNC_LLAMA_MODEL";
    // True if the variable points at a file that exists; does not load the
    // model, so this is cheap enough to call whenever the chat window opens.
    static bool available();

    LlamaClient() = default;
    ~LlamaClient();
    LlamaClient(const LlamaClient&) = delete;
    LlamaClient& operator=(const LlamaClient&) = delete;

    // Blocks until the answer arrives or generation fails; never throws, so a
    // worker thread cannot terminate the program. Loads the model on the first
    // call and keeps it for the lifetime of this object; later calls only pay
    // for re-decoding the (short) chat history.
    ChatReply send(const ChatHistory& history);

    // Answers the latest turn of history with a log excerpt (see
    // readRecentLogEntries()) folded into it. Only this one request sees the
    // excerpt - the caller's own copy of history is untouched, so a persisted
    // ChatHistory never carries it forward into every later send() call.
    ChatReply analyzeLog(const ChatHistory& history, const std::wstring& logExcerpt);

private:
    bool ensureLoaded(std::wstring& error);
    // Renders the history through the model's own chat template; empty if the
    // model carries no template llama.cpp recognises.
    std::wstring buildPrompt(const ChatHistory& history) const;

    // Serialises send(): a llama_context's decode/KV-cache state is not safe
    // to touch from two threads at once, and ChatDialog disables the button
    // while a reply is pending but does not guarantee only one caller exists.
    std::mutex mutex_;
    struct ModelDeleter { void operator()(llama_model*) const; };
    struct ContextDeleter { void operator()(llama_context*) const; };
    std::unique_ptr<llama_model, ModelDeleter> model_;
    std::unique_ptr<llama_context, ContextDeleter> context_;
    const llama_vocab* vocab_ = nullptr;
};
