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
#include "GoogleAuth.h"
#include "HttpClient.h"
#include "Translation.h"
#include "Util.h"
#include "WebDav.h"
#include <map>
#include <mutex>
#include <sstream>
#include <vector>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <openssl/evp.h>
#include <openssl/rand.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

#ifndef AFS_GOOGLE_CLIENT_ID
    #define AFS_GOOGLE_CLIENT_ID ""  // [GoogleAuth.md#client]
    #define AFS_GOOGLE_CLIENT_SECRET ""
#endif

namespace AFS_Core {

namespace google_auth {

namespace pt = boost::property_tree;
using std::chrono::steady_clock;

namespace {
constexpr char CLIENT_ID[] = AFS_GOOGLE_CLIENT_ID;  // [GoogleAuth.md#client]
constexpr char CLIENT_SECRET[] = AFS_GOOGLE_CLIENT_SECRET;  // [GoogleAuth.md#client]
constexpr char SCOPE[] = "https://www.googleapis.com/auth/drive.file";  // [GoogleAuth.md#scope]
constexpr std::size_t RANDOM_BYTES = 32;
constexpr std::size_t SHA256_BYTES = 32;
constexpr std::size_t REQUEST_BYTES = 4096;
constexpr std::chrono::seconds TOKEN_MARGIN{60};  // [GoogleAuth.md#access_token]
constexpr char BROWSER_PAGE[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
    "<html><body style=\"font-family:sans-serif;margin:3em\"><h2>AutoFileSync</h2><p>%s</p></body></html>";

struct AccessToken {
    std::string token;
    steady_clock::time_point expires;
};

std::string base64url(const unsigned char* data, std::size_t length)  // [GoogleAuth.md#pkce]
{
    static const char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string text;
    for (std::size_t i = 0; i < length; i += 3) {
        const unsigned b0 = data[i];
        const unsigned b1 = i + 1 < length ? data[i + 1] : 0;
        const unsigned b2 = i + 2 < length ? data[i + 2] : 0;
        text += ALPHABET[b0 >> 2];
        text += ALPHABET[((b0 & 3) << 4) | (b1 >> 4)];
        if (i + 1 < length)
            text += ALPHABET[((b1 & 15) << 2) | (b2 >> 6)];
        if (i + 2 < length)
            text += ALPHABET[b2 & 63];
    }
    return text;
}

std::string random_text()
{
    unsigned char bytes[RANDOM_BYTES];
    if (RAND_bytes(bytes, static_cast<int>(RANDOM_BYTES)) != 1)
        throw http::Error("no random numbers available");
    return base64url(bytes, RANDOM_BYTES);
}

std::string challenge_of(const std::string& verifier)  // [GoogleAuth.md#pkce]
{
    unsigned char digest[SHA256_BYTES];
    unsigned int length = 0;
    EVP_Digest(verifier.data(), verifier.size(), digest, &length, EVP_sha256(), nullptr);
    return base64url(digest, length);
}

void close_socket(int socket)
{
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void start_sockets()
{
#ifdef _WIN32
    static const int started = [] { WSADATA data; return WSAStartup(MAKEWORD(2, 2), &data); }();
    static_cast<void>(started);
#endif
}

std::string parameter(const std::string& query, const std::string& name)
{
    std::istringstream pairs(query);
    for (std::string pair; std::getline(pairs, pair, '&');)
        if (pair.starts_with(name + "="))
            return http::unescaped(pair.substr(name.size() + 1));
    return "";
}

std::string query_of(const std::string& request)  // "GET /?code=...&state=... HTTP/1.1"
{
    const auto begin = request.find('?');
    const auto end = request.find(' ', begin == std::string::npos ? 0 : begin);
    return begin == std::string::npos || end == std::string::npos ? "" : request.substr(begin + 1, end - begin - 1);
}

pt::ptree token_answer(const std::string& form)  // [GoogleAuth.md#token-endpoint]
{
    if (!webdav::address_allowed(settings().token_url))
        throw http::Error(util::wstring_to_string(translation::translate("webdav_url_not_https")) + settings().token_url);
    http::Request request(settings().token_url, "POST");
    request.header("Content-Type: application/x-www-form-urlencoded");
    request.body(form + "&client_id=" + http::escaped(settings().client_id) + "&client_secret=" + http::escaped(settings().client_secret));
    const auto status = request.perform();
    pt::ptree answer;
    std::istringstream json(request.response());
    try {
        pt::read_json(json, answer);
    } catch (const pt::json_parser_error&) {
        request.expect_success(status);
        throw http::Error("the token answer is not JSON");
    }
    if (!status || *status != http::OK)
        throw http::Error(util::wstring_to_string(translation::translate("google_login_refused")) + answer.get<std::string>("error_description", answer.get<std::string>("error", "")));
    return answer;
}

std::mutex token_mutex;
std::map<std::string, AccessToken> tokens;
}

Settings& settings()
{
    static Settings current{CLIENT_ID, CLIENT_SECRET};
    return current;
}

bool configured()
{
    return !settings().client_id.empty();
}

Login::Login()
    : verifier_(random_text() + random_text()), state_(random_text())
{
    start_sockets();
    listener_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t length = sizeof(address);
    const bool listening = listener_ >= 0 && bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0
        && listen(listener_, 1) == 0 && getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length) == 0;
    if (!listening)
        throw http::Error("no local port for the answer of the login");
    port_ = ntohs(address.sin_port);
}

Login::~Login()
{
    if (listener_ >= 0)
        close_socket(listener_);
}

std::string Login::redirect_uri() const  // [GoogleAuth.md#loopback]
{
    return "http://127.0.0.1:" + std::to_string(port_);
}

std::string Login::browser_url() const
{
    return settings().auth_url + "?response_type=code&access_type=offline&prompt=consent&code_challenge_method=S256"
        + "&client_id=" + http::escaped(settings().client_id) + "&redirect_uri=" + http::escaped(redirect_uri())
        + "&scope=" + http::escaped(SCOPE) + "&state=" + state_ + "&code_challenge=" + challenge_of(verifier_);
}

void Login::answer_browser(int connection)  // [GoogleAuth.md#finished]
{
    std::vector<char> buffer(REQUEST_BYTES);
    const auto received = recv(connection, buffer.data(), static_cast<int>(buffer.size() - 1), 0);
    const std::string query = query_of(std::string(buffer.data(), received > 0 ? static_cast<std::size_t>(received) : 0));
    if (parameter(query, "state") != state_)
        error_ = "state";
    else if (parameter(query, "code").empty())
        error_ = parameter(query, "error").empty() ? "code" : parameter(query, "error");
    else
        code_ = parameter(query, "code");
    const std::string text = util::wstring_to_string(translation::translate(code_.empty() ? "google_login_page_failed" : "google_login_page_done"));
    std::vector<char> page(sizeof(BROWSER_PAGE) + text.size());
    const int length = snprintf(page.data(), page.size(), BROWSER_PAGE, text.c_str());
    send(connection, page.data(), length, 0);
    close_socket(connection);
}

bool Login::finished(std::chrono::milliseconds patience)
{
    if (!code_.empty() || !error_.empty())
        return true;
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(listener_, &readable);
    timeval timeout{static_cast<long>(patience.count() / 1000), static_cast<int>(patience.count() % 1000 * 1000)};
    if (select(listener_ + 1, &readable, nullptr, nullptr, &timeout) <= 0)
        return false;
    const int connection = static_cast<int>(accept(listener_, nullptr, nullptr));
    if (connection >= 0)
        answer_browser(connection);
    return !code_.empty() || !error_.empty();
}

std::string Login::refresh_token() const
{
    if (code_.empty())
        throw http::Error(util::wstring_to_string(translation::translate("google_login_refused")) + error_);
    const pt::ptree answer = token_answer("grant_type=authorization_code&code=" + http::escaped(code_)
        + "&code_verifier=" + verifier_ + "&redirect_uri=" + http::escaped(redirect_uri()));
    const std::string token = answer.get<std::string>("refresh_token", "");
    if (token.empty())
        throw http::Error(util::wstring_to_string(translation::translate("google_login_refused")) + "refresh_token");
    return token;
}

std::string access_token(const SshAccount& account)
{
    std::lock_guard<std::mutex> lock(token_mutex);
    AccessToken& cached = tokens[account.name() + "\n" + account.password()];
    if (!cached.token.empty() && steady_clock::now() + TOKEN_MARGIN < cached.expires)
        return cached.token;
    const pt::ptree answer = token_answer("grant_type=refresh_token&refresh_token=" + http::escaped(account.password()));
    cached.token = answer.get<std::string>("access_token");
    cached.expires = steady_clock::now() + std::chrono::seconds(answer.get<long>("expires_in", 0));
    return cached.token;
}

}

} // namespace AFS_Core
