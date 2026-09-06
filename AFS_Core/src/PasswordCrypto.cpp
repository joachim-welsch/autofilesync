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

const std::string kMarker = "$AES256GCM$";
constexpr size_t kKeyLen = 32;
constexpr size_t kNonceLen = 12;
constexpr size_t kTagLen = 16;

// Generates a random key on first use and stores it next to the config with
// owner-only permissions, like an SSH private key; reused afterward so
// previously encrypted passwords stay decryptable across runs.
//
// Cached in memory per key file path: loadOrCreateKey() re-reads from disk on
// every call, so if persisting a freshly generated key fails (read-only
// filesystem, missing directory), an uncached implementation would hand out a
// *different* random key to every subsequent call - even the very next
// decrypt() of what the preceding encrypt() just produced would fail. The
// cache keeps a run internally consistent; only surviving an actual restart
// still depends on the write below succeeding, which is why that failure is
// logged.
std::mutex keyCacheMutex;
std::map<std::filesystem::path, std::array<unsigned char, kKeyLen>> keyCache;

std::array<unsigned char, kKeyLen> loadOrCreateKey(const std::filesystem::path& keyFile)
{
    std::lock_guard<std::mutex> cacheLock(keyCacheMutex);
    if (auto cached = keyCache.find(keyFile); cached != keyCache.end())
        return cached->second;

    std::array<unsigned char, kKeyLen> key{};
    std::ifstream in(keyFile, std::ios::binary);
    if (in && in.read(reinterpret_cast<char*>(key.data()), key.size()) && in.gcount() == static_cast<std::streamsize>(key.size())) {
        keyCache.emplace(keyFile, key);
        return key;
    }

    if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1)
        LOG_ERROR(L"Konnte keinen kryptographisch sicheren Schluessel erzeugen.")
    std::error_code ec;
    std::filesystem::create_directories(keyFile.parent_path(), ec);

    bool persisted = false;
#ifdef _WIN32
    std::ofstream out(keyFile, std::ios::binary | std::ios::trunc);
    if (out) {
        out.write(reinterpret_cast<const char*>(key.data()), key.size());
        persisted = static_cast<bool>(out);
    }
#else
    // O_EXCL makes the file owner-only (mode 0600) from the moment it exists,
    // avoiding the window a separate chmod() call after creation would leave.
    int fd = open(keyFile.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        persisted = write(fd, key.data(), key.size()) == static_cast<ssize_t>(key.size());
        close(fd);
    }
#endif
    if (!persisted)
        LOG_ERROR2(L"Schluessel fuer verschluesselte Passwoerter konnte nicht gespeichert werden, Passwoerter werden nach einem Neustart unlesbar: ", keyFile.wstring())

    keyCache.emplace(keyFile, key);
    return key;
}

std::string toBase64(const std::vector<unsigned char>& data)
{
    std::vector<unsigned char> out(4 * ((data.size() + 2) / 3) + 1);
    int len = EVP_EncodeBlock(out.data(), data.data(), static_cast<int>(data.size()));
    return std::string(reinterpret_cast<char*>(out.data()), len);
}

std::vector<unsigned char> fromBase64(const std::string& text)
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

std::string encrypt(const std::string& plaintext, const std::filesystem::path& keyFile)
{
    auto key = loadOrCreateKey(keyFile);
    std::array<unsigned char, kNonceLen> nonce{};
    RAND_bytes(nonce.data(), static_cast<int>(nonce.size()));

    std::vector<unsigned char> ciphertext(plaintext.size());
    std::array<unsigned char, kTagLen> tag{};
    int outLen = 0, finalLen = 0;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data());
    EVP_EncryptUpdate(ctx, ciphertext.data(), &outLen, reinterpret_cast<const unsigned char*>(plaintext.data()), static_cast<int>(plaintext.size()));
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + outLen, &finalLen);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen, tag.data());
    EVP_CIPHER_CTX_free(ctx);

    std::vector<unsigned char> blob;
    blob.reserve(kNonceLen + ciphertext.size() + kTagLen);
    blob.insert(blob.end(), nonce.begin(), nonce.end());
    blob.insert(blob.end(), ciphertext.begin(), ciphertext.end());
    blob.insert(blob.end(), tag.begin(), tag.end());
    return kMarker + toBase64(blob);
}

bool isEncrypted(const std::string& stored)
{
    return stored.rfind(kMarker, 0) == 0;
}

std::optional<std::string> decrypt(const std::string& stored, const std::filesystem::path& keyFile)
{
    if (!isEncrypted(stored))
        return std::nullopt; // legacy plaintext, predates this feature

    auto blob = fromBase64(stored.substr(kMarker.size()));
    if (blob.size() < kNonceLen + kTagLen) {
        LOG_ERROR(L"Verschluesseltes Passwort ist beschaedigt.")
        return std::nullopt;
    }
    auto key = loadOrCreateKey(keyFile);
    const unsigned char* nonce = blob.data();
    const unsigned char* ciphertext = blob.data() + kNonceLen;
    const size_t ciphertextLen = blob.size() - kNonceLen - kTagLen;
    unsigned char* tag = blob.data() + kNonceLen + ciphertextLen;

    std::vector<unsigned char> plaintext(ciphertextLen);
    int outLen = 0, finalLen = 0;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce);
    EVP_DecryptUpdate(ctx, plaintext.data(), &outLen, ciphertext, static_cast<int>(ciphertextLen));
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen, tag);
    const int authOk = EVP_DecryptFinal_ex(ctx, plaintext.data() + outLen, &finalLen);
    EVP_CIPHER_CTX_free(ctx);

    if (authOk <= 0) {
        LOG_ERROR(L"Entschluesselung des Passworts fehlgeschlagen (falscher Schluessel oder beschaedigte Daten).")
        return std::nullopt;
    }
    return std::string(reinterpret_cast<char*>(plaintext.data()), outLen + finalLen);
}

} // namespace passwordcrypto

} // namespace AFS_Core
