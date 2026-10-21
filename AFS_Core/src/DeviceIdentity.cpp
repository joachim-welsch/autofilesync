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
#include "DeviceIdentity.h"
#include "Logging.h"
#include "Translation.h"
#include "Util.h"
#include <fstream>
#include <functional>
#include <memory>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

namespace AFS_Core {

namespace {

constexpr const char* KEY_FILE = "device.key";
constexpr const char* CERTIFICATE_FILE = "device.crt";
constexpr long VALID_DAYS = 20 * 365;  // [DeviceIdentity.md#validity]

using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using CertificatePtr = std::unique_ptr<X509, decltype(&X509_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

std::string read_text(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool write_private_text(const std::filesystem::path& file, const std::string& text)  // [DeviceIdentity.md#private-files]
{
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
    if (!out)
        return false;
    out.close();
#ifndef _WIN32
    std::error_code ec;
    std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, ec);
#endif
    return true;
}

std::string pem_of(const std::function<int(BIO*)>& write)
{
    BioPtr bio(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio || write(bio.get()) != 1)
        return {};
    char* data = nullptr;
    const long length = BIO_get_mem_data(bio.get(), &data);
    return length > 0 ? std::string(data, static_cast<size_t>(length)) : std::string();
}

CertificatePtr certificate_from_pem(const std::string& pem)
{
    BioPtr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
    return CertificatePtr(bio ? PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr) : nullptr, X509_free);
}

KeyPtr key_from_pem(const std::string& pem)
{
    BioPtr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
    return KeyPtr(bio ? PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr) : nullptr, EVP_PKEY_free);
}

std::string host_name()
{
    char name[256] = {};
    if (gethostname(name, sizeof name) != 0 || name[0] == '\0')
        return "AutoFileSync";
    name[sizeof name - 1] = '\0';
    return name;
}

CertificatePtr self_signed(EVP_PKEY* key)  // [DeviceIdentity.md#self-signed]
{
    CertificatePtr certificate(X509_new(), X509_free);
    if (!certificate)
        return certificate;
    unsigned char serial[16];
    RAND_bytes(serial, sizeof serial);
    serial[0] &= 0x7F;
    BIGNUM* serial_number = BN_bin2bn(serial, sizeof serial, nullptr);
    BN_to_ASN1_INTEGER(serial_number, X509_get_serialNumber(certificate.get()));
    BN_free(serial_number);
    X509_set_version(certificate.get(), X509_VERSION_3);
    X509_gmtime_adj(X509_getm_notBefore(certificate.get()), -86400L);  // a phone whose clock runs a little behind must still accept it
    X509_gmtime_adj(X509_getm_notAfter(certificate.get()), VALID_DAYS * 86400L);
    X509_set_pubkey(certificate.get(), key);
    const std::string common_name = "AutoFileSync " + host_name();
    X509_NAME* name = X509_get_subject_name(certificate.get());
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(common_name.c_str()), -1, -1, 0);
    X509_set_issuer_name(certificate.get(), name);
    if (X509_sign(certificate.get(), key, EVP_sha256()) <= 0)
        certificate.reset();
    return certificate;
}

} // namespace

std::string certificate_fingerprint(X509* certificate)  // [DeviceIdentity.md#certificate_fingerprint]
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (!certificate || X509_digest(certificate, EVP_sha256(), digest, &length) != 1)
        return {};
    static constexpr char HEX[] = "0123456789abcdef";
    std::string hex;
    for (unsigned int i = 0; i < length; ++i) {
        hex += HEX[digest[i] >> 4];
        hex += HEX[digest[i] & 0xF];
    }
    return hex;
}

std::optional<DeviceIdentity> DeviceIdentity::load_or_create(const std::filesystem::path& dir)  // [DeviceIdentity.md#load_or_create]
{
    DeviceIdentity identity;
    identity.key_pem_ = read_text(dir / KEY_FILE);
    identity.certificate_pem_ = read_text(dir / CERTIFICATE_FILE);
    CertificatePtr certificate = certificate_from_pem(identity.certificate_pem_);
    if (!certificate || !key_from_pem(identity.key_pem_)) {
        const KeyPtr key(EVP_EC_gen("P-256"), EVP_PKEY_free);
        certificate = key ? self_signed(key.get()) : CertificatePtr(nullptr, X509_free);
        if (!certificate) {
            log_error(translation::translate("device_identity_failed"), dir.wstring());
            return std::nullopt;
        }
        identity.key_pem_ = pem_of([&](BIO* bio) { return PEM_write_bio_PrivateKey(bio, key.get(), nullptr, nullptr, 0, nullptr, nullptr); });
        identity.certificate_pem_ = pem_of([&](BIO* bio) { return PEM_write_bio_X509(bio, certificate.get()); });
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (!write_private_text(dir / KEY_FILE, identity.key_pem_) || !write_private_text(dir / CERTIFICATE_FILE, identity.certificate_pem_))
            log_error(translation::translate("device_identity_not_saved"), dir.wstring());  // [DeviceIdentity.md#unsaved-identity]
    }
    identity.fingerprint_ = certificate_fingerprint(certificate.get());
    return identity;
}

} // namespace AFS_Core
