#include "pch.h"
#include "LlamaClient.h"
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <vector>
#include <llama.h>
#include <Logging.h>
#include "util.h"

using std::string;
using std::wstring;
using util::string_to_wstring;
using util::wstring_to_string;

namespace {
// Sent as the "system" turn on every request: the model has no other way to
// know what Autofilesync is, since there is no RAG/document lookup.
const string systemPrompt =
    "Du bist der eingebaute Assistent von Autofilesync, einem Programm zur "
    "benutzerfreundlichen automatischen Synchronisation von Dateien und Verzeichnisbaeumen "
    "unter Windows und Linux. Antworte auf Deutsch, kurz und konkret bezogen auf Autofilesync.\n"
    "Kernkonzepte:\n"
    "- Der Nutzer legt Synchronisationen aus einem Quell- und einem Zielverzeichnis an; "
    "Autofilesync gleicht deren Inhalt automatisch ab, sobald sich dort etwas aendert.\n"
    "- Ein GUI-Programm (wxWidgets) dient der Konfiguration der Synchronisationen.\n"
    "- Ein Hintergrunddienst/Daemon fuehrt dieselbe Konfiguration ohne offene "
    "Bedienoberflaeche im Hintergrund aus.\n"
    "- Oberstes Ziel ist Robustheit, danach eine so intuitive Bedienung, dass unerfahrene "
    "Nutzer sie ohne Dokumentation verstehen.\n"
    "Wenn du eine Frage nicht sicher aus diesem Wissen beantworten kannst, sag das offen, "
    "statt Funktionen zu erfinden, die es nicht gibt.";

// Sent in front of the excerpt for analyzeLog(): a plain log dump without an
// instruction leaves the model guessing what kind of answer is wanted.
const wstring logAnalysisInstruction =
    L"Der folgende Ausschnitt stammt aus der Autofilesync-Logdatei (neueste Eintraege stehen "
    L"zuletzt). Nutze ihn, um die anschliessende Frage des Nutzers zu beantworten. Enthaelt der "
    L"Ausschnitt dafuer nichts Passendes, sag das offen, statt etwas zu erfinden.\n\n";
}

void LlamaClient::ModelDeleter::operator()(llama_model* model) const { llama_model_free(model); }
void LlamaClient::ContextDeleter::operator()(llama_context* ctx) const { llama_free(ctx); }

LlamaClient::~LlamaClient() = default;

bool LlamaClient::available()
{
    const char* path = std::getenv(modelPathVariable);
    return path != nullptr && *path != '\0' && std::filesystem::exists(path);
}

bool LlamaClient::ensureLoaded(wstring& error)
{
    if (context_)
        return true;
    const char* path = std::getenv(modelPathVariable);
    if (path == nullptr || *path == '\0') {
        error = L"Kein lokales Modell konfiguriert. Bitte " + string_to_wstring(modelPathVariable) +
                L" auf eine GGUF-Datei setzen und das Programm neu starten.";
        return false;
    }
    // Idempotent and required exactly once before any other llama_* call;
    // cheapest to do lazily here rather than force every caller to remember it.
    static const bool backendReady = (llama_backend_init(), true);
    (void)backendReady;
    llama_model_params modelParams = llama_model_default_params();
    modelParams.n_gpu_layers = 0; // CPU only: no assumption about the user's GPU/driver setup
    llama_model* model = llama_model_load_from_file(path, modelParams);
    if (model == nullptr) {
        error = L"Modell konnte nicht geladen werden: " + string_to_wstring(path);
        return false;
    }
    llama_context_params ctxParams = llama_context_default_params();
    ctxParams.n_ctx = 8192;
    // n_batch is deliberately left at its library default (2048), not raised
    // to match n_ctx: send() hands the whole prompt to llama_decode() in one
    // llama_batch_get_one() call, and a several-thousand-token prompt (e.g. a
    // log excerpt) submitted as a single batch this large measured drastically
    // slower on the CPU backend than the same tokens processed within the
    // default batch size - minutes instead of seconds. send() checks the
    // prompt against llama_n_batch() before ever calling decode, so a prompt
    // this large is refused outright, not hung.
    const auto threads = static_cast<int32_t>(std::max(1u, std::thread::hardware_concurrency()));
    ctxParams.n_threads = threads;
    ctxParams.n_threads_batch = threads;
    llama_context* ctx = llama_init_from_model(model, ctxParams);
    if (ctx == nullptr) {
        llama_model_free(model);
        error = L"Fuer das Modell konnte kein Kontext erstellt werden.";
        return false;
    }
    model_.reset(model);
    context_.reset(ctx);
    vocab_ = llama_model_get_vocab(model);
    return true;
}

