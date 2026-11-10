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
#include "HostConnect.h"
#ifdef _WIN32
#include <ws2tcpip.h>

namespace AFS_Core {

namespace {

SOCKET connected(const addrinfo& address)
{
    const SOCKET candidate = socket(address.ai_family, address.ai_socktype, address.ai_protocol);
    if (candidate == INVALID_SOCKET)
        return INVALID_SOCKET;
    if (connect(candidate, address.ai_addr, static_cast<int>(address.ai_addrlen)) == 0)
        return candidate;
    closesocket(candidate);
    return INVALID_SOCKET;
}

SOCKET first_connected(const addrinfo* addresses, int family)
{
    for (const addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        if (address->ai_family != family)
            continue;
        if (const SOCKET candidate = connected(*address); candidate != INVALID_SOCKET)
            return candidate;
    }
    return INVALID_SOCKET;
}

} // namespace

HostConnection connect_host(const std::string& host, uint16_t port)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addresses) != 0 || addresses == nullptr)
        return {INVALID_SOCKET, ConnectFailure::Resolve};
    SOCKET found = first_connected(addresses, AF_INET);  // [HostConnect.md#ipv4-first]
    if (found == INVALID_SOCKET)
        found = first_connected(addresses, AF_INET6);
    freeaddrinfo(addresses);
    return {found, found == INVALID_SOCKET ? ConnectFailure::Connect : ConnectFailure::None};
}

} // namespace AFS_Core
#endif
