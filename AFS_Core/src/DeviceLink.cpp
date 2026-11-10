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
#include "DeviceLink.h"
#include "UserObject.h"
#include "Config.h"
#include "Synchronize.h"
#include "SyncItem.h"
#include <algorithm>
#include "Logging.h"
#include "StrUtil.h"
#include "Translation.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <boost/property_tree/json_parser.hpp>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace AFS_Core {

namespace {

using util::string_to_wstring;
namespace pt = boost::property_tree;

constexpr int ACCEPT_POLL_MILLISECONDS = 500;
constexpr int HANDSHAKE_TIMEOUT_SECONDS = 3;  // [DeviceLink.md#one-session-at-a-time]
constexpr int SESSION_TIMEOUT_SECONDS = 10;
constexpr size_t LINE_LIMIT = 64 * 1024;
constexpr const char* PROTOCOL_VERSION = "1";

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket NO_SOCKET = INVALID_SOCKET;
void close_socket(Socket s) { closesocket(s); }
#else
using Socket = int;
constexpr Socket NO_SOCKET = -1;
void close_socket(Socket s) { ::close(s); }
#endif

#ifdef _TEST
constexpr bool TEST_SCOPE = true;
#else
constexpr bool TEST_SCOPE = false;
#endif
std::atomic_bool private_scope = TEST_SCOPE;  // [DeviceLink.md#use_private_scope]

void set_timeouts(Socket s, int seconds)
{
#ifdef _WIN32
    DWORD timeout = seconds * 1000;
    const char* value = reinterpret_cast<const char*>(&timeout);
    const int size = sizeof timeout;
#else
    timeval timeout{seconds, 0};
    const void* value = &timeout;
    const socklen_t size = sizeof timeout;
#endif
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, value, size);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, value, size);
}

std::string json_string(const std::string& text)  // [DeviceLink.md#json-output]
{
    std::string out = "\"";
    for (const unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char escaped[8];
                std::snprintf(escaped, sizeof escaped, "\\u%04x", c);
                out += escaped;
            } else
                out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

std::string error_line(const std::string& reason)
{
    return "{\"type\":\"error\",\"reason\":" + json_string(reason) + "}\n";
}

std::optional<pt::ptree> parse(const std::string& line)
{
    pt::ptree tree;
    try {
        std::istringstream in(line);
        pt::read_json(in, tree);
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
    return tree;
}

uint16_t port_of(const pt::ptree& tree, const char* key)
{
    const int value = tree.get<int>(key, 0);
    return value > 0 && value < 65536 ? static_cast<uint16_t>(value) : 0;
}

std::string pc_json()
{
    return "{\"name\":" + json_string(pc_name()) + ",\"fingerprint\":" + json_string(DeviceLink::instance().fingerprint())
        + ",\"version\":" + json_string(PROTOCOL_VERSION) + "}";
}

#ifdef _WIN32
std::wstring role_name()  // [DeviceLink.md#role]
{
    return private_scope ? L"AutoFileSyncDeviceLinkTest" : L"AutoFileSyncDeviceLink";
}
#else
std::string role_path(const std::filesystem::path& dir)  // [DeviceLink.md#role]
{
    if (private_scope)
        return (std::filesystem::temp_directory_path() / ("autofilesync-" + std::to_string(getuid()) + "-test-devicelink.lock")).string();
    return (dir / "devicelink.lock").string();
}
#endif


constexpr const char* PORT_FILE = "devicelink.port";  // [DeviceLink.md#shared-files]
constexpr const char* PAIRED_FILE = "pairing.done";
constexpr const char* PRESENCE_FILE = "devices.presence";
constexpr const char* DISCOVER = "AFS-DISCOVER 1 ";  // [DeviceLink.md#discovery]
constexpr const char* HERE = "AFS-HERE 1 ";

void write_file(const std::filesystem::path& file, const std::string& text)
{
    const std::filesystem::path temporary = file.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << text;
    }
    std::error_code ec;
    std::filesystem::rename(temporary, file, ec);
}

std::vector<std::string> read_lines(const std::filesystem::path& file)
{
    std::vector<std::string> lines;
    std::ifstream in(file);
    for (std::string line; std::getline(in, line);)
        lines.push_back(line);
    return lines;
}

bool role_taken_logged = false;  // [DeviceLink.md#role] the daemon retries every minute

int accept_any_client_certificate(int, X509_STORE_CTX*)  // [DeviceLink.md#client-certificates]
{
    return 1;
}

} // namespace

