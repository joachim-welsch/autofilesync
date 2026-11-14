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
#include "HttpClient.h"
#include "FilesystemUtil.h"
#include "Logging.h"
#include "SshUnreachable.h"
#include "StrUtil.h"
#include "Synchronize.h"
#include "Translation.h"
#include "Util.h"
#include <algorithm>
#include <fstream>
#include <cctype>
#include <mutex>
#include <set>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace AFS_Core {

namespace webdav {

namespace fs = std::filesystem;
namespace pt = boost::property_tree;

namespace {
constexpr long HTTP_MULTI_STATUS = 207;
constexpr long HTTP_MOVED_PERMANENTLY = 301;
constexpr long HTTP_FOUND = 302;
constexpr long HTTP_METHOD_NOT_ALLOWED = 405;
constexpr char PROPFIND_BODY[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><D:propfind xmlns:D=\"DAV:\"><D:prop>"
    "<D:resourcetype/><D:getcontentlength/><D:getlastmodified/></D:prop></D:propfind>";
constexpr char MTIME_HEADER[] = "X-OC-Mtime";  // [WebDav.md#modification-time]
constexpr char MTIME_ACCEPTED[] = "accepted";

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

std::string url_of(const SshAccount& account, const std::string& path, bool collection = false)  // [WebDav.md#urls]
{
    std::string url = account.url();
    while (url.ends_with('/'))
        url.pop_back();
    std::istringstream segments(path);
    for (std::string segment; std::getline(segments, segment, '/');)
        if (!segment.empty())
            url += "/" + http::escaped(segment);
    return collection ? url + "/" : url;
}

std::size_t note_mtime_accepted(char* data, std::size_t size, std::size_t count, void* accepted)  // [WebDav.md#modification-time]
{
    std::string line(data, size * count);
    std::ranges::transform(line, line.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (line.starts_with("x-oc-mtime:") && line.find(MTIME_ACCEPTED) != std::string::npos)
        *static_cast<bool*>(accepted) = true;
    return size * count;
}

class Request : public http::Request {  // [WebDav.md#Request]
public:
    Request(const SshAccount& account, const std::string& url, const char* method): http::Request(url, method)
    {
        require_encryption(account.url());
        basic_auth(account.user(), account.password());
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

std::string name_of(std::string href)  // [WebDav.md#entry-names]
{
    while (href.ends_with('/'))
        href.pop_back();
    return http::unescaped(href.substr(href.rfind('/') + 1));
}

void read_properties(const pt::ptree& prop, Entry& entry)
{
    if (const pt::ptree* type = child(prop, "resourcetype"))
        entry.is_directory = entry.is_directory || child(*type, "collection") != nullptr;
    if (const pt::ptree* length = child(prop, "getcontentlength"))
        entry.size = length->get_value<uint64_t>(entry.size);
    if (const pt::ptree* modified = child(prop, "getlastmodified")) {
        const std::time_t time = http::parse_date(modified->get_value<std::string>());
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
    std::istringstream xml(request.response());
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
        item.entry.name = name_of(item.href);
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
    request.body(PROPFIND_BODY);
    return request.perform();
}

std::optional<Entry> stat_at(const SshAccount& account, const std::string& path, bool collection)  // [WebDav.md#stat]
{
    Request request(account, url_of(account, path, collection), "PROPFIND");
    const auto status = propfind(request, "0");
    if (!status || *status == http::NOT_FOUND)
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
    Request request(account, url_of(account, path, true), "PROPFIND");
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
    Request request(account, url_of(account, path, true), "MKCOL");
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
    Request request(account, url_of(account, path), "DELETE");
    request.expect_success(request.perform());
}

void move(const SshAccount& account, const std::string& from, const std::string& to)
{
    Request request(account, url_of(account, from), "MOVE");
    request.header("Destination: " + url_of(account, to));
    request.header("Overwrite: F");
    request.expect_success(request.perform());
}

CopyResult download(const SshAccount& account, const std::string& path, const fs::path& local_file)
{
    Request request(account, url_of(account, path), "GET");
    std::ofstream out(local_file, std::ios::binary);
    if (!out.is_open())
        throw Error(util::wstring_to_string(translation::translate("local_file_open_error")) + local_file.string());
    request.download_to(out);
    const auto status = request.perform();
    out.close();
    if (!status)
        return CopyResult::Stopped;
    request.expect_success(status);
    std::error_code ec;
    if (const auto modified = request.file_time())
        fs::last_write_time(local_file, util::to_file_time(*modified), ec);
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
    Request request(account, url_of(account, path), "PUT");
    bool time_kept = false;
    request.header(std::string(MTIME_HEADER) + ": " + std::to_string(util::to_time_t(modified)));
    request.upload_from(in, size);
    request.on_header(note_mtime_accepted, &time_kept);
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
