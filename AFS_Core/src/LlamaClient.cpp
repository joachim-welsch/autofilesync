#include "pch.h"
#include "LlamaClient.h"
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <vector>
#include <llama.h>
#include <Logging.h>
#include "Util.h"

namespace AFS_Core {

using std::string;
using std::wstring;
using util::string_to_wstring;
using util::wstring_to_string;

namespace {
const string SYSTEM_PROMPT =  // [LlamaClient.md#SYSTEM_PROMPT]
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

const wstring LOG_ANALYSIS_INSTRUCTION =  // [LlamaClient.md#LOG_ANALYSIS_INSTRUCTION]
    L"Der folgende Ausschnitt stammt aus der Autofilesync-Logdatei (neueste Eintraege stehen "
    L"zuletzt). Nutze ihn, um die anschliessende Frage des Nutzers zu beantworten. Enthaelt der "
    L"Ausschnitt dafuer nichts Passendes, sag das offen, statt etwas zu erfinden.\n\n";
}

void LlamaClient::ModelDeleter::operator()(llama_model* model) const { llama_model_free(model); }
void LlamaClient::ContextDeleter::operator()(llama_context* ctx) const { llama_free(ctx); }

LlamaClient::~LlamaClient() = default;

bool LlamaClient::available()
{
    const char* path = std::getenv(MODEL_PATH_VARIABLE);
    std::error_code ec;
    return path != nullptr && *path != '\0' && std::filesystem::exists(path, ec);
}

bool LlamaClient::ensure_loaded(wstring& error)
{
    if (context_)
        return true;
    const char* path = std::getenv(MODEL_PATH_VARIABLE);
    if (path == nullptr || *path == '\0') {
        error = L"Kein lokales Modell konfiguriert. Bitte " + string_to_wstring(MODEL_PATH_VARIABLE) +
                L" auf eine GGUF-Datei setzen und das Programm neu starten.";
        return false;
    }
    static const bool BACKEND_READY = (llama_backend_init(), true);  // [LlamaClient.md#backend-init]
    (void)BACKEND_READY;
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0; // CPU only: no assumption about the user's GPU/driver setup
    llama_model* model = llama_model_load_from_file(path, model_params);
    if (model == nullptr) {
        error = L"Modell konnte nicht geladen werden: " + string_to_wstring(path);
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
        error = L"Fuer das Modell konnte kein Kontext erstellt werden.";
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
    contents.push_back(SYSTEM_PROMPT);
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
        return { false, L"Es gibt nichts zu senden." };
    const wstring prompt = build_prompt(history);
    if (prompt.empty())
        return { false, L"Das Modell unterstuetzt kein bekanntes Chat-Format." };

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
        return { false, L"Die Antwort enthielt keinen Text." };
    return { true, string_to_wstring(answer) };
}

ChatReply LlamaClient::analyze_log(const ChatHistory& history, const wstring& log_excerpt)
{
    if (history.empty())
        return { false, L"Es gibt nichts zu senden." };
    if (log_excerpt.empty())
        return { false, L"Die Logdatei ist leer oder konnte nicht gelesen werden." };
    ChatHistory with_excerpt = history;
    with_excerpt.back().text = LOG_ANALYSIS_INSTRUCTION + log_excerpt + L"\n\nFrage: " + with_excerpt.back().text;
    return send(with_excerpt);
}

} // namespace AFS_Core