wstring LlamaClient::buildPrompt(const ChatHistory& history) const
{
    // llama_chat_message.content points into these, so they must outlive the
    // llama_chat_apply_template() calls below.
    std::vector<string> contents;
    contents.reserve(history.size() + 1);
    contents.push_back(systemPrompt);
    for (const ChatTurn& turn : history)
        contents.push_back(wstring_to_string(turn.text));
    std::vector<llama_chat_message> messages;
    messages.reserve(history.size() + 1);
    messages.push_back({ "system", contents[0].c_str() });
    for (std::size_t i = 0; i < history.size(); ++i)
        messages.push_back({ history[i].fromUser ? "user" : "assistant", contents[i + 1].c_str() });

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
    if (!ensureLoaded(error))
        return { false, error };
    if (history.empty())
        return { false, L"Es gibt nichts zu senden." };
    const wstring prompt = buildPrompt(history);
    if (prompt.empty())
        return { false, L"Das Modell unterstuetzt kein bekanntes Chat-Format." };

    llama_context* ctx = context_.get();
    // Every turn re-sends the whole history (see ChatTypes.h), so the previous
    // turn's KV cache is stale weight, not something to build on.
    llama_memory_clear(llama_get_memory(ctx), true);

    const string utf8Prompt = wstring_to_string(prompt);
    std::vector<llama_token> tokens(utf8Prompt.size() + 32);
    int32_t nTokens = llama_tokenize(vocab_, utf8Prompt.data(), static_cast<int32_t>(utf8Prompt.size()),
        tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    if (nTokens < 0) {
        tokens.resize(-nTokens);
        nTokens = llama_tokenize(vocab_, utf8Prompt.data(), static_cast<int32_t>(utf8Prompt.size()),
            tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    }
    tokens.resize(nTokens);
    // Checked against n_batch, not just n_ctx: send() submits the whole prompt
    // to llama_decode() as a single batch (see ensureLoaded()), so a prompt
    // that fits the context window can still exceed what one batch may carry -
    // GGML_ASSERT(n_tokens_all <= n_batch) aborts the whole process instead of
    // just this request, and there is no recovering from that here.
    if (tokens.empty() || static_cast<uint32_t>(tokens.size()) >= llama_n_ctx(ctx)
        || static_cast<uint32_t>(tokens.size()) >= llama_n_batch(ctx)) {
        LOG_ERROR2(L"LlamaClient: Prompt zu lang, Tokens: ", std::to_wstring(tokens.size()));
        return { false, L"Die Unterhaltung ist laenger als das Kontextfenster des Modells." };
    }
    if (llama_decode(ctx, llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()))) != 0)
        return { false, L"Der Prompt konnte nicht verarbeitet werden." };

    std::unique_ptr<llama_sampler, void(*)(llama_sampler*)> sampler(
        llama_sampler_chain_init(llama_sampler_chain_default_params()), llama_sampler_free);
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_temp(0.7f));
    llama_sampler_chain_add(sampler.get(), llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    string answer;
    // Thinking and answer share this budget - a fixed cap keeps one runaway
    // reply from filling the whole context window.
    constexpr int maxNewTokens = 2048;
    for (int i = 0; i < maxNewTokens; ++i) {
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
        return { false, L"Die Antwort enthielt keinen Text." };
    return { true, string_to_wstring(answer) };
}

ChatReply LlamaClient::analyzeLog(const ChatHistory& history, const wstring& logExcerpt)
{
    if (history.empty())
        return { false, L"Es gibt nichts zu senden." };
    if (logExcerpt.empty())
        return { false, L"Die Logdatei ist leer oder konnte nicht gelesen werden." };
    ChatHistory withExcerpt = history;
    withExcerpt.back().text = logAnalysisInstruction + logExcerpt + L"\n\nFrage: " + withExcerpt.back().text;
    return send(withExcerpt);
}
