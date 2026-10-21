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
#pragma once
#include "Device.h"
#include "DeviceIdentity.h"
#include "DevicePairing.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

struct ssl_ctx_st;

namespace AFS_Core {

struct PairRequest {  // [DeviceLink.md#protocol]
    std::string token;
    Device device;
    uint16_t sftp_port = 0;
    std::string app_version;
};

struct HelloRequest {
    std::string device_id;
    uint16_t sftp_port = 0;
    std::string sftp_user;
    std::string sftp_password;
    bool serving = false;  // the phone has its SFTP server open and asks for a pass [DeviceLink.md#serve]
    std::string app_version;
};

struct DeviceItem {  // [DeviceLink.md#items]
    std::wstring name;
    std::string direction;  // toPc, toPhone or both
    bool enabled = true;
};

class DeviceLink {  // [DeviceLink.md#DeviceLink]
public:
    static constexpr uint16_t DEFAULT_PORT = 52381;  // [DeviceLink.md#port]
    static DeviceLink& instance();
    ~DeviceLink();
    DeviceLink(const DeviceLink&) = delete;
    DeviceLink& operator=(const DeviceLink&) = delete;
    bool start(const std::filesystem::path& identity_dir, uint16_t port = DEFAULT_PORT);  // [DeviceLink.md#start]
    void stop();
    bool running() const { return running_; }
    uint16_t port() const { return port_; }
    std::string fingerprint() const;
    DevicePairing& pairing() { return pairing_; }
    PairingOffer offer();  // [DeviceLink.md#offer]
    std::optional<Device> take_paired();  // [DeviceLink.md#take_paired]
    std::optional<DevicePresence> presence(const std::string& device_id) const;
    bool serving(const std::string& device_id) const;  // [Device.md#serving]
    void item_passed(const std::string& device_id, const std::wstring& item_name);  // [Synchronize.md#device-items]
    static std::vector<DeviceItem> items_of(const std::string& device_id);  // [DeviceLink.md#items]
    static void use_private_scope();  // [DeviceLink.md#use_private_scope]
private:
    using Send = std::function<bool(const std::string& line)>;
    DeviceLink() = default;
    bool take_role();
    void release_role();
    bool listen_on(uint16_t port);
    bool prepare_tls();
    void accept_loop();
    void session(std::intptr_t client, const std::string& peer_address);
    void handle(const std::string& line, const std::string& peer_fingerprint, const std::string& peer_address, const Send& send);
    std::string pair(const PairRequest& request, const std::string& peer_fingerprint, const std::string& peer_address);
    void hello(const HelloRequest& request, const std::string& peer_fingerprint, const std::string& peer_address, const Send& send);
    void serve(const Device& device, const HelloRequest& request, const Send& send);  // [DeviceLink.md#serve]
    bool wait_for_pass(const std::string& device_id, const std::vector<DeviceItem>& items, const Send& send);

    std::optional<DeviceIdentity> identity_;
    ssl_ctx_st* ctx_ = nullptr;
    std::intptr_t listener_ = -1;
    std::intptr_t role_handle_ = -1;
    uint16_t port_ = 0;
    std::atomic_bool running_{false};
    std::atomic_bool stop_{false};
    std::thread thread_;
    DevicePairing pairing_;
    mutable std::mutex mutex_;
    std::condition_variable passed_;  // [DeviceLink.md#serve]
    std::set<std::wstring> pending_items_;
    std::optional<Device> paired_;
    std::map<std::string, DevicePresence> presence_;
};

} // namespace AFS_Core
