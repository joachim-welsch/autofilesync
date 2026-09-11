#include "pch.h"
#include "SshFileInspector.h"
#include "SyncFileSsh.h"
#include "SshHostKeyVerification.h"
#include "SshConnectTimeout.h"
#include "Libssh2Init.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "Util.h"

#ifdef _UNIX
    #include <libssh/sftp.h>
#else
    #ifdef _WIN32
        #include <winsock2.h>
        #include <ws2tcpip.h>
        // MinGW: Link with -lws2_32 -lssh2 in Makefile/CMakeLists.txt
    #endif
#endif

namespace AFS_Core {

static std::string time_to_iso(time_t t)
{
    struct tm timeinfo;
    #ifdef _WIN32
        localtime_s(&timeinfo, &t);
    #else
        localtime_r(&t, &timeinfo);
    #endif
    
    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

#ifdef _UNIX
SshFileInspector::SshFileInspector(const SshAccount& account)
    : connection_(account), account_(account)
{}

SshFileInspector::~SshFileInspector() = default;

RemoteFileInfo SshFileInspector::inspect_file(const std::string& remote_path)
{
    RemoteFileInfo info{};
    info.last_modified_iso = "N/A";
    // Configured paths may use either separator (Settings.cpp stores them as
    // entered); the remote sftp server only understands slashes.
    const std::string sftp_path = win_to_sftp_path(remote_path);
    const auto attributes = connection_.stat(sftp_path);
    if (!attributes) {
        // Only a confirmed "no such file" may report exists=false: the caller
        // (read_ssh_file_info) treats that as safe-to-delete for delete_in_target.
        // Any other failure (permission denied, dropped session) must not be
        // mistaken for an absent file - throw into the caller's error path.
        const int err = sftp_get_error(connection_.sftp());
        if (err == SSH_FX_NO_SUCH_FILE || err == SSH_FX_NO_SUCH_PATH)
            return info;
        throw std::runtime_error("SFTP-Stat fehlgeschlagen (Fehlercode " + std::to_string(err) + "): " + sftp_path);
    }
    info.exists = true;
    info.size_bytes = attributes->size;
    info.raw_mtime = attributes->mtime;
    info.last_modified_iso = time_to_iso(info.raw_mtime);
    return info;
}

#else
#ifdef _WIN32
// Windows implementation on libssh2

static void init_winsock()
{
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsadata;
        int err = WSAStartup(MAKEWORD(2, 2), &wsadata);
        if (err != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
        initialized = true;
    }
}

SshFileInspector::SshFileInspector(const SshAccount& account) 
    : account_(account)
{
    
    init_winsock();
    ensure_libssh2_initialized();

    struct addrinfo hints = {}, *result = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(account.host().c_str(), "22", &hints, &result) != 0) {
        throw std::runtime_error("Failed to resolve hostname: " + account.host());
    }
    
    sock_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock_ == INVALID_SOCKET) {
        freeaddrinfo(result);
        throw std::runtime_error("Failed to create socket");
    }
    
    if (connect(sock_, result->ai_addr, (int)result->ai_addrlen) != 0) {
        freeaddrinfo(result);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        throw std::runtime_error("Failed to connect to host");
    }
    freeaddrinfo(result);
    
    my_ssh_session_ = libssh2_session_init();
    if (!my_ssh_session_) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        throw std::runtime_error("Failed to create SSH session");
    }
    apply_ssh_connect_timeout(my_ssh_session_);

    int rc = libssh2_session_handshake(my_ssh_session_, sock_);
    if (rc) {
        cleanup();
        throw std::runtime_error("SSH handshake failed");
    }

    if (!verify_ssh_host_key(my_ssh_session_, account.host())) {
        cleanup();
        throw std::runtime_error("Host key verification failed: possible man-in-the-middle attack.");
    }

    rc = libssh2_userauth_password(my_ssh_session_, 
                                   account.user().c_str(), 
                                   account.password().c_str());
    if (rc) {
        cleanup();
        throw std::runtime_error("Authentication failed");
    }
    
    sftp_session_ptr_ = libssh2_sftp_init(my_ssh_session_);
    if (!sftp_session_ptr_) {
        cleanup();
        throw std::runtime_error("Failed to initialize SFTP session");
    }
}

SshFileInspector::~SshFileInspector()
{
    cleanup();
}

void SshFileInspector::cleanup()
{
    if (sftp_session_ptr_) {
        libssh2_sftp_shutdown(sftp_session_ptr_);
        sftp_session_ptr_ = nullptr;
    }
    if (my_ssh_session_) {
        libssh2_session_disconnect(my_ssh_session_, "Normal shutdown");
        libssh2_session_free(my_ssh_session_);
        my_ssh_session_ = nullptr;
    }
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
}

RemoteFileInfo SshFileInspector::inspect_file(const std::string& remote_path)
{
    RemoteFileInfo info{};
    info.exists = false;
    info.size_bytes = 0;
    info.raw_mtime = 0;
    info.last_modified_iso = "N/A";
    
    if (!sftp_session_ptr_) {
        return info;
    }
    auto sftp_path = remote_path;
#ifdef WIN32
    // Only the spelling is adjusted ("C:\dir" -> "/C:/dir"). The parameter is
    // the remote path without account prefix; running get_remote_path() over
    // it again would cut a drive letter path at its colon ("C:\temp\x" ->
    // "/temp/x"), which only ever worked because the Windows SFTP server
    // looks up "/temp" on the system drive.
    sftp_path = win_to_sftp_path(remote_path);
#endif
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session_ptr_, sftp_path.c_str(), &attrs);

    if (rc != 0) {
        // Only a confirmed "no such file" may report exists=false - see the
        // UNIX variant above. Other errors (permission denied, session error)
        // must throw into the caller's error path.
        const unsigned long err = libssh2_sftp_last_error(sftp_session_ptr_);
        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL
            && (err == LIBSSH2_FX_NO_SUCH_FILE || err == LIBSSH2_FX_NO_SUCH_PATH)) {
            info.exists = false;
            return info;
        }
        throw std::runtime_error("SFTP stat failed (error " + std::to_string(err) + "): " + remote_path);
    }
#ifdef WIN32
    if (remote_path.empty()) {
        info.exists = false;
        return info;
	}
#endif
    info.exists = true;
    info.size_bytes = attrs.filesize;
    info.raw_mtime = static_cast<time_t>(attrs.mtime);
    info.last_modified_iso = time_to_iso(info.raw_mtime);
    
    return info;
}

#else
// Fallback for unsupported platforms

SshFileInspector::SshFileInspector(const SshAccount& account) 
    : account_(account)
{
    throw std::runtime_error("SSH not supported on this platform");
}

SshFileInspector::~SshFileInspector() {}

void SshFileInspector::cleanup() {}

RemoteFileInfo SshFileInspector::inspect_file(const std::string& remote_path)
{
    RemoteFileInfo info{};
    info.exists = false;
    info.size_bytes = 0;
    info.raw_mtime = 0;
    info.last_modified_iso = "N/A";
    return info;
}

#endif // _WIN32
#endif // _UNIX

} // namespace AFS_Core
