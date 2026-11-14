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
#include "HttpClient.h"
#include "SshUnreachable.h"
#include "Synchronize.h"
#include <mutex>
#include <curl/curl.h>

namespace AFS_Core {

namespace http {

namespace {
constexpr long CONNECT_TIMEOUT_SECONDS = 15;
constexpr long STALL_SECONDS = 60;  // [HttpClient.md#timeouts]

struct ThreadHandle {  // [HttpClient.md#connection-reuse]
    CURL* curl;
    ThreadHandle()
    {
        static std::once_flag initialised;
        std::call_once(initialised, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
        curl = curl_easy_init();
    }
    ~ThreadHandle()
    {
        if (curl != nullptr)
            curl_easy_cleanup(curl);
    }
    ThreadHandle(const ThreadHandle&) = delete;
    ThreadHandle& operator=(const ThreadHandle&) = delete;
};

CURL* thread_handle(bool reset)
{
    thread_local ThreadHandle handle;
    if (handle.curl == nullptr)
        throw Error("libcurl could not be initialised");
    if (reset)
        curl_easy_reset(handle.curl);
    return handle.curl;
}

std::size_t append_response(char* data, std::size_t size, std::size_t count, void* response)
{
    static_cast<std::string*>(response)->append(data, size * count);
    return size * count;
}

std::size_t write_file(char* data, std::size_t size, std::size_t count, void* file)
{
    auto* out = static_cast<std::ofstream*>(file);
    return out->write(data, static_cast<std::streamsize>(size * count)) ? size * count : 0;
}

std::size_t read_file(char* buffer, std::size_t size, std::size_t count, void* file)
{
    auto* in = static_cast<std::ifstream*>(file);
    in->read(buffer, static_cast<std::streamsize>(size * count));
    return in->bad() ? CURL_READFUNC_ABORT : static_cast<std::size_t>(in->gcount());
}

int transfer_progress(void*, curl_off_t, curl_off_t downloaded, curl_off_t, curl_off_t uploaded)  // [HttpClient.md#stop]
{
    if (downloaded + uploaded > 0)
        Synchronize::report_copied_bytes(static_cast<uint64_t>(downloaded + uploaded));
    return Synchronize::stopping() ? 1 : 0;
}

bool unreachable(CURLcode code)  // [HttpClient.md#errors]
{
    return code == CURLE_COULDNT_RESOLVE_HOST || code == CURLE_COULDNT_RESOLVE_PROXY || code == CURLE_COULDNT_CONNECT
        || code == CURLE_OPERATION_TIMEDOUT || code == CURLE_SSL_CONNECT_ERROR;
}

CURL* handle_of(void* curl)
{
    return static_cast<CURL*>(curl);
}

void watch_transfer(CURL* curl)
{
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transfer_progress);
}
}

std::string escaped(const std::string& text)
{
    char* encoded = curl_easy_escape(thread_handle(false), text.c_str(), static_cast<int>(text.size()));
    const std::string result = encoded != nullptr ? encoded : "";
    curl_free(encoded);
    return result;
}

std::string unescaped(const std::string& text)
{
    int length = 0;
    char* decoded = curl_easy_unescape(thread_handle(false), text.c_str(), static_cast<int>(text.size()), &length);
    const std::string result = decoded != nullptr ? std::string(decoded, static_cast<std::size_t>(length)) : "";
    curl_free(decoded);
    return result;
}

std::time_t parse_date(const std::string& text)
{
    return curl_getdate(text.c_str(), nullptr);
}

Request::Request(const std::string& url, const char* method)
    : curl_(thread_handle(true)), url_(url)
{
    CURL* curl = handle_of(curl_);
    curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "AutoFileSync");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, STALL_SECONDS);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_);
}

Request::~Request()
{
    curl_slist_free_all(static_cast<curl_slist*>(headers_));
}

void Request::basic_auth(const std::string& user, const std::string& password)
{
    curl_easy_setopt(handle_of(curl_), CURLOPT_USERNAME, user.c_str());
    curl_easy_setopt(handle_of(curl_), CURLOPT_PASSWORD, password.c_str());
    curl_easy_setopt(handle_of(curl_), CURLOPT_HTTPAUTH, static_cast<long>(CURLAUTH_BASIC));
}

void Request::bearer(const std::string& token)
{
    header("Authorization: Bearer " + token);
}

void Request::header(const std::string& line)
{
    headers_ = curl_slist_append(static_cast<curl_slist*>(headers_), line.c_str());
}

void Request::body(const std::string& content)
{
    body_ = content;
    curl_easy_setopt(handle_of(curl_), CURLOPT_POSTFIELDS, body_.c_str());
    curl_easy_setopt(handle_of(curl_), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body_.size()));
}

void Request::download_to(std::ofstream& file)
{
    CURL* curl = handle_of(curl_);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    watch_transfer(curl);
}

void Request::upload_from(std::ifstream& file, std::uintmax_t size)
{
    CURL* curl = handle_of(curl_);
    header("Expect:");  // [HttpClient.md#transfers]
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file);
    curl_easy_setopt(curl, CURLOPT_READDATA, &file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));
    watch_transfer(curl);
}

void Request::on_header(HeaderCallback callback, void* context)
{
    curl_easy_setopt(handle_of(curl_), CURLOPT_HEADERFUNCTION, callback);
    curl_easy_setopt(handle_of(curl_), CURLOPT_HEADERDATA, context);
}

std::optional<long> Request::perform()
{
    CURL* curl = handle_of(curl_);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, static_cast<curl_slist*>(headers_));
    const CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_ABORTED_BY_CALLBACK && Synchronize::stopping())
        return std::nullopt;
    const std::string reason = error_[0] != 0 ? error_ : curl_easy_strerror(code);
    if (unreachable(code))
        throw SshUnreachable(reason);
    if (code != CURLE_OK)
        throw Error(reason);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    return status;
}

std::optional<std::time_t> Request::file_time() const
{
    curl_off_t modified = -1;
    curl_easy_getinfo(handle_of(curl_), CURLINFO_FILETIME_T, &modified);
    return modified > 0 ? std::optional<std::time_t>(static_cast<std::time_t>(modified)) : std::nullopt;
}

void Request::fail(long status) const
{
    throw Error("HTTP " + std::to_string(status) + " " + url_);
}

void Request::expect_success(std::optional<long> status) const
{
    if (status && (*status < OK || *status >= MULTIPLE_CHOICES))
        fail(*status);
}

}

} // namespace AFS_Core
