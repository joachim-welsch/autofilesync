#ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include "SshDirInspector.h"
#include "SshHostKeyVerification.h"
#include "SshConnectTimeout.h"
#include "Libssh2Init.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <format>
#include <cstring>

#ifdef _UNIX
    #include <libssh/sftp.h>
    #include <sys/stat.h>
#endif

namespace AFS_Core {

#ifdef _UNIX
namespace {
    bool is_dot_entry(const char* name)
    {
        return std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0;
    }

    std::string joined(const std::string& dir, const std::string& name)
    {
        return dir.empty() || dir.back() == '/' ? dir + name : dir + '/' + name;
    }

    RemoteDirEntry to_entry(const sftp_attributes_struct& attr, std::string name)
    {
        RemoteDirEntry entry{};
        entry.name = std::move(name);
        entry.is_directory = S_ISDIR(attr.permissions);
        entry.is_regular_file = S_ISREG(attr.permissions);
        entry.is_symlink = S_ISLNK(attr.permissions);
        entry.size_bytes = attr.size;
        entry.raw_mtime = attr.mtime;
        entry.permissions = attr.permissions;
        entry.last_modified_iso = std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::from_time_t(attr.mtime));
        return entry;
    }

    bool require_directory(const SftpConnection& connection, const std::string& remote_path, RemoteDirInfo& info)  // [SshDirInspector.md#require_directory]
    {
        info.path = remote_path;
        const auto attr = connection.stat(remote_path);
        if (!attr) {
            info.error_message = "Pfad existiert nicht: " + remote_path;
            return false;
        }
        info.exists = true;
        info.is_directory = S_ISDIR(attr->permissions);
        if (!info.is_directory)
            info.error_message = "Pfad ist kein Verzeichnis: " + remote_path;
        return info.is_directory;
    }
}

SshDirInspector::SshDirInspector(const SshAccount& account)
    : connection_(account), account_(account)
{}

SshDirInspector::~SshDirInspector() = default;

RemoteDirInfo SshDirInspector::inspect_dir(const std::string& remote_path)
{
    RemoteDirInfo info{};
    if (!require_directory(connection_, remote_path, info))
        return info;
    const SftpDir dir = connection_.open_dir(remote_path);
    if (!dir) {
        info.error_message = "Fehler beim Öffnen des Verzeichnisses: " + connection_.error();
        return info;
    }
    while (const auto attr = connection_.read_dir(dir.get()))
        if (!is_dot_entry(attr->name))
            info.entries.push_back(to_entry(*attr, attr->name));
    if (!sftp_dir_eof(dir.get()))
        info.error_message = "Fehler beim Lesen des Verzeichnisses: " + connection_.error();
    return info;
}

std::string SshDirInspector::get_home_dir()
{
    char* resolved = sftp_canonicalize_path(connection_.sftp(), ".");
    if (!resolved)
        throw std::runtime_error("Home-Verzeichnis konnte nicht ermittelt werden: " + connection_.error());
    std::string home(resolved);
    ssh_string_free_char(resolved);
    return home;
}

RemoteDirInfo SshDirInspector::inspect_dir_recursive(const std::string& remote_path)
{
    RemoteDirInfo result{};
    if (require_directory(connection_, remote_path, result))
        inspect_dir_recursive_impl(remote_path, result);
    return result;
}

void SshDirInspector::inspect_dir_recursive_impl(const std::string& remote_path, RemoteDirInfo& result)
{
    const SftpDir dir = connection_.open_dir(remote_path);
    if (!dir) {
        result.error_message = "Fehler beim Öffnen des Verzeichnisses: " + remote_path;
        return;
    }
    while (const auto attr = connection_.read_dir(dir.get())) {
        if (is_dot_entry(attr->name))
            continue;
        const std::string full_path = joined(remote_path, attr->name);
        result.entries.push_back(to_entry(*attr, full_path));
        if (S_ISDIR(attr->permissions) && !S_ISLNK(attr->permissions))
            inspect_dir_recursive_impl(full_path, result);
    }
    if (!sftp_dir_eof(dir.get()))  // [SshDirInspector.md#readdir-eof-vs-error]
        result.error_message = "Verzeichnis unvollständig gelesen: " + remote_path;
}

#else

