/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Android, the Android app of AutoFileSync.
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
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <ctime>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr size_t BLOCK = 65536;  // same request size as SyncFileSsh
const std::string BASE = "AFS-Probe";

struct Options
{
    std::string backend, host, user, password;
    int port = 52382;
    size_t megabytes = 32;
};

struct Stat
{
    bool ok = false;
    uint64_t size = 0;
    uint64_t mtime = 0;
    uint32_t permissions = 0;
};

class Sftp
{
public:
    virtual ~Sftp() = default;
    virtual std::string fingerprint() = 0;
    virtual std::string home() = 0;
    virtual bool mkdir(const std::string& path) = 0;
    virtual bool rmdir(const std::string& path) = 0;
    virtual bool unlink(const std::string& path) = 0;
    virtual bool rename(const std::string& from, const std::string& to) = 0;
    virtual bool set_time(const std::string& path, uint64_t mtime) = 0;
    virtual bool set_permissions(const std::string& path, uint32_t permissions) = 0;
    virtual Stat stat(const std::string& path) = 0;
    virtual std::vector<std::string> list(const std::string& path) = 0;
    virtual bool write(const std::string& path, const std::vector<char>& data) = 0;
    virtual bool read(const std::string& path, std::vector<char>& data) = 0;
    virtual std::string error() = 0;
};

