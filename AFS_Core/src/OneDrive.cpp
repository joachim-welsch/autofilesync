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
#include "OneDrive.h"
#include "FilesystemUtil.h"
#include "GoogleAuth.h"
#include "HttpClient.h"
#include "PercentEncoding.h"
#include "Translation.h"
#include "StrUtil.h"
#include "FileTime.h"
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace AFS_Core {

namespace one_drive {

namespace fs = std::filesystem;
namespace pt = boost::property_tree;
using http::Error;

namespace {
constexpr long HTTP_CONFLICT = 409;
constexpr std::uintmax_t CHUNK_BYTES = 32 * 320 * 1024;  // [OneDrive.md#upload]
constexpr char ITEM_FIELDS[] = "?$select=name,size,folder,file,fileSystemInfo,lastModifiedDateTime";
constexpr char KEY_SEPARATOR = '|';  // [OneDrive.md#json-keys]

struct Token {
    std::string value;
    explicit Token(const SshAccount& account): value(oauth::access_token(account)) {}
};

class GraphRequest : private Token, public http::Request {  // [GoogleDrive.md#DriveRequest]
public:
    GraphRequest(const SshAccount& account, const std::string& url, const char* method): Token(account), http::Request(url, method)
    {
        bearer(value);
    }
};

std::string checked_base(const SshAccount& account)
{
    if (!webdav::address_allowed(account.url()))
        throw Error(util::wstring_to_string(translation::translate("webdav_url_not_https")) + account.url());
    return account.url() + "/me/drive/root";
}

std::string encoded_path(const std::string& path)
{
    std::string encoded;
    for (const std::string& segment : util::split(path, '/'))
        encoded += "/" + percent::encoded(segment);
    return encoded;
}

std::string item_url(const SshAccount& account, const std::string& path, const std::string& facet = "")  // [OneDrive.md#addresses]
{
    const std::string encoded = encoded_path(path);
    if (encoded.empty())
        return checked_base(account) + (facet.empty() ? "" : "/" + facet);
    return checked_base(account) + ":" + encoded + (facet.empty() ? "" : ":/" + facet);
}

std::string parent_of(const std::string& path)
{
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? "" : path.substr(0, slash);
}

std::string name_of(const std::string& path)
{
    return path.substr(path.find_last_of('/') + 1);
}

std::string json_text(const std::string& text)  // [GoogleDrive.md#json]
{
    std::ostringstream quoted;
    quoted << '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\')
            quoted << '\\' << c;
        else if (c < 0x20)
            quoted << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
        else
            quoted << c;
    }
    quoted << '"';
    return quoted.str();
}

std::time_t parse_time(const std::string& rfc3339)
{
    std::tm utc{};
    std::istringstream text(rfc3339);
    text >> std::get_time(&utc, "%Y-%m-%dT%H:%M:%S");
    if (text.fail())
        return 0;
#ifdef _WIN32
    return _mkgmtime(&utc);
#else
    return timegm(&utc);
#endif
}

std::string format_time(std::time_t time)
{
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream text;
    text << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return text.str();
}

std::string time_json(std::time_t modified)  // [OneDrive.md#modification-time]
{
    return "\"fileSystemInfo\":{\"lastModifiedDateTime\":" + json_text(format_time(modified)) + "}";
}

pt::ptree parsed(const http::Request& request)
{
    pt::ptree answer;
    std::istringstream json(request.response());
    try {
        pt::read_json(json, answer);
    } catch (const pt::json_parser_error& e) {
        throw Error(std::string("OneDrive answer is not JSON: ") + e.what());
    }
    return answer;
}

std::string value_of(const pt::ptree& node, const char* key)  // [OneDrive.md#json-keys]
{
    return node.get<std::string>(pt::ptree::path_type(key, KEY_SEPARATOR), "");
}

Entry entry_of(const pt::ptree& item)
{
    Entry entry;
    entry.name = value_of(item, "name");
    entry.is_directory = item.get_child_optional("folder").has_value();
    entry.size = entry.is_directory ? 0 : item.get<uint64_t>("size", 0);
    const std::string own_time = value_of(item, "fileSystemInfo|lastModifiedDateTime");
    entry.mtime = parse_time(own_time.empty() ? value_of(item, "lastModifiedDateTime") : own_time);
    return entry;
}

std::optional<pt::ptree> item_at(const SshAccount& account, const std::string& path, const char* fields = ITEM_FIELDS)
{
    GraphRequest request(account, item_url(account, path) + fields, "GET");
    const auto status = request.perform();
    if (status && *status == http::NOT_FOUND)
        return std::nullopt;
    request.expect_success(status);
    return parsed(request);
}

void send_json(const SshAccount& account, const std::string& url, const char* method, const std::string& json)
{
    GraphRequest request(account, url, method);
    request.header("Content-Type: application/json; charset=utf-8");
    request.body(json);
    request.expect_success(request.perform());
}

std::string upload_session(const SshAccount& account, const std::string& path, std::time_t modified)  // [OneDrive.md#upload]
{
    GraphRequest request(account, item_url(account, path, "createUploadSession"), "POST");
    request.header("Content-Type: application/json; charset=utf-8");
    request.body("{\"item\":{\"@microsoft.graph.conflictBehavior\":\"replace\"," + time_json(modified) + "}}");
    request.expect_success(request.perform());
    const std::string url = value_of(parsed(request), "uploadUrl");
    if (url.empty() || !webdav::address_allowed(url))
        throw Error("OneDrive named no place for the upload");
    return url;
}

CopyResult upload_empty(const SshAccount& account, const std::string& path, std::time_t modified)  // [OneDrive.md#empty-files]
{
    {
        GraphRequest request(account, item_url(account, path, "content"), "PUT");
        request.header("Content-Type: application/octet-stream");
        request.body("");
        request.expect_success(request.perform());
    }
    send_json(account, item_url(account, path), "PATCH", "{" + time_json(modified) + "}");
    return CopyResult::OK;
}

CopyResult upload_chunks(const std::string& session, std::ifstream& in, std::uintmax_t size)  // [OneDrive.md#upload]
{
    for (std::uintmax_t sent = 0; sent < size;) {
        const std::uintmax_t length = std::min(CHUNK_BYTES, size - sent);
        http::Request request(session, "PUT");
        request.header("Content-Range: bytes " + std::to_string(sent) + "-" + std::to_string(sent + length - 1) + "/" + std::to_string(size));
        request.upload_from(in, length, sent);
        const auto status = request.perform();
        if (!status)
            return CopyResult::Stopped;
        request.expect_success(status);
        sent += length;
    }
    return CopyResult::OK;
}
}

std::optional<Entry> stat(const SshAccount& account, const std::string& path)
{
    const auto item = item_at(account, path);
    if (!item)
        return std::nullopt;
    Entry entry = entry_of(*item);
    entry.is_directory = entry.is_directory || encoded_path(path).empty();
    return entry;
}

std::vector<Entry> list(const SshAccount& account, const std::string& path)
{
    std::vector<Entry> entries;
    std::string page = item_url(account, path, "children") + ITEM_FIELDS;
    while (!page.empty()) {
        if (!webdav::address_allowed(page))
            throw Error("OneDrive named an unencrypted address for the next page");
        GraphRequest request(account, page, "GET");
        request.expect_success(request.perform());
        const pt::ptree answer = parsed(request);
        if (const auto items = answer.get_child_optional("value"))
            for (const auto& [key, item] : *items)
                if (const Entry entry = entry_of(item); fsu::plain_entry_name(entry.name))  // [WebDav.md#entry-names]
                    entries.push_back(entry);
        page = value_of(answer, "@odata.nextLink");
    }
    return entries;
}

void make_directory(const SshAccount& account, const std::string& path)
{
    GraphRequest request(account, item_url(account, parent_of(path), "children"), "POST");
    request.header("Content-Type: application/json; charset=utf-8");
    request.body("{\"name\":" + json_text(name_of(path)) + ",\"folder\":{},\"@microsoft.graph.conflictBehavior\":\"fail\"}");
    const auto status = request.perform();
    if (status && *status == HTTP_CONFLICT) {
        const auto existing = stat(account, path);
        if (existing && existing->is_directory)
            return;
    }
    request.expect_success(status);
}

void remove(const SshAccount& account, const std::string& path)
{
    GraphRequest request(account, item_url(account, path), "DELETE");
    request.expect_success(request.perform());
}

void move(const SshAccount& account, const std::string& from, const std::string& to)
{
    if (stat(account, to))
        throw Error(util::wstring_to_string(translation::translate("one_drive_name_taken")) + to);
    send_json(account, item_url(account, from) + "?@microsoft.graph.conflictBehavior=fail", "PATCH",
        "{\"name\":" + json_text(name_of(to)) + ",\"parentReference\":{\"path\":" + json_text("/drive/root:" + parent_of(to)) + "}}");
}

CopyResult download(const SshAccount& account, const std::string& path, const fs::path& local_file)
{
    const auto item = item_at(account, path, "");
    if (!item)
        throw Error(util::wstring_to_string(translation::translate("one_drive_file_missing")) + path);
    const std::string url = value_of(*item, "@microsoft.graph.downloadUrl");
    if (url.empty() || !webdav::address_allowed(url))
        throw Error("OneDrive named no place for the download");
    http::Request request(url, "GET");
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
    if (const std::time_t modified = entry_of(*item).mtime; modified > 0)
        fs::last_write_time(local_file, file_time::to_file_time(modified), ec);
    return CopyResult::OK;
}

CopyResult upload(const fs::path& local_file, const SshAccount& account, const std::string& path)
{
    std::error_code ec;
    const auto size = fs::file_size(local_file, ec);
    const std::time_t modified = file_time::to_time_t(fs::last_write_time(local_file, ec));
    std::ifstream in(local_file, std::ios::binary);
    if (ec || !in.is_open())
        throw Error(util::wstring_to_string(translation::translate("local_file_open_error")) + local_file.string());
    if (size == 0)
        return upload_empty(account, path, modified);
    return upload_chunks(upload_session(account, path, modified), in, size);
}

}

} // namespace AFS_Core
