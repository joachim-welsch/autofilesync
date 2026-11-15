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
#include "S3.h"
#include "FilesystemUtil.h"
#include "Hex.h"
#include "HttpClient.h"
#include "PercentEncoding.h"
#include "Translation.h"
#include "StrUtil.h"
#include "FileTime.h"
#include <filesystem>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace AFS_Core {

namespace s3 {

namespace fs = std::filesystem;
namespace pt = boost::property_tree;
using http::Error;

namespace {
constexpr char DEFAULT_REGION[] = "us-east-1";
constexpr char UNSIGNED_PAYLOAD[] = "UNSIGNED-PAYLOAD";  // [S3.md#unsigned-payload]
constexpr char MTIME_HEADER[] = "x-amz-meta-mtime";  // [S3.md#modification-time]
constexpr char AWS_HOST_SUFFIX[] = ".amazonaws.com";  // [S3.md#addressing]
constexpr std::uintmax_t PART_BYTES = 16 * 1024 * 1024;  // [S3.md#upload]
constexpr std::size_t HASH_BYTES = 32;
constexpr long HTTP_FORBIDDEN = 403;

using Fields = std::map<std::string, std::string>;

struct Place {  // [S3.md#paths]
    std::string bucket;
    std::string key;
};

struct Call {
    std::string method;
    Place place;
    Fields query;
    Fields amz_headers;
};

Place place_of(const std::string& path)
{
    const std::vector<std::string> segments = util::split(path, '/');
    Place place;
    for (std::size_t i = 0; i < segments.size(); ++i)
        (i == 0 ? place.bucket : place.key) += (i > 1 ? "/" : "") + segments[i];
    return place;
}

std::string sha256_hex(const std::string& text)
{
    unsigned char digest[HASH_BYTES];
    unsigned int length = 0;
    EVP_Digest(text.data(), text.size(), digest, &length, EVP_sha256(), nullptr);
    return hex::encoded({digest, length});
}

std::string hmac(const std::string& key, const std::string& text)
{
    unsigned char digest[HASH_BYTES];
    unsigned int length = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), reinterpret_cast<const unsigned char*>(text.data()), text.size(), digest, &length);
    return std::string(reinterpret_cast<const char*>(digest), length);
}

std::string utc(const char* format)
{
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
#ifdef _WIN32
    gmtime_s(&parts, &now);
#else
    gmtime_r(&now, &parts);
#endif
    std::ostringstream text;
    text << std::put_time(&parts, format);
    return text.str();
}

std::string encoded_key(const std::string& key)
{
    std::string encoded;
    std::istringstream segments(key);
    for (std::string segment; std::getline(segments, segment, '/');)
        encoded += (encoded.empty() ? "" : "/") + percent::encoded(segment);
    return key.ends_with('/') ? encoded + "/" : encoded;
}

std::string encoded_query(const Fields& query)
{
    std::string text;
    for (const auto& [name, value] : query)
        text += (text.empty() ? "" : "&") + percent::encoded(name) + "=" + percent::encoded(value);
    return text;
}

struct Address {  // [S3.md#addressing]
    std::string scheme_and_host;
    std::string host;
    std::string uri;
};

Address address_of(const SshAccount& account, const Place& place)
{
    const std::string endpoint = util::without_trailing_chars(account.url(), '/');
    if (!webdav::address_allowed(endpoint))
        throw Error(util::wstring_to_string(translation::translate("webdav_url_not_https")) + endpoint);
    const auto host_begin = endpoint.find("://") + 3;
    const std::string scheme = endpoint.substr(0, host_begin);
    const std::string host = endpoint.substr(host_begin);
    const bool virtual_hosted = host.ends_with(AWS_HOST_SUFFIX) && !place.bucket.empty();
    if (virtual_hosted)
        return {scheme + place.bucket + "." + host, place.bucket + "." + host, "/" + encoded_key(place.key)};
    return {endpoint, host, place.bucket.empty() ? "/" : "/" + percent::encoded(place.bucket) + (place.key.empty() ? "" : "/" + encoded_key(place.key))};
}