class Libssh : public Sftp
{
    static constexpr size_t IN_FLIGHT = 16;
    ssh_session ssh_ = ssh_new();
    sftp_session sftp_ = nullptr;
    bool pipelined_ = false;
public:
    explicit Libssh(const Options& o) : pipelined_(o.backend == "libssh-aio")
    {
        const unsigned port = static_cast<unsigned>(o.port);
        ssh_options_set(ssh_, SSH_OPTIONS_HOST, o.host.c_str());
        ssh_options_set(ssh_, SSH_OPTIONS_PORT, &port);
        ssh_options_set(ssh_, SSH_OPTIONS_USER, o.user.c_str());
        if (ssh_connect(ssh_) != SSH_OK)
            throw std::runtime_error("connect: " + error());
        if (ssh_userauth_password(ssh_, nullptr, o.password.c_str()) != SSH_AUTH_SUCCESS)
            throw std::runtime_error("auth: " + error());
        sftp_ = sftp_new(ssh_);
        if (!sftp_ || sftp_init(sftp_) != SSH_OK)
            throw std::runtime_error("sftp_init: " + error());
    }
    ~Libssh() override
    {
        if (sftp_)
            sftp_free(sftp_);
        ssh_disconnect(ssh_);
        ssh_free(ssh_);
    }
    std::string fingerprint() override
    {
        ssh_key key = nullptr;
        ssh_get_server_publickey(ssh_, &key);
        unsigned char* hash = nullptr;
        size_t length = 0;
        ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &length);
        char* text = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, length);
        std::string result = std::string(ssh_key_type_to_char(ssh_key_type(key))) + " " + text;
        ssh_string_free_char(text);
        ssh_clean_pubkey_hash(&hash);
        ssh_key_free(key);
        return result;
    }
    std::string home() override
    {
        char* path = sftp_canonicalize_path(sftp_, ".");
        std::string result = path ? path : "?";
        ssh_string_free_char(path);
        return result;
    }
    bool mkdir(const std::string& p) override { return sftp_mkdir(sftp_, p.c_str(), 0755) == SSH_OK; }
    bool rmdir(const std::string& p) override { return sftp_rmdir(sftp_, p.c_str()) == SSH_OK; }
    bool unlink(const std::string& p) override { return sftp_unlink(sftp_, p.c_str()) == SSH_OK; }
    bool rename(const std::string& f, const std::string& t) override { return sftp_rename(sftp_, f.c_str(), t.c_str()) == SSH_OK; }
    bool set_time(const std::string& p, uint64_t mtime) override
    {
        sftp_attributes_struct a{};
        a.flags = SSH_FILEXFER_ATTR_ACMODTIME;
        a.atime = static_cast<uint32_t>(mtime);
        a.mtime = static_cast<uint32_t>(mtime);
        return sftp_setstat(sftp_, p.c_str(), &a) == SSH_OK;
    }
    bool set_permissions(const std::string& p, uint32_t permissions) override
    {
        sftp_attributes_struct a{};
        a.flags = SSH_FILEXFER_ATTR_PERMISSIONS;
        a.permissions = permissions;
        return sftp_setstat(sftp_, p.c_str(), &a) == SSH_OK;
    }
    Stat stat(const std::string& p) override
    {
        sftp_attributes a = sftp_stat(sftp_, p.c_str());
        if (!a)
            return {};
        Stat s{true, a->size, a->mtime, a->permissions};
        sftp_attributes_free(a);
        return s;
    }
    std::vector<std::string> list(const std::string& p) override
    {
        std::vector<std::string> names;
        sftp_dir dir = sftp_opendir(sftp_, p.c_str());
        if (!dir)
            return names;
        while (sftp_attributes a = sftp_readdir(sftp_, dir)) {
            names.push_back(a->name);
            sftp_attributes_free(a);
        }
        if (!sftp_dir_eof(dir))
            names.push_back("<readdir error>");
        sftp_closedir(dir);
        return names;
    }
    bool write(const std::string& p, const std::vector<char>& data) override
    {
        sftp_file f = sftp_open(sftp_, p.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (!f)
            return false;
        if (pipelined_)
            return write_pipelined(f, data) & (sftp_close(f) == SSH_OK);
        for (size_t done = 0; done < data.size();) {
            const ssize_t n = sftp_write(f, data.data() + done, std::min(BLOCK, data.size() - done));
            if (n < 0) {
                sftp_close(f);
                return false;
            }
            done += static_cast<size_t>(n);
        }
        return sftp_close(f) == SSH_OK;
    }
    bool read(const std::string& p, std::vector<char>& data) override
    {
        sftp_file f = sftp_open(sftp_, p.c_str(), O_RDONLY, 0);
        if (!f)
            return false;
        if (pipelined_) {
            const bool ok = read_pipelined(f, data);
            sftp_close(f);
            return ok;
        }
        std::vector<char> buffer(BLOCK);
        ssize_t n;
        while ((n = sftp_read(f, buffer.data(), buffer.size())) > 0)
            data.insert(data.end(), buffer.begin(), buffer.begin() + n);
        sftp_close(f);
        return n == 0;
    }
    bool write_pipelined(sftp_file f, const std::vector<char>& data)
    {
        std::deque<sftp_aio> pending;
        bool ok = true;
        for (size_t done = 0; ok && (done < data.size() || !pending.empty());) {
            while (done < data.size() && pending.size() < IN_FLIGHT) {
                sftp_aio aio = nullptr;
                const ssize_t n = sftp_aio_begin_write(f, data.data() + done, std::min(BLOCK, data.size() - done), &aio);
                if (n < 0)
                    return false;
                done += static_cast<size_t>(n);
                pending.push_back(aio);
            }
            ok = sftp_aio_wait_write(&pending.front()) >= 0;
            pending.pop_front();
        }
        for (auto& aio : pending)
            sftp_aio_free(aio);
        return ok;
    }
    bool read_pipelined(sftp_file f, std::vector<char>& data)
    {
        std::deque<sftp_aio> pending;
        std::vector<char> buffer(BLOCK);
        bool eof = false;
        while (!eof || !pending.empty()) {
            while (!eof && pending.size() < IN_FLIGHT) {
                sftp_aio aio = nullptr;
                if (sftp_aio_begin_read(f, BLOCK, &aio) < 0)
                    return false;
                pending.push_back(aio);
            }
            const ssize_t n = sftp_aio_wait_read(&pending.front(), buffer.data(), buffer.size());
            pending.pop_front();
            if (n < 0)
                return false;
            eof = eof || n == 0;
            data.insert(data.end(), buffer.begin(), buffer.begin() + n);
        }
        return true;
    }
    std::string error() override
    {
        std::string e = ssh_get_error(ssh_);
        if (sftp_)
            e += " (sftp " + std::to_string(sftp_get_error(sftp_)) + ")";
        return e;
    }
};

