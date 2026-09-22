/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#include "License.h"
#include "Logging.h"
#include "Translation.h"
#include "Util.h"
#include <openssl/evp.h>
#include <cstdio>
#include <fstream>
#include <memory>

namespace AFS_Core::license {

namespace {

constexpr std::string_view LICENSEE_FIELD = "Licensee: ";
constexpr std::string_view SIGNATURE_FIELD = "Signature: ";

using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using ContextPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

std::vector<unsigned char> from_hex(std::string_view hex)
{
    std::vector<unsigned char> bytes;
    if (hex.size() % 2 != 0)
        return bytes;
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        unsigned int value = 0;
        if (std::sscanf(std::string(hex.substr(i, 2)).c_str(), "%2x", &value) != 1)
            return {};
        bytes.push_back(static_cast<unsigned char>(value));
    }
    return bytes;
}

std::string to_hex(std::span<const unsigned char> bytes)
{
    static constexpr char DIGITS[] = "0123456789abcdef";
    std::string hex;
    for (const unsigned char byte : bytes) {
        hex += DIGITS[byte >> 4];
        hex += DIGITS[byte & 0x0f];
    }
    return hex;
}

std::string without_line_break(std::string line)
{
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();
    return line;
}

struct Parsed {  // [License.md#format]
    std::string payload;
    std::string licensee;
    std::string signature_hex;
};

std::optional<Parsed> parse(std::istream& in)
{
    Parsed parsed;
    for (std::string line; std::getline(in, line);) {
        const std::string text = without_line_break(line);
        if (text.starts_with(SIGNATURE_FIELD)) {
            parsed.signature_hex = text.substr(SIGNATURE_FIELD.size());
            return parsed;
        }
        parsed.payload += text + '\n';
        if (text.starts_with(LICENSEE_FIELD))
            parsed.licensee = text.substr(LICENSEE_FIELD.size());
    }
    return std::nullopt;
}

std::span<const unsigned char> bytes_of(const std::string& text)
{
    return std::span(reinterpret_cast<const unsigned char*>(text.data()), text.size());
}

}

bool verify(std::span<const unsigned char> payload, std::span<const unsigned char> signature, PublicKey public_key)
{
    if (signature.size() != SIGNATURE_SIZE)
        return false;
    const KeyPtr key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, public_key.data(), public_key.size()), EVP_PKEY_free);
    const ContextPtr context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (key == nullptr || context == nullptr || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1)
        return false;
    return EVP_DigestVerify(context.get(), signature.data(), signature.size(), payload.data(), payload.size()) == 1;
}

std::optional<License> read(const std::filesystem::path& file, PublicKey public_key)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return std::nullopt;
    const std::optional<Parsed> parsed = parse(in);
    const bool valid = parsed  // [License.md#private-use]
        && verify(bytes_of(parsed->payload), from_hex(parsed->signature_hex), public_key);
    if (!valid) {
        log_warning(translation::translate("license_file_invalid"), file.wstring());  // [License.md#read]
        return std::nullopt;
    }
    return License{util::string_to_wstring(parsed->licensee)};
}

std::optional<License> find(const std::vector<std::filesystem::path>& directories, PublicKey public_key)
{
    for (const std::filesystem::path& directory : directories) {
        const std::filesystem::path file = directory / LICENSE_FILE_NAME;
        std::error_code ec;
        if (std::filesystem::is_regular_file(file, ec))
            return read(file, public_key);  // [License.md#find]
    }
    return std::nullopt;
}

std::string sign_for_test(std::span<const unsigned char> payload, std::array<unsigned char, PUBLIC_KEY_SIZE>& public_key)  // [License.md#sign_for_test]
{
    const KeyPtr key(EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519"), EVP_PKEY_free);
    const ContextPtr context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    std::size_t key_size = public_key.size();
    if (key == nullptr || context == nullptr || EVP_PKEY_get_raw_public_key(key.get(), public_key.data(), &key_size) != 1)
        return {};
    std::array<unsigned char, SIGNATURE_SIZE> signature{};
    std::size_t signature_size = signature.size();
    if (EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1
        || EVP_DigestSign(context.get(), signature.data(), &signature_size, payload.data(), payload.size()) != 1)
        return {};
    return to_hex(signature);
}

} // namespace AFS_Core::license
