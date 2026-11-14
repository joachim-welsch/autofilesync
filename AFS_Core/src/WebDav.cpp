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
#include "WebDav.h"
#include "FilesystemUtil.h"
#include "Logging.h"
#include "SshUnreachable.h"
#include "StrUtil.h"
#include "Synchronize.h"
#include "Translation.h"
#include "Util.h"
#include <algorithm>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <curl/curl.h>

namespace AFS_Core {

namespace webdav {

namespace fs = std::filesystem;
namespace pt = boost::property_tree;

namespace {
constexpr long CONNECT_TIMEOUT_SECONDS = 15;
constexpr long STALL_SECONDS = 60;  // [WebDav.md#timeouts]
constexpr long HTTP_OK = 200;
constexpr long HTTP_MULTIPLE_CHOICES = 300;
constexpr long HTTP_MULTI_STATUS = 207;
constexpr long HTTP_MOVED_PERMANENTLY = 301;
constexpr long HTTP_FOUND = 302;
constexpr long HTTP_NOT_FOUND = 404;
constexpr long HTTP_METHOD_NOT_ALLOWED = 405;
constexpr char PROPFIND_BODY[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><D:propfind xmlns:D=\"DAV:\"><D:prop>"
    "<D:resourcetype/><D:getcontentlength/><D:getlastmodified/></D:prop></D:propfind>";
constexpr char MTIME_HEADER[] = "X-OC-Mtime";  // [WebDav.md#modification-time]
constexpr char MTIME_ACCEPTED[] = "accepted";

struct ThreadHandle {  // [WebDav.md#connection-reuse]
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

CURL* thread_handle()
{
    thread_local ThreadHandle handle;
    if (handle.curl == nullptr)
        throw Error("libcurl could not be initialised");
    curl_easy_reset(handle.curl);
    return handle.curl;
}

bool loopback(const std::string& url)
{
    for (const char* host : {"http://localhost", "http://127.0.0.1", "http://[::1]"}) {
        const std::string prefix(host);
        const bool matches = url.starts_with(prefix);
        if (matches && (url.size() == prefix.size() || url[prefix.size()] == ':' || url[prefix.size()] == '/'))
            return true;
    }
    return false;
}

void require_encryption(const std::string& url)  // [WebDav.md#https-only]
{
    if (!address_allowed(url))
        throw Error(util::wstring_to_string(translation::translate("webdav_url_not_https")) + url);
}

std::string escaped(CURL* curl, const std::string& segment)
{
    char* text = curl_easy_escape(curl, segment.c_str(), static_cast<int>(segment.size()));
    const std::string result = text != nullptr ? text : "";
    curl_free(text);
    return result;
}

std::string unescaped(CURL* curl, const std::string& segment)
{
    int length = 0;
    char* text = curl_easy_unescape(curl, segment.c_str(), static_cast<int>(segment.size()), &length);
    const std::string result = text != nullptr ? std::string(text, static_cast<std::size_t>(length)) : "";
    curl_free(text);
    return result;
}

std::string url_of(CURL* curl, const SshAccount& account, const std::string& path, bool collection = false)  // [WebDav.md#urls]
{
    std::string url = account.url();
    while (url.ends_with('/'))
        url.pop_back();
    std::istringstream segments(path);
    for (std::string segment; std::getline(segments, segment, '/');)
        if (!segment.empty())
            url += "/" + escaped(curl, segment);
    return collection ? url + "/" : url;
}

std::size_t append_body(char* data, std::size_t size, std::size_t count, void* body)
{
    static_cast<std::string*>(body)->append(data, size * count);
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

std::size_t note_mtime_accepted(char* data, std::size_t size, std::size_t count, void* accepted)  // [WebDav.md#modification-time]
{
    std::string line(data, size * count);
    std::ranges::transform(line, line.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (line.starts_with("x-oc-mtime:") && line.find(MTIME_ACCEPTED) != std::string::npos)
        *static_cast<bool*>(accepted) = true;
    return size * count;
}

int transfer_progress(void*, curl_off_t, curl_off_t downloaded, curl_off_t, curl_off_t uploaded)  // [WebDav.md#stop]
{
    if (downloaded + uploaded > 0)
        Synchronize::report_copied_bytes(static_cast<uint64_t>(downloaded + uploaded));
    return Synchronize::stopping() ? 1 : 0;
}

bool unreachable(CURLcode code)  // [WebDav.md#errors]
{
    return code == CURLE_COULDNT_RESOLVE_HOST || code == CURLE_COULDNT_RESOLVE_PROXY || code == CURLE_COULDNT_CONNECT
        || code == CURLE_OPERATION_TIMEDOUT || code == CURLE_SSL_CONNECT_ERROR;
}

class Request {  // [WebDav.md#Request]
    CURL* curl_ = thread_handle();
    curl_slist* headers_ = nullptr;
    std::string url_;
    std::string body_;
    char error_[CURL_ERROR_SIZE] = {};
public:
    Request(const SshAccount& account, const std::string& url, const char* method): url_(url)
    {
        require_encryption(account.url());
        curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
        curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, method);
        curl_easy_setopt(curl_, CURLOPT_USERNAME, account.user().c_str());
        curl_easy_setopt(curl_, CURLOPT_PASSWORD, account.password().c_str());
        curl_easy_setopt(curl_, CURLOPT_HTTPAUTH, static_cast<long>(CURLAUTH_BASIC));
        curl_easy_setopt(curl_, CURLOPT_USERAGENT, "AutoFileSync");
        curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SECONDS);
        curl_easy_setopt(curl_, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl_, CURLOPT_LOW_SPEED_TIME, STALL_SECONDS);
        curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, error_);
        curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, append_body);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &body_);
    }
    ~Request()
    {
        curl_slist_free_all(headers_);
    }
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    CURL* curl() const { return curl_; }
    const std::string& body() const { return body_; }
    const std::string& url() const { return url_; }
    void header(const std::string& line)
    {
        headers_ = curl_slist_append(headers_, line.c_str());
    }
    std::optional<long> perform()  // the HTTP status, or nothing when the user stopped the run
    {
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers_);
        const CURLcode code = curl_easy_perform(curl_);
        if (code == CURLE_ABORTED_BY_CALLBACK && Synchronize::stopping())
            return std::nullopt;
        const std::string reason = error_[0] != 0 ? error_ : curl_easy_strerror(code);
        if (unreachable(code))
            throw SshUnreachable(reason);
        if (code != CURLE_OK)
            throw Error(reason);
        long status = 0;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &status);
        return status;
    }
    [[noreturn]] void fail(long status) const
    {
        throw Error("HTTP " + std::to_string(status) + " " + url_);
    }
    void expect_success(std::optional<long> status) const
    {
        if (status && (*status < HTTP_OK || *status >= HTTP_MULTIPLE_CHOICES))
            fail(*status);
    }
};

