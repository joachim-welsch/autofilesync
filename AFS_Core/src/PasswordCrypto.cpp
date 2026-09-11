#include "pch.h"
#include "PasswordCrypto.h"
#include "Logging.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <fstream>
#include <array>
#include <vector>
#include <map>
#include <mutex>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace AFS_Core {

namespace passwordcrypto {

namespace {

const std::string MARKER = "$AES256GCM$";
constexpr size_t KEY_LEN = 32;
constexpr size_t NONCE_LEN = 12;
constexpr size_t TAG_LEN = 16;

// Generates a random key on first use and stores it next to the config with
// owner-only permissions, like an SSH private key; reused afterward so
// previously encrypted passwords stay decryptable across runs.
//
// Cached in memory per key file path: load_or_create_key() re-reads from disk on
// every call, so if persisting a freshly generated key fails (read-only
// filesystem, missing directory), an uncached implementation would hand out a
// *different* random key to every subsequent call - even the very next
// decrypt() of what the preceding encrypt() just produced would fail. The
// cache keeps a run internally consistent; only surviving an actual restart
// still depends on the write below succeeding, which is why that failure is
// logged.
std::mutex key_cache_mutex;
std::map<std::filesystem::path, std::array<unsigned char, KEY_LEN>> key_cache;

std::array<unsigned char, KEY_LEN> load_or_create_key(const std::filesystem::path& key_file)
{
    std::lock_guard<std::mutex> cache_lock(key_cache_mutex);
    if (auto cached = key_cache.find(key_file); cached != key_cache.end())
        return cached->second;

    std::array<unsigned char, KEY_LEN> key{};
    std::ifstream in(key_file, std::ios::binary);
    if (in && in.read(reinterpret_cast<char*>(key.data()), key.size()) && in.gcount() == static_cast<std::streamsize>(key.size())) {
        key_cache.emplace(key_file, key);
        return key;
    }

    if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1)
        log_error(L"Konnte keinen kryptographisch sicheren Schluessel erzeugen.");
    std::error_code ec;
    std::filesystem::create_directories(key_file.parent_path(), ec);

    bool persisted = false;
#ifdef _WIN32
    std::ofstream out(key_file, std::ios::binary | std::ios::trunc);
    if (out) {
        out.write(reinterpret_cast<const char*>(key.data()), key.size());
        persisted = static_cast<bool>(out);
    }
#else
    // O_EXCL makes the file owner-only (mode 0600) from the moment it exists,
    // avoiding the window a separate chmod() call after creation would leave.
    int fd = open(key_file.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        persisted = write(fd, key.data(), key.size()) == static_cast<ssize_t>(key.size());
        close(fd);
    }
#endif
    if (!persisted)
        log_error(L"Schluessel fuer verschluesselte Passwoerter konnte nicht gespeichert werden, Passwoerter werden nach einem Neustart unlesbar: ", key_file.wstring());

    key_cache.emplace(key_file, key);
    return key;
}

std::string to_base64(const std::vector<unsigned char>& data)
{
    std::vector<unsigned char> out(4 * ((data.size() + 2) / 3) + 1);
    int len = EVP_EncodeBlock(out.data(), data.data(), static_cast<int>(data.size()));
    return std::string(reinterpret_cast<char*>(out.data()), len);
}

std::vector<unsigned char> from_base64(const std::string& text)
{
    std::vector<unsigned char> out(3 * (text.size() / 4) + 3);
    int len = EVP_DecodeBlock(out.data(), reinterpret_cast<const unsigned char*>(text.data()), static_cast<int>(text.size()));
    if (len < 0)
        return {};
    int padding = 0;
    if (!text.empty() && text.back() == '=')
        ++padding;
    if (text.size() > 1 && text[text.size() - 2] == '=')
        ++padding;
    out.resize(static_cast<size_t>(len) - padding);
    return out;
}

} // namespace

std::string encrypt(const std::string& plaintext, const std::filesystem::path& key_file)
{
    auto key = load_or_create_key(key_file);
    std::array<unsigned char, NONCE_LEN> nonce{};
    RAND_bytes(nonce.data(), static_cast<int>(nonce.size()));

    std::vector<unsigned char> ciphertext(plaintext.size());
    std::array<unsigned char, TAG_LEN> tag{};
    int out_len = 0, final_len = 0;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        log_error(L"Verschluesselung nicht verfuegbar, Passwort wird nicht gespeichert.");
        return {};
    }
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_LEN, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data());
    EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len, reinterpret_cast<const unsigned char*>(plaintext.data()), static_cast<int>(plaintext.size()));
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + out_len, &final_len);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag.data());
    EVP_CIPHER_CTX_free(ctx);

    std::vector<unsigned char> blob;
    blob.reserve(NONCE_LEN + ciphertext.size() + TAG_LEN);
    blob.insert(blob.end(), nonce.begin(), nonce.end());
    blob.insert(blob.end(), ciphertext.begin(), ciphertext.end());
    blob.insert(blob.end(), tag.begin(), tag.end());
    return MARKER + to_base64(blob);
}

bool is_encrypted(const std::string& stored)
{
    return stored.rfind(MARKER, 0) == 0;
}

std::optional<std::string> decrypt(const std::string& stored, const std::filesystem::path& key_file)
{
    if (!is_encrypted(stored))
        return std::nullopt; // legacy plaintext, predates this feature

    auto blob = from_base64(stored.substr(MARKER.size()));
    if (blob.size() < NONCE_LEN + TAG_LEN) {
        log_error(L"Verschluesseltes Passwort ist beschaedigt.");
        return std::nullopt;
    }
    auto key = load_or_create_key(key_file);
    const unsigned char* nonce = blob.data();
    const unsigned char* ciphertext = blob.data() + NONCE_LEN;
    const size_t ciphertext_len = blob.size() - NONCE_LEN - TAG_LEN;
    unsigned char* tag = blob.data() + NONCE_LEN + ciphertext_len;

    std::vector<unsigned char> plaintext(ciphertext_len);
    int out_len = 0, final_len = 0;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        log_error(L"Entschluesselung nicht verfuegbar.");
        return std::nullopt;
    }
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_LEN, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce);
    EVP_DecryptUpdate(ctx, plaintext.data(), &out_len, ciphertext, static_cast<int>(ciphertext_len));
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag);
    const int auth_ok = EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &final_len);
    EVP_CIPHER_CTX_free(ctx);

    if (auth_ok <= 0) {
        log_error(L"Entschluesselung des Passworts fehlgeschlagen (falscher Schluessel oder beschaedigte Daten).");
        return std::nullopt;
    }
    return std::string(reinterpret_cast<char*>(plaintext.data()), out_len + final_len);
}

} // namespace passwordcrypto

} // namespace AFS_Core