DeviceLink& DeviceLink::instance()
{
    static DeviceLink link;
    return link;
}

DeviceLink::~DeviceLink()
{
    stop();
}

void DeviceLink::use_private_scope()
{
    private_scope = true;
}

std::string DeviceLink::fingerprint() const
{
    return identity_ ? identity_->fingerprint() : std::string();
}

bool DeviceLink::take_role()  // [DeviceLink.md#role]
{
#ifdef _WIN32
    void* handle = userobject::acquire_mutex(role_name());
    if (handle == nullptr)
        return false;
    role_handle_ = reinterpret_cast<std::intptr_t>(handle);
    return true;
#else
    const int fd = open(role_path(dir_).c_str(), O_RDWR | O_CREAT, 0600);
    if (fd < 0)
        return false;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return false;
    }
    role_handle_ = fd;
    return true;
#endif
}

void DeviceLink::release_role()
{
    if (role_handle_ < 0)
        return;
#ifdef _WIN32
    HANDLE handle = reinterpret_cast<HANDLE>(role_handle_);
    ReleaseMutex(handle);
    CloseHandle(handle);
#else
    flock(static_cast<int>(role_handle_), LOCK_UN);
    ::close(static_cast<int>(role_handle_));
#endif
    role_handle_ = -1;
}

bool DeviceLink::listen_on(uint16_t port)  // [DeviceLink.md#port]
{
    const Socket s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == NO_SOCKET)
        return false;
    const int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof on);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(private_scope ? INADDR_LOOPBACK : INADDR_ANY);  // [DeviceLink.md#loopback-in-tests]
    address.sin_port = htons(port);
    if (bind(s, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0 && port != 0) {
        address.sin_port = 0;
        if (bind(s, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
            close_socket(s);
            return false;
        }
    }
    socklen_t length = sizeof address;
    if (listen(s, 5) != 0 || getsockname(s, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        close_socket(s);
        return false;
    }
    port_ = ntohs(address.sin_port);
    listener_ = static_cast<std::intptr_t>(s);
    return true;
}

bool DeviceLink::prepare_tls()  // [DeviceLink.md#tls]
{
    ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ctx_)
        return false;
    SSL_CTX_set_min_proto_version(ctx_, TLS1_3_VERSION);
    BIO* certificate_bio = BIO_new_mem_buf(identity_->certificate_pem().data(), static_cast<int>(identity_->certificate_pem().size()));
    X509* certificate = PEM_read_bio_X509(certificate_bio, nullptr, nullptr, nullptr);
    BIO_free(certificate_bio);
    BIO* key_bio = BIO_new_mem_buf(identity_->key_pem().data(), static_cast<int>(identity_->key_pem().size()));
    EVP_PKEY* key = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
    BIO_free(key_bio);
    const bool ok = certificate && key && SSL_CTX_use_certificate(ctx_, certificate) == 1 && SSL_CTX_use_PrivateKey(ctx_, key) == 1;
    X509_free(certificate);
    EVP_PKEY_free(key);
    if (!ok)
        return false;
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, accept_any_client_certificate);
    return true;
}

std::filesystem::path DeviceLink::default_dir()  // [DeviceLink.md#shared-files]
{
    return std::filesystem::path(Config::get_root_pathname()).parent_path();
}

bool DeviceLink::start(const std::filesystem::path& dir, uint16_t port, uint16_t discovery_port)  // [DeviceLink.md#start]
{
    if (running_)
        return true;
    dir_ = dir;
    pairing_.set_dir(dir);  // the pairing works through files even when another process listens
    identity_ = DeviceIdentity::load_or_create(dir);
    if (!identity_)
        return false;
    if (!take_role()) {
        if (!std::exchange(role_taken_logged, true))
            log_info(translation::translate("device_link_role_taken"));
        return false;
    }
    role_taken_logged = false;
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif
    if (!prepare_tls() || !listen_on(port)) {
        log_error(translation::translate("device_link_listen_failed"), std::to_wstring(port));
        stop();
        return false;
    }
    if (!open_discovery(discovery_port))
        log_warning(translation::translate("device_discovery_unavailable", {std::to_wstring(discovery_port)}));
    write_file(dir_ / PORT_FILE, std::to_string(port_) + "\n");
    stop_ = false;
    running_ = true;
    thread_ = std::thread(&DeviceLink::accept_loop, this);
    log_info(translation::translate("device_link_listening", {std::to_wstring(port_)}));
    return true;
}