std::string authorization(const SshAccount& account, const Call& call, const Address& address, const Fields& headers, const std::string& amz_date)  // [S3.md#signature]
{
    const std::string region = account.region().empty() ? DEFAULT_REGION : account.region();
    const std::string date = amz_date.substr(0, 8);
    std::string canonical_headers;
    std::string signed_headers;
    for (const auto& [name, value] : headers) {
        canonical_headers += name + ":" + value + "\n";
        signed_headers += (signed_headers.empty() ? "" : ";") + name;
    }
    const std::string canonical_request = call.method + "\n" + address.uri + "\n" + encoded_query(call.query) + "\n" + canonical_headers + "\n" + signed_headers + "\n" + UNSIGNED_PAYLOAD;
    const std::string scope = date + "/" + region + "/s3/aws4_request";
    const std::string string_to_sign = "AWS4-HMAC-SHA256\n" + amz_date + "\n" + scope + "\n" + sha256_hex(canonical_request);
    const std::string signing_key = hmac(hmac(hmac(hmac("AWS4" + account.password(), date), region), "s3"), "aws4_request");
    const std::string signature = hmac(signing_key, string_to_sign);
    return "AWS4-HMAC-SHA256 Credential=" + account.user() + "/" + scope + ", SignedHeaders=" + signed_headers
        + ", Signature=" + hex::encoded(signature);
}

std::unique_ptr<http::Request> request_for(const SshAccount& account, const Call& call)  // [S3.md#signature]
{
    const Address address = address_of(account, call.place);
    const std::string amz_date = utc("%Y%m%dT%H%M%SZ");
    Fields headers = call.amz_headers;
    headers["host"] = address.host;
    headers["x-amz-content-sha256"] = UNSIGNED_PAYLOAD;
    headers["x-amz-date"] = amz_date;
    const std::string query = encoded_query(call.query);
    auto request = std::make_unique<http::Request>(address.scheme_and_host + address.uri + (query.empty() ? "" : "?" + query), call.method.c_str());
    for (const auto& [name, value] : headers)
        if (name != "host")
            request->header(name + ": " + value);
    request->header("Authorization: " + authorization(account, call, address, headers, amz_date));
    request->header("Content-Type: application/octet-stream");
    return request;
}

std::size_t note_header(char* data, std::size_t size, std::size_t count, void* headers)
{
    const std::string line(data, size * count);
    const auto colon = line.find(':');
    if (colon != std::string::npos)
        (*static_cast<Fields*>(headers))[util::lowercase(line.substr(0, colon))] = util::trimmed(std::string_view(line).substr(colon + 1));
    return size * count;
}

[[noreturn]] void fail(const http::Request& request, long status)  // [S3.md#errors]
{
    const auto begin = request.response().find("<Message>");
    const auto end = request.response().find("</Message>");
    const std::string reason = begin != std::string::npos && end != std::string::npos ? ": " + request.response().substr(begin + 9, end - begin - 9) : "";
    throw Error("S3 HTTP " + std::to_string(status) + reason);
}

void expect_success(const http::Request& request, std::optional<long> status)
{
    if (status && (*status < http::OK || *status >= http::MULTIPLE_CHOICES))
        fail(request, *status);
}

pt::ptree answered_xml(const SshAccount& account, const Call& call)
{
    const auto request = request_for(account, call);
    expect_success(*request, request->perform());
    pt::ptree document;
    std::istringstream xml(request->response());
    try {
        pt::read_xml(xml, document);
    } catch (const pt::xml_parser_error& e) {
        throw Error(std::string("S3 answer is not XML: ") + e.what());
    }
    return document;
}

void send(const SshAccount& account, const Call& call)
{
    const auto request = request_for(account, call);
    if (call.method == "PUT")
        request->body("");
    expect_success(*request, request->perform());
    if (request->response().find("<Error>") != std::string::npos)  // [S3.md#move]
        fail(*request, http::OK);
}

std::time_t parse_iso_time(const std::string& text)  // "2026-11-14T17:31:57.000Z"
{
    std::tm utc_time{};
    std::istringstream in(text);
    in >> std::get_time(&utc_time, "%Y-%m-%dT%H:%M:%S");
    if (in.fail())
        return 0;
#ifdef _WIN32
    return _mkgmtime(&utc_time);
#else
    return timegm(&utc_time);
#endif
}

std::time_t modified_of(const Fields& headers)  // [S3.md#modification-time]
{
    const auto own = headers.find(MTIME_HEADER);
    if (own != headers.end() && !own->second.empty() && std::isdigit(static_cast<unsigned char>(own->second.front())))
        return static_cast<std::time_t>(std::strtod(own->second.c_str(), nullptr));
    const auto server = headers.find("last-modified");
    return server == headers.end() ? 0 : std::max<std::time_t>(http::parse_date(server->second), 0);
}

std::optional<Fields> head(const SshAccount& account, const Place& place)  // [S3.md#stat]
{
    const auto request = request_for(account, {"HEAD", place, {}, {}});
    Fields headers;
    request->without_body();
    request->on_header(note_header, &headers);
    const auto status = request->perform();
    if (status && *status == http::NOT_FOUND)
        return std::nullopt;
    if (status && *status == HTTP_FORBIDDEN)
        throw Error(util::wstring_to_string(translation::translate("s3_access_denied")) + place.bucket + "/" + place.key);
    expect_success(*request, status);
    return headers;
}

