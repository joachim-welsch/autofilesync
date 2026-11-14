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
#pragma once
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>

namespace AFS_Core {

namespace http {  // [HttpClient.md#http]

constexpr long OK = 200;
constexpr long MULTIPLE_CHOICES = 300;
constexpr long UNAUTHORIZED = 401;
constexpr long NOT_FOUND = 404;

struct Error : std::runtime_error {  // [HttpClient.md#errors]
    using std::runtime_error::runtime_error;
};

using HeaderCallback = std::size_t (*)(char* data, std::size_t size, std::size_t count, void* context);

std::string escaped(const std::string& text);  // [HttpClient.md#escaping]
std::string unescaped(const std::string& text);
std::time_t parse_date(const std::string& text);

class Request {  // [HttpClient.md#Request]
    void* curl_;
    void* headers_ = nullptr;
    std::string url_;
    std::string body_;
    std::string response_;
    char error_[256] = {};
public:
    Request(const std::string& url, const char* method);
    ~Request();
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    const std::string& url() const { return url_; }
    const std::string& response() const { return response_; }
    void basic_auth(const std::string& user, const std::string& password);
    void bearer(const std::string& token);
    void header(const std::string& line);
    void body(const std::string& content);
    void download_to(std::ofstream& file);  // [HttpClient.md#transfers]
    void upload_from(std::ifstream& file, std::uintmax_t size);
    void on_header(HeaderCallback callback, void* context);
    std::optional<long> perform();  // [HttpClient.md#perform]
    std::optional<std::time_t> file_time() const;
    [[noreturn]] void fail(long status) const;
    void expect_success(std::optional<long> status) const;
};

}

} // namespace AFS_Core