bool DeviceLink::hosted_elsewhere() const  // [DeviceLink.md#shared-files]
{
    if (running_ || dir_.empty())
        return false;
    const auto lines = read_lines(dir_ / PORT_FILE);
    return !lines.empty() && std::atoi(lines.front().c_str()) > 0;
}

bool DeviceLink::open_discovery(uint16_t port)  // [DeviceLink.md#discovery]
{
    const Socket s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == NO_SOCKET)
        return false;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    socklen_t length = sizeof address;
    if (bind(s, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0 || getsockname(s, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        close_socket(s);
        return false;
    }
    discovery_ = static_cast<std::intptr_t>(s);
    discovery_port_ = ntohs(address.sin_port);
    return true;
}

void DeviceLink::answer_discovery()  // [DeviceLink.md#discovery]
{
    char buffer[256];
    sockaddr_in peer{};
    socklen_t length = sizeof peer;
    const auto n = recvfrom(static_cast<Socket>(discovery_), buffer, sizeof buffer - 1, 0, reinterpret_cast<sockaddr*>(&peer), &length);
    if (n <= 0)
        return;
    const std::string request(buffer, static_cast<size_t>(n));
    if (request.rfind(DISCOVER, 0) != 0 || request.substr(std::strlen(DISCOVER), 64) != fingerprint())
        return;  // only a phone that already knows this PC gets an answer
    const std::string reply = HERE + fingerprint() + " " + std::to_string(port_);
    sendto(static_cast<Socket>(discovery_), reply.data(), static_cast<int>(reply.size()), 0, reinterpret_cast<sockaddr*>(&peer), length);
}

void DeviceLink::stop()
{
    stop_ = true;
    if (thread_.joinable())
        thread_.join();
    if (listener_ >= 0) {
        close_socket(static_cast<Socket>(listener_));
        listener_ = -1;
    }
    if (discovery_ >= 0) {
        close_socket(static_cast<Socket>(discovery_));
        discovery_ = -1;
    }
    if (running_ && !dir_.empty()) {
        std::error_code ec;
        std::filesystem::remove(dir_ / PORT_FILE, ec);
        std::filesystem::remove(dir_ / PRESENCE_FILE, ec);
    }
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
    release_role();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_items_.clear();
        presence_.clear();  // presence describes this run of the link [Device.md#DevicePresence]
    }
    running_ = false;
    port_ = 0;
    discovery_port_ = 0;
}

void DeviceLink::accept_loop()
{
    const Socket listener = static_cast<Socket>(listener_);
    const Socket discovery = static_cast<Socket>(discovery_);
    while (!stop_) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(listener, &set);
        if (discovery_ >= 0)
            FD_SET(discovery, &set);
        timeval wait{0, ACCEPT_POLL_MILLISECONDS * 1000};
        const int highest = static_cast<int>(discovery_ >= 0 && discovery > listener ? discovery : listener);
        if (select(highest + 1, &set, nullptr, nullptr, &wait) <= 0)
            continue;
        if (discovery_ >= 0 && FD_ISSET(discovery, &set))
            answer_discovery();
        if (!FD_ISSET(listener, &set))
            continue;
        sockaddr_in peer{};
        socklen_t length = sizeof peer;
        const Socket client = accept(listener, reinterpret_cast<sockaddr*>(&peer), &length);
        if (client == NO_SOCKET)
            continue;
        char text[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &peer.sin_addr, text, sizeof text);
        session(static_cast<std::intptr_t>(client), text);  // [DeviceLink.md#one-session-at-a-time]
    }
}

namespace {

bool read_line(SSL* ssl, std::string& line)
{
    char chunk[4096];
    while (line.find('\n') == std::string::npos) {
        const int n = SSL_read(ssl, chunk, sizeof chunk);
        if (n <= 0 || line.size() + static_cast<size_t>(n) > LINE_LIMIT)
            return false;
        line.append(chunk, static_cast<size_t>(n));
    }
    line.erase(line.find('\n'));
    return true;
}

bool write_all(SSL* ssl, const std::string& text)
{
    for (size_t done = 0; done < text.size();) {
        const int n = SSL_write(ssl, text.data() + done, static_cast<int>(text.size() - done));
        if (n <= 0)
            return false;
        done += static_cast<size_t>(n);
    }
    return true;
}

} // namespace

