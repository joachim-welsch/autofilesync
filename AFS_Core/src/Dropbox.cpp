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
#include "Dropbox.h"
#include "FilesystemUtil.h"
#include "GoogleAuth.h"
#include "HttpClient.h"
#include "Translation.h"
#include "Util.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace AFS_Core {

namespace dropbox {

namespace fs = std::filesystem;
namespace pt = boost::property_tree;
using http::Error;

namespace {
constexpr long HTTP_CONFLICT = 409;
constexpr std::uintmax_t CHUNK_BYTES = 8 * 1024 * 1024;  // [Dropbox.md#upload]
constexpr char KEY_SEPARATOR = '|';  // [Dropbox.md#json-keys]
constexpr unsigned ASCII_LIMIT = 0x7F;
constexpr unsigned FIRST_SUPPLEMENTARY = 0x10000;

struct Token {
    std::string value;
    explicit Token(const SshAccount& account): value(oauth::access_token(account)) {}
};

class DropboxRequest : private Token, public http::Request {  // [GoogleDrive.md#DriveRequest]
public:
    DropboxRequest(const SshAccount& account, const std::string& url, const char* method): Token(account), http::Request(url, method)
    {
        bearer(value);
    }
};

std::string checked(const std::string& url)
{
    if (!webdav::address_allowed(url))
        throw Error(util::wstring_to_string(translation::translate("webdav_url_not_https")) + url);
    return url;
}

std::string content_url(const std::string& endpoint)  // [Dropbox.md#two-hosts]
{
    return checked(dropbox_auth::settings().content_url) + endpoint;
}

void append_escape(std::ostringstream& text, unsigned unit)
{
    text << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unit << std::dec;
}

unsigned code_point(const std::string& text, std::size_t& index)  // decodes one UTF-8 sequence and moves the index past it
{
    const unsigned char lead = static_cast<unsigned char>(text[index++]);
    const int extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
    unsigned value = extra == 3 ? lead & 0x07u : extra == 2 ? lead & 0x0Fu : extra == 1 ? lead & 0x1Fu : lead;
    for (int i = 0; i < extra && index < text.size(); ++i)
        value = (value << 6) | (static_cast<unsigned char>(text[index++]) & 0x3Fu);
    return value;
}

std::string json_text(const std::string& text)  // [Dropbox.md#header-safe-json]
{
    std::ostringstream quoted;
    quoted << '"';
    for (std::size_t index = 0; index < text.size();) {
        const unsigned c = code_point(text, index);
        if (c == '"' || c == '\\')
            quoted << '\\' << static_cast<char>(c);
        else if (c >= 0x20 && c < ASCII_LIMIT)
            quoted << static_cast<char>(c);
        else if (c < FIRST_SUPPLEMENTARY)
            append_escape(quoted, c);
        else {
            append_escape(quoted, 0xD800 + ((c - FIRST_SUPPLEMENTARY) >> 10));
            append_escape(quoted, 0xDC00 + ((c - FIRST_SUPPLEMENTARY) & 0x3FF));
        }
    }
    quoted << '"';
    return quoted.str();
}

std::string api_path(const std::string& path)  // [Dropbox.md#paths]
{
    std::string normal;
    std::istringstream segments(path);
    for (std::string segment; std::getline(segments, segment, '/');)
        if (!segment.empty())
            normal += "/" + segment;
    return normal;
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

pt::ptree parsed(const http::Request& request)
{
    pt::ptree answer;
    std::istringstream json(request.response());
    try {
        pt::read_json(json, answer);
    } catch (const pt::json_parser_error& e) {
        throw Error(std::string("Dropbox answer is not JSON: ") + e.what());
    }
    return answer;
}

std::string value_of(const pt::ptree& node, const char* key)  // [Dropbox.md#json-keys]
{
    return node.get<std::string>(pt::ptree::path_type(key, KEY_SEPARATOR), "");
}

Entry entry_of(const pt::ptree& metadata)
{
    Entry entry;
    entry.name = value_of(metadata, "name");
    entry.is_directory = value_of(metadata, ".tag") == "folder";
    entry.size = metadata.get<uint64_t>("size", 0);
    entry.mtime = parse_time(value_of(metadata, "client_modified"));
    return entry;
}

bool listable(const pt::ptree& metadata)  // [Dropbox.md#list]
{
    const std::string tag = value_of(metadata, ".tag");
    return tag == "file" || tag == "folder";
}

struct Answer {
    long status = 0;
    pt::ptree json;
    std::string summary;
};

Answer call(const SshAccount& account, const std::string& endpoint, const std::string& arguments)  // [Dropbox.md#errors]
{
    DropboxRequest request(account, checked(account.url()) + endpoint, "POST");
    request.header("Content-Type: application/json");
    request.body(arguments);
    const auto status = request.perform();
    Answer answer{status.value_or(0), {}, ""};
    if (answer.status == http::OK || answer.status == HTTP_CONFLICT) {
        answer.json = parsed(request);
        answer.summary = value_of(answer.json, "error_summary");
    } else {
        request.expect_success(status);
    }
    return answer;
}

pt::ptree succeeded(const Answer& answer, const std::string& endpoint)
{
    if (answer.status != http::OK)
        throw Error("Dropbox " + endpoint + ": " + answer.summary);
    return answer.json;
}

std::string commit_json(const std::string& path, std::time_t modified)  // [Dropbox.md#upload]
{
    return "{\"path\":" + json_text(api_path(path)) + ",\"mode\":\"overwrite\",\"mute\":true,\"client_modified\":" + json_text(format_time(modified)) + "}";
}

std::optional<pt::ptree> send_piece(const SshAccount& account, const std::string& endpoint, const std::string& arguments, std::ifstream& in, std::uintmax_t length, std::uintmax_t sent)
{
    DropboxRequest request(account, content_url(endpoint), "POST");
    request.header("Dropbox-API-Arg: " + arguments);
    request.header("Content-Type: application/octet-stream");
    request.upload_from(in, length, sent);
    const auto status = request.perform();
    if (!status)
        return std::nullopt;
    if (*status != http::OK)
        throw Error("Dropbox " + endpoint + ": HTTP " + std::to_string(*status) + " " + request.response());
    return request.response().empty() || request.response() == "null" ? pt::ptree() : parsed(request);
}

std::string cursor_json(const std::string& session, std::uintmax_t offset)
{
    return "{\"session_id\":" + json_text(session) + ",\"offset\":" + std::to_string(offset) + "}";
}

CopyResult upload_in_session(const SshAccount& account, const std::string& path, std::time_t modified, std::ifstream& in, std::uintmax_t size)  // [Dropbox.md#upload]
{
    const auto started = send_piece(account, "/files/upload_session/start", "{\"close\":false}", in, CHUNK_BYTES, 0);
    if (!started)
        return CopyResult::Stopped;
    const std::string session = value_of(*started, "session_id");
    std::uintmax_t sent = CHUNK_BYTES;
    for (; size - sent > CHUNK_BYTES; sent += CHUNK_BYTES)
        if (!send_piece(account, "/files/upload_session/append_v2", "{\"cursor\":" + cursor_json(session, sent) + ",\"close\":false}", in, CHUNK_BYTES, sent))
            return CopyResult::Stopped;
    const std::string finish = "{\"cursor\":" + cursor_json(session, sent) + ",\"commit\":" + commit_json(path, modified) + "}";
    return send_piece(account, "/files/upload_session/finish", finish, in, size - sent, sent) ? CopyResult::OK : CopyResult::Stopped;
}
}

std::optional<Entry> stat(const SshAccount& account, const std::string& path)
{
    if (api_path(path).empty())
        return Entry{"", true, 0, 0};
    const Answer answer = call(account, "/files/get_metadata", "{\"path\":" + json_text(api_path(path)) + "}");
    if (answer.status == HTTP_CONFLICT && answer.summary.starts_with("path/not_found"))
        return std::nullopt;
    const pt::ptree metadata = succeeded(answer, "get_metadata");
    return listable(metadata) ? std::optional<Entry>(entry_of(metadata)) : std::nullopt;
}

std::vector<Entry> list(const SshAccount& account, const std::string& path)
{
    std::vector<Entry> entries;
    pt::ptree page = succeeded(call(account, "/files/list_folder", "{\"path\":" + json_text(api_path(path)) + "}"), "list_folder");
    for (;;) {
        if (const auto listed = page.get_child_optional("entries"))
            for (const auto& [key, metadata] : *listed)
                if (const Entry entry = entry_of(metadata); listable(metadata) && fsu::plain_entry_name(entry.name))  // [WebDav.md#entry-names]
                    entries.push_back(entry);
        if (!page.get<bool>("has_more", false))
            return entries;
        page = succeeded(call(account, "/files/list_folder/continue", "{\"cursor\":" + json_text(value_of(page, "cursor")) + "}"), "list_folder/continue");
    }
}

void make_directory(const SshAccount& account, const std::string& path)
{
    const Answer answer = call(account, "/files/create_folder_v2", "{\"path\":" + json_text(api_path(path)) + ",\"autorename\":false}");
    if (answer.status == HTTP_CONFLICT && answer.summary.starts_with("path/conflict/folder"))
        return;
    succeeded(answer, "create_folder");
}

void remove(const SshAccount& account, const std::string& path)
{
    succeeded(call(account, "/files/delete_v2", "{\"path\":" + json_text(api_path(path)) + "}"), "delete");
}

void move(const SshAccount& account, const std::string& from, const std::string& to)
{
    succeeded(call(account, "/files/move_v2", "{\"from_path\":" + json_text(api_path(from)) + ",\"to_path\":" + json_text(api_path(to)) + ",\"autorename\":false}"), "move");
}

CopyResult download(const SshAccount& account, const std::string& path, const fs::path& local_file)
{
    const auto entry = stat(account, path);
    if (!entry)
        throw Error(util::wstring_to_string(translation::translate("dropbox_file_missing")) + path);
    DropboxRequest request(account, content_url("/files/download"), "GET");
    request.header("Dropbox-API-Arg: {\"path\":" + json_text(api_path(path)) + "}");
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
    if (entry->mtime > 0)
        fs::last_write_time(local_file, util::to_file_time(entry->mtime), ec);
    return CopyResult::OK;
}

CopyResult upload(const fs::path& local_file, const SshAccount& account, const std::string& path)
{
    std::error_code ec;
    const auto size = fs::file_size(local_file, ec);
    const std::time_t modified = util::to_time_t(fs::last_write_time(local_file, ec));
    std::ifstream in(local_file, std::ios::binary);
    if (ec || !in.is_open())
        throw Error(util::wstring_to_string(translation::translate("local_file_open_error")) + local_file.string());
    if (size > CHUNK_BYTES)
        return upload_in_session(account, path, modified, in, size);
    return send_piece(account, "/files/upload", commit_json(path, modified), in, size, 0) ? CopyResult::OK : CopyResult::Stopped;
}

}

} // namespace AFS_Core
