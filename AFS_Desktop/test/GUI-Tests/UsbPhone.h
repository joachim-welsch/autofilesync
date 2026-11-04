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
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace guitest {

class UsbPhone {  // [UsbPhone.md#UsbPhone]
public:
    UsbPhone();
    ~UsbPhone();
    UsbPhone(const UsbPhone&) = delete;
    UsbPhone& operator=(const UsbPhone&) = delete;
    bool ready() const { return why_not_.empty(); }
    const std::string& why_not() const { return why_not_; }
    bool reverse(uint16_t port);  // [UsbPhone.md#reverse]
    bool forward(uint16_t port);  // [UsbPhone.md#forward]
    bool open_link(const std::string& url);  // [UsbPhone.md#open_link]
    bool tap(const std::string& string_name, std::chrono::seconds timeout);  // [UsbPhone.md#tap]
    bool write_file(const std::string& path, const std::string& content);
    std::optional<std::string> read_file(const std::string& path);
    void remove_dir(const std::string& path);
private:
    std::string check();
    bool unlocked();
    bool backup_pairing();
    bool restore_pairing();  // [UsbPhone.md#pairing-backup]
    std::string adb(const std::string& arguments) const;
    std::string adb(const std::string& service, const std::string& phone_command) const;
    std::string shell(const std::string& phone_command, int* status = nullptr);
    std::string why_not_;
    std::string serial_;
    std::optional<std::string> pairing_;
    bool backed_up_ = false;
    std::vector<std::string> undo_;
};

std::string app_package();

}
