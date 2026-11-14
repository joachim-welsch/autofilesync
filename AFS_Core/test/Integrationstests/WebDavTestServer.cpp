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
#include "WebDavTestServer.h"
#ifndef _WIN32
#include <csignal>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tu {

#ifdef _WIN32
WebDavTestServer::WebDavTestServer(const std::filesystem::path&, bool)  // [WebDavTestServer.md#windows]
{}

WebDavTestServer::~WebDavTestServer() = default;
#else
namespace {
std::string script()
{
    return (std::filesystem::path(__FILE__).parent_path().parent_path() / "tools" / "webdavTestServer.py").string();  // [WebDavTestServer.md#script]
}

int read_port(int descriptor)  // [WebDavTestServer.md#port]
{
    FILE* output = fdopen(descriptor, "r");
    int port = 0;
    if (output == nullptr || fscanf(output, "%d", &port) != 1)
        port = 0;
    if (output != nullptr)
        fclose(output);
    return port;
}
}

WebDavTestServer::WebDavTestServer(const std::filesystem::path& root, bool keep_mtime)
{
    int output[2];
    if (pipe(output) != 0)
        return;
    const std::string script_path = script();
    const std::string root_path = root.string();
    process_ = fork();
    if (process_ == 0) {
        dup2(output[1], STDOUT_FILENO);
        close(output[0]);
        close(output[1]);
        execlp("python3", "python3", script_path.c_str(), root_path.c_str(), "--user", USER, "--password", PASSWORD,
            keep_mtime ? "--keep-mtime" : static_cast<char*>(nullptr), static_cast<char*>(nullptr));
        _exit(127);
    }
    close(output[1]);
    port_ = process_ > 0 ? read_port(output[0]) : 0;
}

WebDavTestServer::~WebDavTestServer()
{
    if (process_ <= 0)
        return;
    kill(process_, SIGTERM);
    waitpid(process_, nullptr, 0);
}
#endif

} // namespace tu