void DeviceLink::session(std::intptr_t client_handle, const std::string& peer_address)
{
    const Socket client = static_cast<Socket>(client_handle);
    set_timeouts(client, HANDSHAKE_TIMEOUT_SECONDS);  // [DeviceLink.md#one-session-at-a-time]
    SSL* ssl = SSL_new(ctx_);
    SSL_set_fd(ssl, static_cast<int>(client));
    if (SSL_accept(ssl) == 1) {
        X509* peer = SSL_get1_peer_certificate(ssl);
        const std::string peer_fingerprint = certificate_fingerprint(peer);
        X509_free(peer);
        std::string line;
        if (read_line(ssl, line)) {
            set_timeouts(client, SESSION_TIMEOUT_SECONDS);
            handle(line, peer_fingerprint, peer_address, [ssl](const std::string& text) { return write_all(ssl, text); });
        }
        SSL_shutdown(ssl);
    } else {
        char reason[256] = {};
        ERR_error_string_n(ERR_get_error(), reason, sizeof reason);
        log_debug(translation::translate("device_link_handshake_failed"), string_to_wstring(peer_address + " " + reason));
    }
    SSL_free(ssl);
    close_socket(client);
}

void DeviceLink::handle(const std::string& line, const std::string& peer_fingerprint, const std::string& peer_address, const Send& send)  // [DeviceLink.md#protocol]
{
    const auto request = parse(line);
    if (!request) {
        send(error_line("bad_request"));
        return;
    }
    const std::string type = request->get<std::string>("type", "");
    if (peer_fingerprint.empty()) {
        send(error_line("certificate_missing"));
        return;
    }
    if (type == "pair") {
        PairRequest pair_request;
        pair_request.token = request->get<std::string>("token", "");
        pair_request.device.id = request->get<std::string>("device.id", "");
        pair_request.device.name = request->get<std::string>("device.name", "");
        pair_request.device.ssh_host_key = request->get<std::string>("device.sshHostKey", "");
        pair_request.device.cert_fingerprint = peer_fingerprint;
        pair_request.sftp_port = port_of(*request, "device.sftpPort");
        pair_request.app_version = request->get<std::string>("appVersion", "");
        send(pair(pair_request, peer_fingerprint, peer_address));
        return;
    }
    if (type == "hello") {
        HelloRequest hello_request;
        hello_request.device_id = request->get<std::string>("deviceId", "");
        hello_request.sftp_port = port_of(*request, "sftpPort");
        hello_request.sftp_user = request->get<std::string>("sftpUser", "");
        hello_request.sftp_password = request->get<std::string>("sftpPassword", "");
        hello_request.serving = request->get<std::string>("serving", "false") == "true";
        hello_request.app_version = request->get<std::string>("appVersion", "");
        hello(hello_request, peer_fingerprint, peer_address, send);
        return;
    }
    send(error_line("bad_request"));
}

std::string DeviceLink::pair(const PairRequest& request, const std::string& peer_fingerprint, const std::string& peer_address)  // [DeviceLink.md#pair]
{
    if (!pairing_.open()) {
        log_warning(translation::translate("device_pairing_rejected", {string_to_wstring(peer_address), L"pairing_closed"}));
        return error_line("pairing_closed");
    }
    if (!valid_device_id(request.device.id) || !valid_device_name(request.device.name))
        return error_line("bad_request");
    if (!pairing_.redeem(request.token)) {
        log_warning(translation::translate("device_pairing_rejected", {string_to_wstring(peer_address), L"token_invalid"}));
        return error_line("token_invalid");
    }
    Device device = request.device;
    Config& config = Config::instance();
    if (config.find_device(device.id))
        config.update_device(device);  // [DeviceLink.md#repaired-device]
    else
        config.add(device);
    write_file(dir_ / PAIRED_FILE, device.id + "\n" + device.name + "\n" + device.cert_fingerprint + "\n" + device.ssh_host_key + "\n");  // [DeviceLink.md#shared-files]
    {
        std::lock_guard<std::mutex> lock(mutex_);
        presence_[device.id] = DevicePresence{peer_address, request.sftp_port, "", "", request.app_version, std::time(nullptr), false};
    }
    write_presence_file();
    log_info(translation::translate("device_paired", {string_to_wstring(device.name), string_to_wstring(peer_address)}));
    return "{\"type\":\"paired\",\"pc\":" + pc_json() + "}\n";
}