std::string local_name(const std::string& qualified)  // [WebDav.md#xml-prefixes]
{
    const auto colon = qualified.rfind(':');
    return colon == std::string::npos ? qualified : qualified.substr(colon + 1);
}

std::vector<const pt::ptree*> children(const pt::ptree& node, const char* name)
{
    std::vector<const pt::ptree*> found;
    for (const auto& [key, child] : node)
        if (local_name(key) == name)
            found.push_back(&child);
    return found;
}

const pt::ptree* child(const pt::ptree& node, const char* name)
{
    const auto found = children(node, name);
    return found.empty() ? nullptr : found.front();
}

std::string name_of(CURL* curl, std::string href)  // [WebDav.md#entry-names]
{
    while (href.ends_with('/'))
        href.pop_back();
    return unescaped(curl, href.substr(href.rfind('/') + 1));
}

void read_properties(const pt::ptree& prop, Entry& entry)
{
    if (const pt::ptree* type = child(prop, "resourcetype"))
        entry.is_directory = entry.is_directory || child(*type, "collection") != nullptr;
    if (const pt::ptree* length = child(prop, "getcontentlength"))
        entry.size = length->get_value<uint64_t>(entry.size);
    if (const pt::ptree* modified = child(prop, "getlastmodified")) {
        const std::time_t time = curl_getdate(modified->get_value<std::string>().c_str(), nullptr);
        entry.mtime = time > 0 ? time : entry.mtime;
    }
}

struct Response {
    std::string href;
    Entry entry;
};

std::vector<Response> responses(const Request& request)  // [WebDav.md#multistatus]
{
    pt::ptree document;
    std::istringstream xml(request.body());
    try {
        pt::read_xml(xml, document);
    }
    catch (const pt::xml_parser_error& e) {
        throw Error(std::string("PROPFIND answer is not XML: ") + e.what());
    }
    std::vector<Response> found;
    const pt::ptree* multistatus = child(document, "multistatus");
    for (const pt::ptree* response : multistatus != nullptr ? children(*multistatus, "response") : std::vector<const pt::ptree*>{}) {
        const pt::ptree* href = child(*response, "href");
        if (href == nullptr)
            continue;
        Response item{href->get_value<std::string>(), {}};
        item.entry.name = name_of(request.curl(), item.href);
        for (const pt::ptree* propstat : children(*response, "propstat"))
            if (const pt::ptree* prop = child(*propstat, "prop"))
                read_properties(*prop, item.entry);
        found.push_back(std::move(item));
    }
    return found;
}

std::optional<long> propfind(Request& request, const char* depth)
{
    request.header(std::string("Depth: ") + depth);
    request.header("Content-Type: application/xml; charset=utf-8");
    curl_easy_setopt(request.curl(), CURLOPT_POSTFIELDS, PROPFIND_BODY);
    return request.perform();
}

std::optional<Entry> stat_at(const SshAccount& account, const std::string& path, bool collection)  // [WebDav.md#stat]
{
    Request request(account, url_of(thread_handle(), account, path, collection), "PROPFIND");
    const auto status = propfind(request, "0");
    if (!status || *status == HTTP_NOT_FOUND)
        return std::nullopt;
    if (!collection && (*status == HTTP_MOVED_PERMANENTLY || *status == HTTP_FOUND))
        return stat_at(account, path, true);
    if (*status != HTTP_MULTI_STATUS)
        request.fail(*status);
    const auto found = responses(request);
    if (found.empty())
        return std::nullopt;
    return found.front().entry;
}