#ifdef _WIN32

constexpr bool has_file_type(unsigned long permissions, unsigned long type)
{
    return (permissions & LIBSSH2_SFTP_S_IFMT) == type;
}

SshDirInspector::SshDirInspector(const SshAccount& account)
    : account_(account)
{
    WSADATA wsadata;
    int rc = WSAStartup(MAKEWORD(2, 0), &wsadata);
    if (rc != 0) {
        throw std::runtime_error("WSAStartup fehlgeschlagen");
    }

    struct sockaddr_in sin;
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == INVALID_SOCKET) {
        WSACleanup();
        throw std::runtime_error("Fehler beim Erstellen des Sockets");
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    sin.sin_addr.s_addr = inet_addr(account_.host().c_str());
    if (sin.sin_addr.s_addr == INADDR_NONE) {
        struct hostent* host = gethostbyname(account_.host().c_str());
        if (!host) {
            closesocket(sock_);
            WSACleanup();
            throw std::runtime_error("Hostname konnte nicht aufgelöst werden: " + account_.host());
        }
        memcpy(&sin.sin_addr, host->h_addr, host->h_length);
    }

    if (connect(sock_, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
        closesocket(sock_);
        WSACleanup();
        throw std::runtime_error("Verbindung zum Server fehlgeschlagen");
    }

    ensure_libssh2_initialized();

    my_ssh_session_ = libssh2_session_init();
    if (!my_ssh_session_) {
        closesocket(sock_);
        WSACleanup();
        throw std::runtime_error("Fehler beim Erstellen der SSH Session");
    }
    apply_ssh_connect_timeout(my_ssh_session_);

    rc = libssh2_session_handshake(my_ssh_session_, sock_);
    if (rc != 0) {
        cleanup();
        throw std::runtime_error("SSH Handshake fehlgeschlagen");
    }

    if (!verify_ssh_host_key(my_ssh_session_, account_.host())) {
        cleanup();
        throw std::runtime_error("Host-Key-Prüfung fehlgeschlagen: mögliche Man-in-the-Middle-Attacke.");
    }

    rc = libssh2_userauth_password(my_ssh_session_, account_.user().c_str(), account_.password().c_str());
    if (rc != 0) {
        cleanup();
        throw std::runtime_error("Authentifizierung fehlgeschlagen");
    }

    sftp_session_ptr_ = libssh2_sftp_init(my_ssh_session_);
    if (!sftp_session_ptr_) {
        cleanup();
        throw std::runtime_error("SFTP Initialisierung fehlgeschlagen");
    }
}

SshDirInspector::~SshDirInspector()
{
    if (sftp_session_ptr_) {
        libssh2_sftp_shutdown(sftp_session_ptr_);
        sftp_session_ptr_ = nullptr;
    }
    cleanup();
}

void SshDirInspector::cleanup()
{
    if (my_ssh_session_) {
        libssh2_session_disconnect(my_ssh_session_, "Normal Shutdown");
        libssh2_session_free(my_ssh_session_);
        my_ssh_session_ = nullptr;
    }
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    WSACleanup();
}

RemoteDirInfo SshDirInspector::inspect_dir(const std::string& remote_path)
{
    RemoteDirInfo info{};
    info.exists = false;
    info.is_directory = false;
    info.path = remote_path;

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session_ptr_, remote_path.c_str(), &attrs);
    if (rc != 0) {
        info.error_message = "Pfad existiert nicht: " + remote_path;
        return info;
    }

    info.exists = true;
    info.is_directory = has_file_type(attrs.permissions, LIBSSH2_SFTP_S_IFDIR);

    if (!info.is_directory) {
        info.error_message = "Pfad ist kein Verzeichnis: " + remote_path;
        return info;
    }

    LIBSSH2_SFTP_HANDLE* dir_handle = libssh2_sftp_opendir(sftp_session_ptr_, remote_path.c_str());
    if (!dir_handle) {
        char* err_msg;
        libssh2_session_last_error(my_ssh_session_, &err_msg, nullptr, 0);
        info.error_message = "Fehler beim Öffnen des Verzeichnisses: " + std::string(err_msg);
        return info;
    }

    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES entry_attrs;
    int read_rc = 0;

    while ((read_rc = libssh2_sftp_readdir(dir_handle, buffer, sizeof(buffer), &entry_attrs)) > 0) {
        std::string name = buffer;

        if (name == "." || name == "..") {
            continue;
        }

        RemoteDirEntry entry{};
        entry.name = name;
        entry.is_directory = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFDIR);
        entry.is_regular_file = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFREG);
        entry.is_symlink = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFLNK);
        entry.size_bytes = entry_attrs.filesize;
        entry.raw_mtime = static_cast<time_t>(entry_attrs.mtime);
        entry.permissions = static_cast<uint32_t>(entry_attrs.permissions);

        auto tp = std::chrono::system_clock::from_time_t(entry.raw_mtime);
        entry.last_modified_iso = std::format("{:%Y-%m-%d %H:%M:%S}", tp);

        info.entries.push_back(entry);
    }

    if (read_rc < 0)  // 0 is EOF, negative means an error in the middle of the listing
        info.error_message = "Verzeichnis unvollständig gelesen: " + remote_path;

    libssh2_sftp_closedir(dir_handle);
    return info;
}

