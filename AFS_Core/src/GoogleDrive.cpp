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
#include "GoogleDrive.h"
#include "FilesystemUtil.h"
#include "GoogleAuth.h"
#include "HttpClient.h"
#include "Translation.h"
#include "Util.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace AFS_Core {

namespace google_drive {

namespace fs = std::filesystem;
namespace pt = boost::property_tree;
using http::Error;

namespace {
constexpr char ROOT_ID[] = "root";
constexpr char FOLDER_TYPE[] = "application/vnd.google-apps.folder";
constexpr char GOOGLE_TYPE_PREFIX[] = "application/vnd.google-apps.";  // [GoogleDrive.md#google-documents]
constexpr char FIELDS[] = "id,name,mimeType,size,modifiedTime";
constexpr char PAGE_SIZE[] = "1000";

struct Node {
    std::string id;
    std::string type;
    Entry entry;
};

struct Token {
    std::string value;
    explicit Token(const SshAccount& account): value(google_auth::access_token(account)) {}
};

class DriveRequest : private Token, public http::Request {  // [GoogleDrive.md#DriveRequest]
public:
    DriveRequest(const SshAccount& account, const std::string& url, const char* method): Token(account), http::Request(url, method)
    {
        bearer(value);
    }
};

std::mutex folder_mutex;
std::map<std::string, std::string> folder_ids;  // [GoogleDrive.md#folder-cache]

std::string cache_key(const SshAccount& account, const std::string& path)
{
    return account.name() + "\n" + path;
}

void forget_folders()  // [GoogleDrive.md#folder-cache]
{
    std::lock_guard<std::mutex> lock(folder_mutex);
    folder_ids.clear();
}

std::string files_url(const SshAccount& account, const std::string& rest = "", bool upload = false)
{
    if (!webdav::address_allowed(account.url()))
        throw Error(util::wstring_to_string(translation::translate("webdav_url_not_https")) + account.url());
    return account.url() + (upload ? "/upload" : "") + "/drive/v3/files" + rest;
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

std::string query_text(const std::string& text)  // [GoogleDrive.md#queries]
{
    std::string quoted = "'";
    for (const char c : text) {
        if (c == '\'' || c == '\\')
            quoted += '\\';
        quoted += c;
    }
    return quoted + "'";
}

std::time_t parse_time(const std::string& rfc3339)  // "2026-11-14T17:31:57.000Z"
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

pt::ptree answer_of(const http::Request& request, std::optional<long> status)
{
    request.expect_success(status);
    pt::ptree answer;
    std::istringstream json(request.response());
    try {
        pt::read_json(json, answer);
    } catch (const pt::json_parser_error& e) {
        throw Error(std::string("Google Drive answer is not JSON: ") + e.what());
    }
    return answer;
}

Node node_of(const pt::ptree& file)
{
    Node node;
    node.id = file.get<std::string>("id");
    node.type = file.get<std::string>("mimeType", "");
    node.entry.name = file.get<std::string>("name", "");
    node.entry.is_directory = node.type == FOLDER_TYPE;
    node.entry.size = file.get<uint64_t>("size", 0);
    node.entry.mtime = parse_time(file.get<std::string>("modifiedTime", ""));
    return node;
}

bool usable(const Node& node)  // [GoogleDrive.md#google-documents]
{
    return node.entry.is_directory || !node.type.starts_with(GOOGLE_TYPE_PREFIX);
}

std::vector<Node> children(const SshAccount& account, const std::string& folder_id, const std::string& name_filter = "")  // [GoogleDrive.md#queries]
{
    const std::string query = query_text(folder_id) + " in parents and trashed = false" + (name_filter.empty() ? "" : " and name = " + query_text(name_filter));
    std::vector<Node> found;
    std::string page;
    do {
        DriveRequest request(account, files_url(account, "?spaces=drive&pageSize=" + std::string(PAGE_SIZE) + "&fields=" + http::escaped(std::string("nextPageToken,files(") + FIELDS + ")")
            + "&q=" + http::escaped(query) + (page.empty() ? "" : "&pageToken=" + http::escaped(page))), "GET");
            const pt::ptree answer = answer_of(request, request.perform());
        if (const auto files = answer.get_child_optional("files"))
            for (const auto& [key, file] : *files)
                if (const Node node = node_of(file); usable(node))
                    found.push_back(node);
        page = answer.get<std::string>("nextPageToken", "");
    } while (!page.empty());
    return found;
}

std::optional<Node> child(const SshAccount& account, const std::string& folder_id, const std::string& name)  // [GoogleDrive.md#same-names]
{
    const std::vector<Node> found = children(account, folder_id, name);
    return found.empty() ? std::nullopt : std::optional<Node>(found.front());
}

std::vector<std::string> segments_of(const std::string& path)
{
    std::vector<std::string> segments;
    std::istringstream parts(path);
    for (std::string segment; std::getline(parts, segment, '/');)
        if (!segment.empty())
            segments.push_back(segment);
    return segments;
}

std::string joined(const std::vector<std::string>& segments, std::size_t count)
{
    std::string path;
    for (std::size_t i = 0; i < count; ++i)
        path += "/" + segments[i];
    return path;
}

std::optional<std::string> cached_folder(const SshAccount& account, const std::string& path)
{
    std::lock_guard<std::mutex> lock(folder_mutex);
    const auto found = folder_ids.find(cache_key(account, path));
    return found == folder_ids.end() ? std::nullopt : std::optional<std::string>(found->second);
}

void remember_folder(const SshAccount& account, const std::string& path, const std::string& id)
{
    std::lock_guard<std::mutex> lock(folder_mutex);
    folder_ids[cache_key(account, path)] = id;
}

std::optional<std::string> folder_id(const SshAccount& account, const std::vector<std::string>& segments, std::size_t count)  // [GoogleDrive.md#folder-cache]
{
    if (count == 0)
        return std::string(ROOT_ID);
    const std::string path = joined(segments, count);
    if (const auto known = cached_folder(account, path))
        return known;
    const auto parent = folder_id(account, segments, count - 1);
    const auto node = parent ? child(account, *parent, segments[count - 1]) : std::nullopt;
    if (!node || !node->entry.is_directory)
        return std::nullopt;
    remember_folder(account, path, node->id);
    return node->id;
}

struct Place {  // [GoogleDrive.md#Place]
    std::string parent_id;
    std::string name;
    std::optional<Node> node;
};

std::optional<Place> place_of(const SshAccount& account, const std::string& path)
{
    const std::vector<std::string> segments = segments_of(path);
    if (segments.empty())
        return std::nullopt;
    const auto parent = folder_id(account, segments, segments.size() - 1);
    if (!parent)
        return std::nullopt;
    return Place{*parent, segments.back(), child(account, *parent, segments.back())};
}

Place existing_parent(const SshAccount& account, const std::string& path)
{
    const auto place = place_of(account, path);
    if (!place)
        throw Error(util::wstring_to_string(translation::translate("google_drive_folder_missing")) + path);
    return *place;
}

Node existing(const SshAccount& account, const std::string& path)
{
    const Place place = existing_parent(account, path);
    if (!place.node)
        throw Error(util::wstring_to_string(translation::translate("google_drive_file_missing")) + path);
    return *place.node;
}

void change(const SshAccount& account, const std::string& id, const std::string& parameters, const std::string& metadata)
{
    DriveRequest request(account, files_url(account, "/" + http::escaped(id) + parameters), "PATCH");
    request.header("Content-Type: application/json; charset=utf-8");
    request.body(metadata);
    request.expect_success(request.perform());
}

std::size_t note_location(char* data, std::size_t size, std::size_t count, void* location)  // [GoogleDrive.md#upload]
{
    const std::string line(data, size * count);
    std::string name = line.substr(0, line.find(':'));
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (name == "location") {
        const auto begin = line.find_first_not_of(" \t", line.find(':') + 1);
        const auto end = line.find_last_not_of(" \t\r\n");
        if (begin != std::string::npos)
            *static_cast<std::string*>(location) = line.substr(begin, end - begin + 1);
    }
    return size * count;
}

std::string upload_session(const SshAccount& account, const Place& place, std::time_t modified, std::uintmax_t size)  // [GoogleDrive.md#upload]
{
    const std::string target = place.node ? "/" + http::escaped(place.node->id) : "";
    std::string metadata = "{\"modifiedTime\":" + json_text(format_time(modified));
    if (!place.node)
        metadata += ",\"name\":" + json_text(place.name) + ",\"parents\":[" + json_text(place.parent_id) + "]";
    DriveRequest request(account, files_url(account, target + "?uploadType=resumable", true), place.node ? "PATCH" : "POST");
    std::string location;
    request.header("Content-Type: application/json; charset=utf-8");
    request.header("X-Upload-Content-Length: " + std::to_string(size));
    request.body(metadata + "}");
    request.on_header(note_location, &location);
    request.expect_success(request.perform());
    if (location.empty() || !webdav::address_allowed(location))
        throw Error("Google Drive named no place for the upload");
    return location;
}
}

std::optional<Entry> stat(const SshAccount& account, const std::string& path)
{
    if (segments_of(path).empty())
        return Entry{"", true, 0, 0};
    const auto place = place_of(account, path);
    return place && place->node ? std::optional<Entry>(place->node->entry) : std::nullopt;
}

std::vector<Entry> list(const SshAccount& account, const std::string& path)
{
    const std::vector<std::string> segments = segments_of(path);
    const auto folder = folder_id(account, segments, segments.size());
    if (!folder)
        throw Error(util::wstring_to_string(translation::translate("google_drive_folder_missing")) + path);
    std::vector<Entry> entries;
    for (const Node& node : children(account, *folder))
        if (fsu::plain_entry_name(node.entry.name))  // [WebDav.md#entry-names]
            entries.push_back(node.entry);
    return entries;
}

void make_directory(const SshAccount& account, const std::string& path)
{
    const Place place = existing_parent(account, path);
    if (place.node && place.node->entry.is_directory)
        return;
    if (place.node)
        throw Error(util::wstring_to_string(translation::translate("google_drive_name_taken")) + path);
    DriveRequest request(account, files_url(account, std::string("?fields=") + http::escaped(FIELDS)), "POST");
    request.header("Content-Type: application/json; charset=utf-8");
    request.body("{\"name\":" + json_text(place.name) + ",\"mimeType\":" + json_text(FOLDER_TYPE) + ",\"parents\":[" + json_text(place.parent_id) + "]}");
    remember_folder(account, joined(segments_of(path), segments_of(path).size()), node_of(answer_of(request, request.perform())).id);
}

void remove(const SshAccount& account, const std::string& path)
{
    const Node node = existing(account, path);
    change(account, node.id, "", "{\"trashed\":true}");
    forget_folders();
}

void move(const SshAccount& account, const std::string& from, const std::string& to)
{
    const Place source = existing_parent(account, from);
    const Place target = existing_parent(account, to);
    if (!source.node)
        throw Error(util::wstring_to_string(translation::translate("google_drive_file_missing")) + from);
    if (target.node)
        throw Error(util::wstring_to_string(translation::translate("google_drive_name_taken")) + to);
    const bool same_folder = source.parent_id == target.parent_id;
    const std::string parents = same_folder ? "" : "?addParents=" + http::escaped(target.parent_id) + "&removeParents=" + http::escaped(source.parent_id);
    change(account, source.node->id, parents, "{\"name\":" + json_text(target.name) + "}");
    forget_folders();
}

CopyResult download(const SshAccount& account, const std::string& path, const fs::path& local_file)
{
    const Node node = existing(account, path);
    DriveRequest request(account, files_url(account, "/" + http::escaped(node.id) + "?alt=media"), "GET");
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
    if (node.entry.mtime > 0)
        fs::last_write_time(local_file, util::to_file_time(node.entry.mtime), ec);
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
    const std::string session = upload_session(account, existing_parent(account, path), util::to_time_t(modified), size);
    http::Request request(session, "PUT");
    request.upload_from(in, size);
    const auto status = request.perform();
    if (!status)
        return CopyResult::Stopped;
    request.expect_success(status);
    return CopyResult::OK;
}

}

} // namespace AFS_Core