class Libssh2 : public Sftp
{
    int socket_ = -1;
    LIBSSH2_SESSION* ssh_ = nullptr;
    LIBSSH2_SFTP* sftp_ = nullptr;
public:
    explicit Libssh2(const Options& o)
    {
        libssh2_init(0);
        addrinfo hints{}, *found = nullptr;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(o.host.c_str(), std::to_string(o.port).c_str(), &hints, &found) != 0)
            throw std::runtime_error("resolve failed");
        socket_ = ::socket(found->ai_family, SOCK_STREAM, 0);
        const int rc = ::connect(socket_, found->ai_addr, found->ai_addrlen);
        freeaddrinfo(found);
        if (rc != 0)
            throw std::runtime_error("tcp connect failed");
        ssh_ = libssh2_session_init();
        libssh2_session_set_blocking(ssh_, 1);
        if (libssh2_session_handshake(ssh_, socket_) != 0)
            throw std::runtime_error("handshake: " + error());
        if (libssh2_userauth_password(ssh_, o.user.c_str(), o.password.c_str()) != 0)
            throw std::runtime_error("auth: " + error());
        sftp_ = libssh2_sftp_init(ssh_);
        if (!sftp_)
            throw std::runtime_error("sftp_init: " + error());
    }
    ~Libssh2() override
    {
        if (sftp_)
            libssh2_sftp_shutdown(sftp_);
        if (ssh_) {
            libssh2_session_disconnect(ssh_, "bye");
            libssh2_session_free(ssh_);
        }
        if (socket_ >= 0)
            ::close(socket_);
        libssh2_exit();
    }
    std::string fingerprint() override
    {
        const auto* hash = reinterpret_cast<const unsigned char*>(libssh2_hostkey_hash(ssh_, LIBSSH2_HOSTKEY_HASH_SHA256));
        std::string hex;
        char byte[3];
        for (int i = 0; hash && i < 32; ++i) {
            std::snprintf(byte, sizeof byte, "%02x", hash[i]);
            hex += byte;
        }
        size_t length = 0;
        int type = 0;
        libssh2_session_hostkey(ssh_, &length, &type);
        return "type " + std::to_string(type) + " sha256 " + hex;
    }
    std::string home() override
    {
        char buffer[1024];
        const int n = libssh2_sftp_realpath(sftp_, ".", buffer, sizeof buffer);
        return n > 0 ? std::string(buffer, static_cast<size_t>(n)) : "?";
    }
    bool mkdir(const std::string& p) override { return libssh2_sftp_mkdir(sftp_, p.c_str(), 0755) == 0; }
    bool rmdir(const std::string& p) override { return libssh2_sftp_rmdir(sftp_, p.c_str()) == 0; }
    bool unlink(const std::string& p) override { return libssh2_sftp_unlink(sftp_, p.c_str()) == 0; }
    bool rename(const std::string& f, const std::string& t) override { return libssh2_sftp_rename(sftp_, f.c_str(), t.c_str()) == 0; }
    bool set_time(const std::string& p, uint64_t mtime) override
    {
        LIBSSH2_SFTP_ATTRIBUTES a{};
        a.flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
        a.atime = mtime;
        a.mtime = mtime;
        return libssh2_sftp_setstat(sftp_, p.c_str(), &a) == 0;
    }
    bool set_permissions(const std::string& p, uint32_t permissions) override
    {
        LIBSSH2_SFTP_ATTRIBUTES a{};
        a.flags = LIBSSH2_SFTP_ATTR_PERMISSIONS;
        a.permissions = permissions;
        return libssh2_sftp_setstat(sftp_, p.c_str(), &a) == 0;
    }
    Stat stat(const std::string& p) override
    {
        LIBSSH2_SFTP_ATTRIBUTES a{};
        if (libssh2_sftp_stat(sftp_, p.c_str(), &a) != 0)
            return {};
        return {true, a.filesize, a.mtime, static_cast<uint32_t>(a.permissions)};
    }
    std::vector<std::string> list(const std::string& p) override
    {
        std::vector<std::string> names;
        LIBSSH2_SFTP_HANDLE* dir = libssh2_sftp_opendir(sftp_, p.c_str());
        if (!dir)
            return names;
        char buffer[512];
        LIBSSH2_SFTP_ATTRIBUTES a;
        int n;
        while ((n = libssh2_sftp_readdir(dir, buffer, sizeof buffer, &a)) > 0)
            names.emplace_back(buffer, static_cast<size_t>(n));
        if (n < 0)
            names.push_back("<readdir error>");
        libssh2_sftp_closedir(dir);
        return names;
    }
    bool write(const std::string& p, const std::vector<char>& data) override
    {
        LIBSSH2_SFTP_HANDLE* f = libssh2_sftp_open(sftp_, p.c_str(),
            LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC, 0644);
        if (!f)
            return false;
        for (size_t done = 0; done < data.size();) {
            const ssize_t n = libssh2_sftp_write(f, data.data() + done, std::min(BLOCK, data.size() - done));
            if (n < 0) {
                libssh2_sftp_close(f);
                return false;
            }
            done += static_cast<size_t>(n);
        }
        return libssh2_sftp_close(f) == 0;
    }
    bool read(const std::string& p, std::vector<char>& data) override
    {
        LIBSSH2_SFTP_HANDLE* f = libssh2_sftp_open(sftp_, p.c_str(), LIBSSH2_FXF_READ, 0);
        if (!f)
            return false;
        std::vector<char> buffer(BLOCK);
        ssize_t n;
        while ((n = libssh2_sftp_read(f, buffer.data(), buffer.size())) > 0)
            data.insert(data.end(), buffer.begin(), buffer.begin() + n);
        libssh2_sftp_close(f);
        return n == 0;
    }
    std::string error() override
    {
        char* message = nullptr;
        libssh2_session_last_error(ssh_, &message, nullptr, 0);
        return std::string(message ? message : "") + " (sftp " + std::to_string(sftp_ ? libssh2_sftp_last_error(sftp_) : 0) + ")";
    }
};

