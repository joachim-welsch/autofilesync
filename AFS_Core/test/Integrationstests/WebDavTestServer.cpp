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
#ifdef _WIN32
#include <array>
#include <cstdlib>
#include <windows.h>
#else
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

namespace {
std::vector<std::string> webdav_arguments(const std::filesystem::path& root, bool keep_mtime, bool nextcloud_layout)
{
    std::vector<std::string> arguments = {root.string(), "--user", WebDavTestServer::USER, "--password", WebDavTestServer::PASSWORD};
    if (keep_mtime)
        arguments.push_back("--keep-mtime");
    if (nextcloud_layout)
        arguments.insert(arguments.end(), {"--prefix", WebDavTestServer::NEXTCLOUD_PREFIX});
    return arguments;
}
}

WebDavTestServer::WebDavTestServer(const std::filesystem::path& root, bool keep_mtime, bool nextcloud_layout)
    : PythonTestServer("webdavTestServer.py", webdav_arguments(root, keep_mtime, nextcloud_layout))
{}

GoogleDriveTestServer::GoogleDriveTestServer(int page_size)
    : PythonTestServer("googleDriveTestServer.py", {"--page-size", std::to_string(page_size)})
{}

DropboxTestServer::DropboxTestServer(int page_size)
    : PythonTestServer("dropboxTestServer.py", {"--page-size", std::to_string(page_size)})
{}

S3TestServer::S3TestServer(int page_size)
    : PythonTestServer("s3TestServer.py", {"--page-size", std::to_string(page_size)})
{}

OneDriveTestServer::OneDriveTestServer(int page_size)
    : PythonTestServer("oneDriveTestServer.py", {"--page-size", std::to_string(page_size)})
{}

namespace {
std::string script(const std::string& name)
{
    return (std::filesystem::path(__FILE__).parent_path().parent_path() / "tools" / name).string();  // [WebDavTestServer.md#script]
}
}

#ifdef _WIN32
namespace {
const std::vector<std::vector<std::string>> PYTHONS = {{"py", "-3"}, {"python"}, {"python3"}};  // [WebDavTestServer.md#windows-python]

struct Launched {
    HANDLE process = nullptr;
    HANDLE job = nullptr;
    int port = 0;
};

std::string argv_quoted(const std::string& argument)  // [WebDavTestServer.md#windows-quoting]
{
    if (!argument.empty() && argument.find_first_of(" \t\"") == std::string::npos)
        return argument;
    std::string quoted = "\"";
    std::size_t backslashes = 0;
    for (const char c : argument) {
        if (c == '\\') {
            ++backslashes;
            continue;
        }
        quoted.append(c == '"' ? 2 * backslashes + 1 : backslashes, '\\');
        quoted += c;
        backslashes = 0;
    }
    return quoted.append(2 * backslashes, '\\') + "\"";
}

std::wstring command_line(const std::vector<std::string>& words)
{
    std::string line;
    for (const std::string& word : words)
        line += (line.empty() ? "" : " ") + argv_quoted(word);
    const int size = MultiByteToWideChar(CP_ACP, 0, line.data(), static_cast<int>(line.size()), nullptr, 0);
    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_ACP, 0, line.data(), static_cast<int>(line.size()), wide.data(), size);
    return wide;
}

HANDLE job_killing_on_close()  // [WebDavTestServer.md#windows-job]
{
    const HANDLE job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job != nullptr)
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    return job;
}

HANDLE started_in(HANDLE job, std::wstring line, HANDLE nothing, HANDLE output)  // [WebDavTestServer.md#windows-handles]
{
    std::array<HANDLE, 2> inherited{nothing, output};
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<char> buffer(size);
    const auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &size))
        return nullptr;
    UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited.data(), sizeof(HANDLE) * inherited.size(), nullptr, nullptr);
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nothing;
    startup.StartupInfo.hStdOutput = output;
    startup.StartupInfo.hStdError = nothing;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    const DWORD flags = CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT;
    const bool created = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, flags, nullptr, nullptr, &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(attributes);
    if (!created)
        return nullptr;
    AssignProcessToJobObject(job, process.hProcess);
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    return process.hProcess;
}

int read_port(HANDLE output)  // [WebDavTestServer.md#port]
{
    std::string line;
    char c = 0;
    for (DWORD n = 0; ReadFile(output, &c, 1, &n, nullptr) && n == 1 && c != '\n';)
        line += c;
    return std::atoi(line.c_str());
}

void stop(const Launched& server)
{
    if (server.job != nullptr)
        CloseHandle(server.job);
    if (server.process == nullptr)
        return;
    WaitForSingleObject(server.process, INFINITE);  // [WebDavTestServer.md#windows-job]
    CloseHandle(server.process);
}

Launched launched(const std::vector<std::string>& words)
{
    SECURITY_ATTRIBUTES inheritable{sizeof(inheritable), nullptr, TRUE};
    HANDLE read = nullptr;
    HANDLE write = nullptr;
    if (!CreatePipe(&read, &write, &inheritable, 0))
        return {};
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    const HANDLE nothing = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, 0, nullptr);
    Launched server;
    server.job = job_killing_on_close();
    server.process = started_in(server.job, command_line(words), nothing, write);
    CloseHandle(write);  // the read below ends only when the child holds the last write end
    CloseHandle(nothing);
    server.port = server.process != nullptr ? read_port(read) : 0;
    CloseHandle(read);
    if (server.port > 0)
        return server;
    stop(server);
    return {};
}
}

PythonTestServer::PythonTestServer(const std::string& script_name, const std::vector<std::string>& arguments)
{
    for (std::vector<std::string> words : PYTHONS) {
        words.push_back(script(script_name));
        words.insert(words.end(), arguments.begin(), arguments.end());
        const Launched server = launched(words);
        if (server.port == 0)
            continue;
        process_ = server.process;
        job_ = server.job;
        port_ = server.port;
        return;
    }
}

PythonTestServer::~PythonTestServer()
{
    stop({process_, job_, port_});
}
#else
namespace {
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