std::size_t self_index(const std::vector<Response>& found)  // [WebDav.md#list]
{
    std::size_t shortest = 0;
    for (std::size_t i = 1; i < found.size(); ++i)
        if (found[i].href.size() < found[shortest].href.size())
            shortest = i;
    return shortest;
}

void warn_once_about_time(const SshAccount& account)  // [WebDav.md#modification-time]
{
    static std::mutex mutex;
    static std::set<std::string> warned;
    std::lock_guard<std::mutex> lock(mutex);
    if (warned.insert(account.name()).second)
        log_warning(translation::translate("webdav_time_not_kept"), util::string_to_wstring(account.name()));
}
}

bool address_allowed(const std::string& url)
{
    return url.starts_with("https://") || loopback(url);
}

std::optional<Entry> stat(const SshAccount& account, const std::string& path)
{
    return stat_at(account, path, false);
}

std::vector<Entry> list(const SshAccount& account, const std::string& path)
{
    Request request(account, url_of(thread_handle(), account, path, true), "PROPFIND");
    const auto status = propfind(request, "1");
    if (!status)
        return {};
    if (*status != HTTP_MULTI_STATUS)
        request.fail(*status);
    const auto found = responses(request);
    std::vector<Entry> entries;
    for (std::size_t i = 0, self = self_index(found); i < found.size(); ++i)
        if (i != self && fsu::plain_entry_name(found[i].entry.name))  // [WebDav.md#entry-names]
            entries.push_back(found[i].entry);
    return entries;
}

void make_directory(const SshAccount& account, const std::string& path)
{
    Request request(account, url_of(thread_handle(), account, path, true), "MKCOL");
    const auto status = request.perform();
    if (status && *status == HTTP_METHOD_NOT_ALLOWED) {
        const auto existing = stat(account, path);
        if (existing && existing->is_directory)
            return;
    }
    request.expect_success(status);
}

void remove(const SshAccount& account, const std::string& path)
{
    Request request(account, url_of(thread_handle(), account, path), "DELETE");
    request.expect_success(request.perform());
}

void move(const SshAccount& account, const std::string& from, const std::string& to)
{
    Request request(account, url_of(thread_handle(), account, from), "MOVE");
    request.header("Destination: " + url_of(request.curl(), account, to));
    request.header("Overwrite: F");
    request.expect_success(request.perform());
}

CopyResult download(const SshAccount& account, const std::string& path, const fs::path& local_file)
{
    Request request(account, url_of(thread_handle(), account, path), "GET");
    std::ofstream out(local_file, std::ios::binary);
    if (!out.is_open())
        throw Error(util::wstring_to_string(translation::translate("local_file_open_error")) + local_file.string());
    curl_easy_setopt(request.curl(), CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(request.curl(), CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(request.curl(), CURLOPT_FILETIME, 1L);
    curl_easy_setopt(request.curl(), CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(request.curl(), CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(request.curl(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(request.curl(), CURLOPT_XFERINFOFUNCTION, transfer_progress);
    const auto status = request.perform();
    out.close();
    if (!status)
        return CopyResult::Stopped;
    request.expect_success(status);
    curl_off_t modified = -1;
    curl_easy_getinfo(request.curl(), CURLINFO_FILETIME_T, &modified);
    std::error_code ec;
    if (modified > 0)
        fs::last_write_time(local_file, util::to_file_time(static_cast<std::time_t>(modified)), ec);
    return CopyResult::OK;
}

CopyResult upload(const fs::path& local_file, const SshAccount& account, const std::string& path)
{
    std::error_code ec;
    const auto size = fs::file_size(local_file, ec);
    const auto modified = fs::last_write_time(local_file, ec);
    std::ifstream in(local_file, std::ios::binary);
    if (ec || !in.is_open())
        throw Error(util::wstring_to_string(translation::translate("local_file_open_error")) + local_file.string());
    Request request(account, url_of(thread_handle(), account, path), "PUT");
    bool time_kept = false;
    request.header(std::string(MTIME_HEADER) + ": " + std::to_string(util::to_time_t(modified)));
    request.header("Expect:");  // [WebDav.md#upload]
    curl_easy_setopt(request.curl(), CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(request.curl(), CURLOPT_READFUNCTION, read_file);
    curl_easy_setopt(request.curl(), CURLOPT_READDATA, &in);
    curl_easy_setopt(request.curl(), CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));
    curl_easy_setopt(request.curl(), CURLOPT_HEADERFUNCTION, note_mtime_accepted);
    curl_easy_setopt(request.curl(), CURLOPT_HEADERDATA, &time_kept);
    curl_easy_setopt(request.curl(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(request.curl(), CURLOPT_XFERINFOFUNCTION, transfer_progress);
    const auto status = request.perform();
    if (!status)
        return CopyResult::Stopped;
    request.expect_success(status);
    if (!time_kept)
        warn_once_about_time(account);
    return CopyResult::OK;
}

}

} // namespace AFS_Core
