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
#include "DevicePairing.h"
#include <openssl/rand.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#endif

namespace AFS_Core {

bool DevicePairing::open_unlocked() const
{
    return !token_.empty() && std::chrono::steady_clock::now() < deadline_;
}

std::string DevicePairing::begin()  // [DevicePairing.md#begin]
{
    std::lock_guard<std::mutex> lock(mutex_);
    token_ = random_hex(16);
    deadline_ = std::chrono::steady_clock::now() + TOKEN_LIFETIME;
    expires_ = std::time(nullptr) + std::chrono::duration_cast<std::chrono::seconds>(TOKEN_LIFETIME).count();
    failures_ = 0;
    return token_;
}

void DevicePairing::end()
{
    std::lock_guard<std::mutex> lock(mutex_);
    token_.clear();
}

bool DevicePairing::open() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return open_unlocked();
}

bool DevicePairing::redeem(const std::string& token)  // [DevicePairing.md#redeem]
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!open_unlocked())
        return false;
    if (token != token_) {
        if (++failures_ >= FAILURE_LIMIT)
            token_.clear();
        return false;
    }
    token_.clear();
    return true;
}

std::string DevicePairing::token() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return open_unlocked() ? token_ : std::string();
}

std::time_t DevicePairing::expires() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return expires_;
}

std::string random_hex(size_t bytes)
{
    std::vector<unsigned char> buffer(bytes);
    RAND_bytes(buffer.data(), static_cast<int>(buffer.size()));
    static constexpr char HEX[] = "0123456789abcdef";
    std::string hex;
    for (const unsigned char b : buffer) {
        hex += HEX[b >> 4];
        hex += HEX[b & 0xF];
    }
    return hex;
}

namespace {

std::string url_encoded(const std::string& text)
{
    static constexpr char HEX[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char c : text) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            encoded += static_cast<char>(c);
        else
            encoded += std::string("%") + HEX[c >> 4] + HEX[c & 0xF];
    }
    return encoded;
}

std::string joined(const std::vector<std::string>& values)
{
    std::string text;
    for (const auto& value : values)
        text += (text.empty() ? "" : ",") + value;
    return text;
}

} // namespace

std::string qr_content(const PairingOffer& offer)  // [DevicePairing.md#qr_content]
{
    return std::string(PAIRING_URL) + "#v=1&n=" + url_encoded(offer.pc_name) + "&a=" + joined(offer.addresses)
        + "&p=" + std::to_string(offer.port) + "&f=" + offer.fingerprint + "&t=" + offer.token + "&e=" + std::to_string(offer.expires);
}

std::string pc_name()
{
    char name[256] = {};
    if (gethostname(name, sizeof name) != 0 || name[0] == '\0')
        return "PC";
    name[sizeof name - 1] = '\0';
    return name;
}

#ifdef _WIN32

std::vector<std::string> local_ipv4_addresses()  // [DevicePairing.md#local_ipv4_addresses]
{
    std::vector<std::string> addresses;
    ULONG size = 16 * 1024;
    std::vector<unsigned char> buffer(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, adapters, &size) != NO_ERROR)
        return addresses;
    for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            char text[INET_ADDRSTRLEN] = {};
            const auto* in = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
            if (inet_ntop(AF_INET, &in->sin_addr, text, sizeof text))
                addresses.emplace_back(text);
        }
    }
    return addresses;
}

#else

std::vector<std::string> local_ipv4_addresses()  // [DevicePairing.md#local_ipv4_addresses]
{
    std::vector<std::string> addresses;
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0)
        return addresses;
    for (const ifaddrs* entry = list; entry; entry = entry->ifa_next) {
        if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET || (entry->ifa_flags & IFF_LOOPBACK) || !(entry->ifa_flags & IFF_RUNNING))
            continue;  // IFF_RUNNING, not IFF_UP: a docker bridge without a container is up but leads nowhere
        char text[INET_ADDRSTRLEN] = {};
        const auto* in = reinterpret_cast<const sockaddr_in*>(entry->ifa_addr);
        if (inet_ntop(AF_INET, &in->sin_addr, text, sizeof text))
            addresses.emplace_back(text);
    }
    freeifaddrs(list);
    return addresses;
}

#endif

} // namespace AFS_Core
