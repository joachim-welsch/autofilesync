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
#include <filesystem>
#include <string>

namespace tu {

class WebDavTestServer {  // [WebDavTestServer.md#WebDavTestServer]
public:
    static constexpr char USER[] = "anna";
    static constexpr char PASSWORD[] = "geheim";
    WebDavTestServer(const std::filesystem::path& root, bool keep_mtime);
    ~WebDavTestServer();
    WebDavTestServer(const WebDavTestServer&) = delete;
    WebDavTestServer& operator=(const WebDavTestServer&) = delete;
    bool running() const { return port_ > 0; }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }
private:
    int process_ = -1;
    int port_ = 0;
};

} // namespace tu