std::string name_of(const std::string& key)
{
    const std::string path = util::without_trailing_chars(key, '/');
    return path.substr(path.rfind('/') + 1);
}

struct Page {
    std::vector<Entry> entries;
    std::string next;
};

Page list_page(const SshAccount& account, const std::string& bucket, const std::string& prefix, const std::string& token, const char* max_keys)  // [S3.md#list]
{
    Fields query = {{"list-type", "2"}, {"delimiter", "/"}, {"prefix", prefix}};
    if (!token.empty())
        query["continuation-token"] = token;
    if (*max_keys != 0)
        query["max-keys"] = max_keys;
    const pt::ptree document = answered_xml(account, {"GET", {bucket, ""}, query, {}});
    Page page;
    const auto result = document.get_child_optional("ListBucketResult");
    for (const auto& [name, node] : result ? *result : pt::ptree()) {
        if (name == "Contents" && node.get<std::string>("Key", "") != prefix)
            page.entries.push_back({name_of(node.get<std::string>("Key", "")), false, node.get<uint64_t>("Size", 0), parse_iso_time(node.get<std::string>("LastModified", ""))});
        if (name == "CommonPrefixes")
            page.entries.push_back({name_of(node.get<std::string>("Prefix", "")), true, 0, 0});
    }
    if (result && result->get<std::string>("IsTruncated", "false") == "true")
        page.next = result->get<std::string>("NextContinuationToken", "");
    return page;
}

std::vector<Entry> buckets(const SshAccount& account)
{
    const pt::ptree document = answered_xml(account, {"GET", {}, {}, {}});
    std::vector<Entry> entries;
    if (const auto listed = document.get_child_optional("ListAllMyBucketsResult.Buckets"))
        for (const auto& [name, bucket] : *listed)
            entries.push_back({bucket.get<std::string>("Name", ""), true, 0, 0});
    return entries;
}

bool has_entries_below(const SshAccount& account, const Place& place)  // [S3.md#directories]
{
    return !list_page(account, place.bucket, place.key + "/", "", "1").entries.empty() || head(account, {place.bucket, place.key + "/"}).has_value();
}

std::string mtime_text(const fs::path& local_file)
{
    std::error_code ec;
    return std::to_string(file_time::to_time_t(fs::last_write_time(local_file, ec)));
}

std::optional<std::string> upload_part(const SshAccount& account, const Place& place, const std::string& upload_id, int number, std::ifstream& in, std::uintmax_t length, std::uintmax_t sent)
{
    const auto request = request_for(account, {"PUT", place, {{"partNumber", std::to_string(number)}, {"uploadId", upload_id}}, {}});
    Fields headers;
    request->upload_from(in, length, sent);
    request->on_header(note_header, &headers);
    const auto status = request->perform();
    if (!status)
        return std::nullopt;
    expect_success(*request, status);
    return headers["etag"];
}

CopyResult upload_parts(const SshAccount& account, const Place& place, const std::string& upload_id, std::ifstream& in, std::uintmax_t size)  // [S3.md#upload]
{
    std::string parts = "<CompleteMultipartUpload>";
    int number = 1;
    for (std::uintmax_t sent = 0; sent < size; ++number) {
        const std::uintmax_t length = std::min(PART_BYTES, size - sent);
        const auto etag = upload_part(account, place, upload_id, number, in, length, sent);
        if (!etag)
            return CopyResult::Stopped;
        parts += "<Part><PartNumber>" + std::to_string(number) + "</PartNumber><ETag>" + *etag + "</ETag></Part>";
        sent += length;
    }
    const auto request = request_for(account, {"POST", place, {{"uploadId", upload_id}}, {}});
    request->body(parts + "</CompleteMultipartUpload>");
    expect_success(*request, request->perform());
    if (request->response().find("<Error>") != std::string::npos)
        fail(*request, http::OK);
    return CopyResult::OK;
}

void abandon_upload(const SshAccount& account, const Place& place, const std::string& upload_id)  // [S3.md#upload]
{
    try {
        const auto request = request_for(account, {"DELETE", place, {{"uploadId", upload_id}}, {}});
        request->perform();
    } catch (const std::exception&) {
    }
}

CopyResult upload_multipart(const SshAccount& account, const Place& place, const fs::path& local_file, std::ifstream& in, std::uintmax_t size)
{
    const pt::ptree started = answered_xml(account, {"POST", place, {{"uploads", ""}}, {{MTIME_HEADER, mtime_text(local_file)}}});
    const std::string upload_id = started.get<std::string>("InitiateMultipartUploadResult.UploadId", "");
    if (upload_id.empty())
        throw Error("S3 named no multipart upload");
    try {
        const CopyResult result = upload_parts(account, place, upload_id, in, size);
        if (result != CopyResult::OK)
            abandon_upload(account, place, upload_id);
        return result;
    } catch (...) {
        abandon_upload(account, place, upload_id);
        throw;
    }
}
}

