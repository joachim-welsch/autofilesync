/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
#include "FirewallHint.h"
#include <algorithm>
#include <bit>
#include <fstream>
#ifndef _WIN32
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

namespace {

std::string setting(const std::filesystem::path& file, const std::string& key)  // [FirewallHint.md#setting]
{
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.starts_with(key + "="))
            continue;
        std::string value = line.substr(key.size() + 1);
        std::erase(value, '"');
        return value;
    }
    return {};
}

}

bool ufw_blocks_incoming(const std::filesystem::path& etc)  // [FirewallHint.md#ufw_blocks_incoming]
{
    if (setting(etc / "ufw" / "ufw.conf", "ENABLED") != "yes")
        return false;
    return setting(etc / "default" / "ufw", "DEFAULT_INPUT_POLICY") != "ACCEPT";
}

#ifdef _WIN32

std::vector<std::string> local_ipv4_networks()  // [FirewallHint.md#local_ipv4_networks]
{
    return {};
}

#else

std::vector<std::string> local_ipv4_networks()  // [FirewallHint.md#local_ipv4_networks]
{
    std::vector<std::string> networks;
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0)
        return networks;
    for (const ifaddrs* entry = list; entry; entry = entry->ifa_next) {
        if (!entry->ifa_addr || !entry->ifa_netmask || entry->ifa_addr->sa_family != AF_INET || (entry->ifa_flags & IFF_LOOPBACK) || !(entry->ifa_flags & IFF_RUNNING))
            continue;  // the same interfaces as local_ipv4_addresses [DevicePairing.md#local_ipv4_addresses]
        const uint32_t mask = ntohl(reinterpret_cast<const sockaddr_in*>(entry->ifa_netmask)->sin_addr.s_addr);
        in_addr network{htonl(ntohl(reinterpret_cast<const sockaddr_in*>(entry->ifa_addr)->sin_addr.s_addr) & mask)};
        char text[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &network, text, sizeof text))
            networks.emplace_back(std::string(text) + "/" + std::to_string(std::popcount(mask)));
    }
    freeifaddrs(list);
    std::ranges::sort(networks);
    networks.erase(std::ranges::unique(networks).begin(), networks.end());
    return networks;
}

#endif

std::string ufw_allow_command(const std::vector<std::string>& networks, uint16_t port)  // [FirewallHint.md#ufw_allow_command]
{
    const std::string port_text = std::to_string(port);
    if (networks.empty())
        return "sudo ufw allow " + port_text;
    std::string command;
    for (const auto& network : networks)
        command += (command.empty() ? "" : " && ") + std::string("sudo ufw allow from ") + network + " to any port " + port_text;
    return command;
}
