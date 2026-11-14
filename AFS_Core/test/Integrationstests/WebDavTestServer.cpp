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
#include <curl/curl.h>
#ifndef _WIN32
#include <csignal>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tu {

void visit_like_a_browser(const std::string& url)  // [WebDavTestServer.md#visit_like_a_browser]
{
    CURL* browser = curl_easy_init();
    curl_easy_setopt(browser, CURLOPT_URL, url.c_str());
    curl_easy_setopt(browser, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(browser, CURLOPT_WRITEFUNCTION, +[](char*, std::size_t size, std::size_t count, void*) { return size * count; });
    curl_easy_perform(browser);
    curl_easy_cleanup(browser);
}

WebDavTestServer::WebDavTestServer(const std::filesystem::path& root, bool keep_mtime)
    : PythonTestServer("webdavTestServer.py", keep_mtime ? std::vector<std::string>{root.string(), "--user", USER, "--password", PASSWORD, "--keep-mtime"}
                                                           : std::vector<std::string>{root.string(), "--user", USER, "--password", PASSWORD})
{}

GoogleDriveTestServer::GoogleDriveTestServer(int page_size)
    : PythonTestServer("googleDriveTestServer.py", {"--page-size", std::to_string(page_size)})
{}

#ifdef _WIN32
PythonTestServer::PythonTestServer(const std::string&, const std::vector<std::string>&)  // [WebDavTestServer.md#windows]
{}

PythonTestServer::~PythonTestServer() = default;
#else
namespace {
std::string script(const std::string& name)
{
    return (std::filesystem::path(__FILE__).parent_path().parent_path() / "tools" / name).string();  // [WebDavTestServer.md#script]
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

PythonTestServer::PythonTestServer(const std::string& script_name, const std::vector<std::string>& arguments)
{
    int output[2];
    if (pipe(output) != 0)
        return;
    std::vector<std::string> words = {"python3", script(script_name)};
    words.insert(words.end(), arguments.begin(), arguments.end());
    std::vector<char*> command;
    for (std::string& word : words)
        command.push_back(word.data());
    command.push_back(nullptr);
    process_ = fork();
    if (process_ == 0) {
        dup2(output[1], STDOUT_FILENO);
        close(output[0]);
        close(output[1]);
        execvp("python3", command.data());
        _exit(127);
    }
    close(output[1]);
    port_ = process_ > 0 ? read_port(output[0]) : 0;
}

PythonTestServer::~PythonTestServer()
{
    if (process_ <= 0)
        return;
    kill(process_, SIGTERM);
    waitpid(process_, nullptr, 0);
}
#endif

} // namespace tu