std::string SshDirInspector::get_home_dir()
{
    char buffer[1024];
    int rc = libssh2_sftp_realpath(sftp_session_ptr_, ".", buffer, sizeof(buffer));
    if (rc < 0) {
        char* err_msg;
        libssh2_session_last_error(my_ssh_session_, &err_msg, nullptr, 0);
        throw std::runtime_error("Home-Verzeichnis konnte nicht ermittelt werden: " + std::string(err_msg));
    }
    return std::string(buffer, rc);
}

RemoteDirInfo SshDirInspector::inspect_dir_recursive(const std::string& remote_path)
{
    RemoteDirInfo result{};
    result.path = remote_path;
    result.exists = false;
    result.is_directory = false;

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session_ptr_, remote_path.c_str(), &attrs);
    if (rc != 0) {
        result.error_message = "Pfad existiert nicht: " + remote_path;
        return result;
    }

    result.exists = true;
    result.is_directory = has_file_type(attrs.permissions, LIBSSH2_SFTP_S_IFDIR);

    if (!result.is_directory) {
        result.error_message = "Pfad ist kein Verzeichnis: " + remote_path;
        return result;
    }

    inspect_dir_recursive_impl(remote_path, result);
    return result;
}

void SshDirInspector::inspect_dir_recursive_impl(const std::string& remote_path, RemoteDirInfo& result)
{
    LIBSSH2_SFTP_HANDLE* dir_handle = libssh2_sftp_opendir(sftp_session_ptr_, remote_path.c_str());
    if (!dir_handle) {
        result.error_message = "Fehler beim Öffnen des Verzeichnisses: " + remote_path;
        return;
    }

    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES entry_attrs;
    int read_rc = 0;

    while ((read_rc = libssh2_sftp_readdir(dir_handle, buffer, sizeof(buffer), &entry_attrs)) > 0) {
        std::string name = buffer;

        if (name == "." || name == "..") {
            continue;
        }

        std::string full_path = remote_path;
        if (!full_path.empty() && full_path.back() != '/') {
            full_path += '/';
        }
        full_path += name;

        RemoteDirEntry entry{};
        entry.name = full_path;
        entry.is_directory = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFDIR);
        entry.is_regular_file = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFREG);
        entry.is_symlink = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFLNK);
        entry.size_bytes = entry_attrs.filesize;
        entry.raw_mtime = static_cast<time_t>(entry_attrs.mtime);
        entry.permissions = static_cast<uint32_t>(entry_attrs.permissions);

        auto tp = std::chrono::system_clock::from_time_t(entry.raw_mtime);
        entry.last_modified_iso = std::format("{:%Y-%m-%d %H:%M:%S}", tp);

        result.entries.push_back(entry);

        if (entry.is_directory && !entry.is_symlink) {
            inspect_dir_recursive_impl(full_path, result);
        }
    }

    if (read_rc < 0)  // 0 is EOF, negative means an error in the middle of the listing
        result.error_message = "Verzeichnis unvollständig gelesen: " + remote_path;

    libssh2_sftp_closedir(dir_handle);
}

#endif // _WIN32
#endif // _UNIX

} // namespace AFS_Core