namespace {

constexpr std::chrono::minutes PASS_LIMIT{60};  // [DeviceLink.md#serve]

std::string items_line(const std::vector<DeviceItem>& items)
{
    std::string json = "{\"type\":\"items\",\"pc\":" + pc_json() + ",\"items\":[";
    for (size_t i = 0; i < items.size(); ++i)
        json += (i ? "," : "") + std::string("{\"name\":") + json_string(util::wstring_to_string(items[i].name)) + ",\"direction\":" + json_string(items[i].direction)
            + ",\"enabled\":" + (items[i].enabled ? "true" : "false") + "}";
    return json + "]}\n";
}

std::string progress_line(size_t done, size_t total)
{
    return "{\"type\":\"progress\",\"done\":" + std::to_string(done) + ",\"total\":" + std::to_string(total) + "}\n";
}

std::string done_line(bool completed, size_t errors)
{
    return std::string("{\"type\":\"done\",\"completed\":") + (completed ? "true" : "false") + ",\"errors\":" + std::to_string(errors) + "}\n";
}

size_t errors_of(const std::vector<DeviceItem>& items)
{
    size_t errors = 0;
    for (const auto& item : items)
        if (const auto found = Config::instance().find_by_name(item.name); found && found->last_run())
            errors += found->last_run()->errors;
    return errors;
}

} // namespace

std::vector<DeviceItem> DeviceLink::items_of(const std::string& device_id)  // [DeviceLink.md#items]
{
    std::vector<DeviceItem> items;
    for (const SyncItem& i : Config::instance().syncitems()) {
        const bool source = i.ssh_source() && dynamic_cast<const SshPath*>(i.source_path())->account().name() == device_id;
        const bool target = i.ssh_target() && dynamic_cast<const SshPath*>(i.target_path())->account().name() == device_id;
        if (!source && !target)
            continue;
        items.push_back({i.name(), i.two_way_sync() || (source && target) ? "both" : source ? "toPc" : "toPhone", i.enabled()});
    }
    return items;
}

bool DeviceLink::serving(const std::string& device_id) const  // [Device.md#serving]
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = presence_.find(device_id);
    return found != presence_.end() && found->second.serving;
}

void DeviceLink::item_passed(const std::string&, const std::wstring& item_name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    pending_items_.erase(item_name);
    passed_.notify_all();
}

void DeviceLink::hello(const HelloRequest& request, const std::string& peer_fingerprint, const std::string& peer_address, const Send& send)  // [DeviceLink.md#hello]
{
    const auto device = Config::instance().find_device(request.device_id);
    if (!device || device->cert_fingerprint != peer_fingerprint) {
        log_warning(translation::translate("device_hello_rejected", {string_to_wstring(peer_address)}));
        send(error_line("unknown_device"));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        presence_[device->id] = DevicePresence{peer_address, request.sftp_port, request.sftp_user, request.sftp_password, request.app_version, std::time(nullptr), false};
    }
    write_presence_file();
    log_debug(translation::translate("device_hello", {string_to_wstring(device->name), string_to_wstring(peer_address)}));
    const std::vector<DeviceItem> items = items_of(device->id);
    if (!send(items_line(items)))
        return;
    const bool pass_wanted = request.serving && request.sftp_port != 0 && std::ranges::any_of(items, &DeviceItem::enabled);
    if (!pass_wanted) {
        send("{\"type\":\"idle\"}\n");
        return;
    }
    serve(*device, request, send);
}