int failures = 0;

void check(bool ok, const std::string& what, const std::string& detail = "")
{
    failures += ok ? 0 : 1;
    std::printf("%-4s %s%s%s\n", ok ? "OK" : "FAIL", what.c_str(), detail.empty() ? "" : "  ", detail.c_str());
}

double seconds_of(const std::function<bool()>& step, bool& ok)
{
    const auto start = std::chrono::steady_clock::now();
    ok = step();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

std::string octal(uint32_t value)
{
    char text[16];
    std::snprintf(text, sizeof text, "%o", value);
    return text;
}

void transfer(Sftp& s, const Options& o)
{
    std::vector<char> data(o.megabytes << 20);
    std::mt19937 random(42);
    for (auto& c : data)
        c = static_cast<char>(random());
    const std::string path = BASE + "/probe.bin";
    bool ok = false;
    const double up = seconds_of([&] { return s.write(path, data); }, ok);
    check(ok, "upload " + std::to_string(o.megabytes) + " MiB", std::to_string(o.megabytes / up) + " MiB/s " + (ok ? "" : s.error()));
    std::vector<char> back;
    const double down = seconds_of([&] { return s.read(path, back); }, ok);
    check(ok && back == data, "download and compare", std::to_string(o.megabytes / down) + " MiB/s");
}

void attributes(Sftp& s)
{
    const std::string path = BASE + "/probe.bin";
    const uint64_t target = static_cast<uint64_t>(std::time(nullptr)) - 3 * 86400;
    check(s.set_time(path, target), "setstat ACMODTIME", s.error());
    const Stat after_time = s.stat(path);
    check(after_time.mtime == target, "mtime read back", std::to_string(after_time.mtime) + " wanted " + std::to_string(target));
    const Stat before = s.stat(path);
    check(before.ok, "permissions reported", octal(before.permissions));
    check(s.set_permissions(path, 0640 | S_IWUSR), "setstat PERMISSIONS 0640", s.error());
    check(s.stat(path).mtime == target, "mtime survives chmod", std::to_string(s.stat(path).mtime));
    check(s.stat(BASE + "/missing").ok == false, "stat of missing file fails");
}

void renames(Sftp& s)
{
    check(s.rename(BASE + "/probe.bin", BASE + "/sub/moved.bin"), "rename into subdir", s.error());
    check(s.write(BASE + "/other.bin", std::vector<char>(10, 'x')), "small upload");
    const bool overwrite = s.rename(BASE + "/other.bin", BASE + "/sub/moved.bin");
    check(overwrite, "rename onto existing file (SFTP v3 may refuse)", overwrite ? "" : s.error());
    std::string names;
    for (const auto& n : s.list(BASE + "/sub"))
        names += n + " ";
    check(names.find("moved.bin") != std::string::npos, "readdir sub", names);
}

void cleanup(Sftp& s)
{
    for (const auto& name : s.list(BASE + "/sub"))
        if (name != "." && name != "..")
            s.unlink(BASE + "/sub/" + name);
    for (const auto& name : s.list(BASE))
        if (name != "." && name != ".." && name != "sub")
            s.unlink(BASE + "/" + name);
    check(s.rmdir(BASE + "/sub") && s.rmdir(BASE), "unlink + rmdir cleanup", s.error());
    check(!s.stat(BASE).ok, "probe directory gone");
}

std::unique_ptr<Sftp> connect(const Options& o)
{
    if (o.backend == "libssh2")
        return std::make_unique<Libssh2>(o);
    return std::make_unique<Libssh>(o);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5) {
        std::fprintf(stderr, "usage: %s libssh|libssh-aio|libssh2 host port password [megabytes]\n", argv[0]);
        return 2;
    }
    Options o{argv[1], argv[2], "afs", argv[4], std::stoi(argv[3])};
    if (argc > 5)
        o.megabytes = std::stoul(argv[5]);
    try {
        const auto s = connect(o);
        std::printf("backend %s, host key %s, home %s\n", o.backend.c_str(), s->fingerprint().c_str(), s->home().c_str());
        check(s->mkdir(BASE) && s->mkdir(BASE + "/sub"), "mkdir", s->error());
        transfer(*s, o);
        attributes(*s);
        renames(*s);
        cleanup(*s);
    } catch (const std::exception& e) {
        check(false, "session", e.what());
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