std::optional<Entry> stat(const SshAccount& account, const std::string& path)
{
    const Place place = place_of(path);
    if (place.bucket.empty())
        return Entry{"", true, 0, 0};
    const auto headers = head(account, place);
    if (place.key.empty())
        return headers ? std::optional<Entry>(Entry{place.bucket, true, 0, 0}) : std::nullopt;
    if (headers) {
        const auto length = headers->find("content-length");
        return Entry{name_of(place.key), false, length == headers->end() ? 0 : std::strtoull(length->second.c_str(), nullptr, 10), modified_of(*headers)};
    }
    return has_entries_below(account, place) ? std::optional<Entry>(Entry{name_of(place.key), true, 0, 0}) : std::nullopt;
}

std::vector<Entry> list(const SshAccount& account, const std::string& path)
{
    const Place place = place_of(path);
    if (place.bucket.empty())
        return buckets(account);
    std::vector<Entry> entries;
    std::string token;
    do {
        Page page = list_page(account, place.bucket, place.key.empty() ? "" : place.key + "/", token, "");
        for (Entry& entry : page.entries)
            if (fsu::plain_entry_name(entry.name))  // [WebDav.md#entry-names]
                entries.push_back(std::move(entry));
        token = page.next;
    } while (!token.empty());
    return entries;
}

void make_directory(const SshAccount& account, const std::string& path)  // [S3.md#directories]
{
    const Place place = place_of(path);
    const auto existing = stat(account, path);
    if (existing && existing->is_directory)
        return;
    if (existing || place.key.empty())
        throw Error(util::wstring_to_string(translation::translate(existing ? "s3_name_taken" : "s3_bucket_missing")) + path);
    send(account, {"PUT", {place.bucket, place.key + "/"}, {}, {}});
}

void remove(const SshAccount& account, const std::string& path)
{
    const Place place = place_of(path);
    const auto existing = stat(account, path);
    if (!existing || place.key.empty())
        throw Error(util::wstring_to_string(translation::translate("s3_file_missing")) + path);
    send(account, {"DELETE", {place.bucket, existing->is_directory ? place.key + "/" : place.key}, {}, {}});
}

void move(const SshAccount& account, const std::string& from, const std::string& to)
{
    const Place source = place_of(from);
    const Place target = place_of(to);
    const auto existing = stat(account, from);
    if (!existing || existing->is_directory)
        throw Error(util::wstring_to_string(translation::translate(existing ? "s3_folder_not_renamable" : "s3_file_missing")) + from);
    if (stat(account, to))
        throw Error(util::wstring_to_string(translation::translate("s3_name_taken")) + to);
    send(account, {"PUT", target, {}, {{"x-amz-copy-source", "/" + percent::encoded(source.bucket) + "/" + encoded_key(source.key)}}});
    send(account, {"DELETE", source, {}, {}});
}

CopyResult download(const SshAccount& account, const std::string& path, const fs::path& local_file)
{
    const auto request = request_for(account, {"GET", place_of(path), {}, {}});
    Fields headers;
    std::ofstream out(local_file, std::ios::binary);
    if (!out.is_open())
        throw Error(util::wstring_to_string(translation::translate("local_file_open_error")) + local_file.string());
    request->download_to(out);
    request->on_header(note_header, &headers);
    const auto status = request->perform();
    out.close();
    if (!status)
        return CopyResult::Stopped;
    request->expect_success(status);
    std::error_code ec;
    if (const std::time_t modified = modified_of(headers); modified > 0)
        fs::last_write_time(local_file, file_time::to_file_time(modified), ec);
    return CopyResult::OK;
}

CopyResult upload(const fs::path& local_file, const SshAccount& account, const std::string& path)
{
    std::error_code ec;
    const auto size = fs::file_size(local_file, ec);
    std::ifstream in(local_file, std::ios::binary);
    if (ec || !in.is_open())
        throw Error(util::wstring_to_string(translation::translate("local_file_open_error")) + local_file.string());
    const Place place = place_of(path);
    if (size > PART_BYTES)
        return upload_multipart(account, place, local_file, in, size);
    const auto request = request_for(account, {"PUT", place, {}, {{MTIME_HEADER, mtime_text(local_file)}}});
    request->upload_from(in, size);
    const auto status = request->perform();
    if (!status)
        return CopyResult::Stopped;
    expect_success(*request, status);
    return CopyResult::OK;
}

}

} // namespace AFS_Core