void DeviceLink::serve(const Device& device, const HelloRequest& request, const Send& send)  // [DeviceLink.md#serve]
{
    std::vector<DeviceItem> items = items_of(device.id);
    std::erase_if(items, [](const DeviceItem& item) { return !item.enabled; });
    SshAccount account;
    account.set_name(device.id);
    account.set_host(presence(device.id)->address);
    account.set_port(request.sftp_port);
    account.set_user(request.sftp_user);
    account.set_password(request.sftp_password);
    account.set_host_key(device.ssh_host_key);
    Config::instance().set_device_account(account);  // [Config.md#device-accounts]
    {
        std::lock_guard<std::mutex> lock(mutex_);
        presence_[device.id].serving = true;
        pending_items_.clear();
        for (const auto& item : items)
            pending_items_.insert(item.name);
    }
    log_info(translation::translate("device_pass_started", {string_to_wstring(device.name), std::to_wstring(items.size())}));
    send("{\"type\":\"serve\"}\n");
    const bool completed = wait_for_pass(device.id, items, send);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        presence_[device.id].serving = false;
        presence_[device.id].sftp_password.clear();
        pending_items_.clear();
    }
    Config::instance().clear_device_account(device.id);
    const size_t errors = errors_of(items);
    log_info(translation::translate(completed ? "device_pass_finished" : "device_pass_incomplete", {string_to_wstring(device.name), std::to_wstring(errors)}));
    send(done_line(completed, errors));
}

bool DeviceLink::wait_for_pass(const std::string& device_id, const std::vector<DeviceItem>& items, const Send& send)
{
    const auto deadline = std::chrono::steady_clock::now() + PASS_LIMIT;
    const bool own_run = !Synchronize::has_instance();  // [DeviceLink.md#serve]
    if (own_run)
        Synchronize::create(SyncType::Single, nullptr, SyncRole::Foreground, false, device_id);
    else
        Synchronize::wake();
    size_t reported = 0;
    std::unique_lock<std::mutex> lock(mutex_);
    while (!pending_items_.empty() && !stop_ && std::chrono::steady_clock::now() < deadline) {
        passed_.wait_for(lock, std::chrono::seconds(1));
        const size_t done = items.size() - pending_items_.size();
        if (done != reported) {
            reported = done;
            lock.unlock();
            send(progress_line(done, items.size()));
            lock.lock();
        }
        if (own_run && Synchronize::has_instance() && Synchronize::instance().finished())
            break;
    }
    const bool completed = pending_items_.empty();
    lock.unlock();
    if (own_run)
        Synchronize::destroy();
    return completed;
}

PairingOffer DeviceLink::offer()  // [DeviceLink.md#offer]
{
    PairingOffer offer;
    offer.pc_name = pc_name();
    offer.addresses = local_ipv4_addresses();
    if (running_)
        offer.port = port_;
    else if (const auto lines = read_lines(dir_ / PORT_FILE); !lines.empty())
        offer.port = static_cast<uint16_t>(std::atoi(lines.front().c_str()));  // [DeviceLink.md#shared-files]
    offer.fingerprint = fingerprint();
    offer.token = pairing_.token().empty() ? pairing_.begin() : pairing_.token();
    offer.expires = pairing_.expires();
    return offer;
}

std::optional<Device> DeviceLink::take_paired()  // [DeviceLink.md#take_paired]
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (dir_.empty())
        return std::nullopt;
    const auto lines = read_lines(dir_ / PAIRED_FILE);
    if (lines.size() < 4)
        return std::nullopt;
    std::error_code ec;
    std::filesystem::remove(dir_ / PAIRED_FILE, ec);
    Device device{lines[0], lines[1], lines[2], lines[3]};
    if (!valid_device_id(device.id))
        return std::nullopt;
    if (!Config::instance().find_device(device.id))
        Config::instance().add(device);  // the device paired in another process [DeviceLink.md#shared-files]
    else
        Config::instance().update_device(device);
    return device;
}

void DeviceLink::write_presence_file() const  // [DeviceLink.md#shared-files]
{
    std::string text;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, presence] : presence_)
            text += id + " " + presence.address + " " + std::to_string(presence.sftp_port) + " " + std::to_string(presence.last_seen) + "\n";
    }
    if (!dir_.empty())
        write_file(dir_ / PRESENCE_FILE, text);
}

std::optional<DevicePresence> DeviceLink::presence(const std::string& device_id) const
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = presence_.find(device_id);
        if (found != presence_.end())
            return found->second;
    }
    if (running_ || dir_.empty())
        return std::nullopt;
    for (const auto& line : read_lines(dir_ / PRESENCE_FILE)) {  // another process hosts the link
        std::istringstream in(line);
        std::string id;
        DevicePresence presence;
        long long seen = 0;
        if (in >> id >> presence.address >> presence.sftp_port >> seen && id == device_id) {
            presence.last_seen = static_cast<std::time_t>(seen);
            return presence;
        }
    }
    return std::nullopt;
}

} // namespace AFS_Core
