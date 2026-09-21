#include "SshTestServer.h"
#include "SshHostKeyVerification.h"
#ifdef _WIN32
    #include "Libssh2Init.h"
    #include <ws2tcpip.h>
    #include <libssh2.h>
    #include <libssh2_sftp.h>
#else
    #include <libssh/libssh.h>
    #include <libssh/sftp.h>
    #include <fcntl.h>
#endif
#include <stdexcept>
#ifdef _WIN32
    #include <process.h>
#else
    #include <unistd.h>
#endif

using namespace AFS_Core;

namespace guitest {

SshAccount sshTestAccount(const std::string& name)
{
    SshAccount account;
    account.set_name(name);
    account.set_host(SSH_TEST_HOST);
    account.set_user(SSH_TEST_USER);
    account.set_password(SSH_TEST_PASSWORD);
    return account;
}

#ifdef _WIN32

struct RemoteSftp::Session {  // [SshTestServer.md#RemoteSftp]
    SOCKET socket_ = INVALID_SOCKET;
    LIBSSH2_SESSION* session_ = nullptr;
    LIBSSH2_SFTP* sftp_ = nullptr;
    bool winsockStarted_ = false;

    Session()
    {
        try {
            open();
        } catch (...) {
            release();  // the destructor does not run for a throwing constructor
            throw;
        }
    }

    ~Session()
    {
        release();
    }

    void open()
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            throw std::runtime_error("WSAStartup fehlgeschlagen");
        winsockStarted_ = true;
        connectSocket();
        ensure_libssh2_initialized();
        session_ = libssh2_session_init();
        if (session_ == nullptr)
            throw std::runtime_error("libssh2_session_init fehlgeschlagen");
        if (libssh2_session_handshake(session_, socket_) != 0)
            fail("SSH-Handshake fehlgeschlagen");
        if (!verify_ssh_host_key(session_, SSH_TEST_HOST))
            throw std::runtime_error("Host-Key-Pruefung fehlgeschlagen");
        if (libssh2_userauth_password(session_, SSH_TEST_USER, SSH_TEST_PASSWORD) != 0)
            fail("Anmeldung fehlgeschlagen");
        sftp_ = libssh2_sftp_init(session_);
        if (sftp_ == nullptr)
            fail("SFTP-Initialisierung fehlgeschlagen");
    }

    void connectSocket()
    {
        addrinfo hints{};
        hints.ai_family = AF_INET;  // same address family SshDirInspector connects with
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* address = nullptr;
        if (getaddrinfo(SSH_TEST_HOST, "22", &hints, &address) != 0)
            throw std::runtime_error("Host nicht aufloesbar: " + std::string(SSH_TEST_HOST));
        socket_ = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        const bool connected = socket_ != INVALID_SOCKET
            && ::connect(socket_, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0;
        freeaddrinfo(address);
        if (!connected)
            throw std::runtime_error("Verbindung zum SSH-Server fehlgeschlagen");
    }

    [[noreturn]] void fail(const char* what) const
    {
        char* message = nullptr;
        libssh2_session_last_error(session_, &message, nullptr, 0);
        throw std::runtime_error(std::string(what) + ": " + (message ? message : ""));
    }

    void release()
    {
        if (sftp_ != nullptr)
            libssh2_sftp_shutdown(sftp_);
        if (session_ != nullptr) {
            libssh2_session_disconnect(session_, "Normal Shutdown");
            libssh2_session_free(session_);
        }
        if (socket_ != INVALID_SOCKET)
            closesocket(socket_);
        if (winsockStarted_)
            WSACleanup();
        sftp_ = nullptr;
        session_ = nullptr;
        socket_ = INVALID_SOCKET;
        winsockStarted_ = false;
    }
};

std::string RemoteSftp::homeDir() const
{
    char buffer[1024];
    const int length = libssh2_sftp_realpath(session_->sftp_, ".", buffer, sizeof(buffer));
    if (length < 0)
        session_->fail("Login-Verzeichnis nicht ermittelbar");
    return std::string(buffer, length);
}

std::string RemoteSftp::scratchDir() const  // [SshTestServer.md#scratchDir]
{
    return homeDir();
}

bool RemoteSftp::makeDir(const std::string& path) const
{
    return libssh2_sftp_mkdir(session_->sftp_, path.c_str(), 0755) == 0;
}

bool RemoteSftp::createFile(const std::string& path, const std::string& content) const
{
    LIBSSH2_SFTP_HANDLE* file = libssh2_sftp_open(session_->sftp_, path.c_str(),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC, 0644);
    if (file == nullptr)
        return false;
    const bool written = content.empty() || libssh2_sftp_write(file, content.data(), content.size()) == static_cast<ssize_t>(content.size());
    libssh2_sftp_close(file);
    return written;
}

std::string RemoteSftp::readFile(const std::string& path) const
{
    LIBSSH2_SFTP_HANDLE* file = libssh2_sftp_open(session_->sftp_, path.c_str(), LIBSSH2_FXF_READ, 0);
    if (file == nullptr)
        return std::string();
    std::string content;
    char buffer[4096];
    for (ssize_t read = libssh2_sftp_read(file, buffer, sizeof(buffer)); read > 0; read = libssh2_sftp_read(file, buffer, sizeof(buffer)))
        content.append(buffer, read);
    libssh2_sftp_close(file);
    return content;
}

bool RemoteSftp::exists(const std::string& path) const
{
    LIBSSH2_SFTP_ATTRIBUTES attributes;
    return libssh2_sftp_stat(session_->sftp_, path.c_str(), &attributes) == 0;
}

void RemoteSftp::removeFile(const std::string& path) const
{
    libssh2_sftp_unlink(session_->sftp_, path.c_str());
}

void RemoteSftp::removeDir(const std::string& path) const
{
    libssh2_sftp_rmdir(session_->sftp_, path.c_str());
}

#else

struct RemoteSftp::Session {  // [SshTestServer.md#RemoteSftp]
    ssh_session session_ = nullptr;
    sftp_session sftp_ = nullptr;

    Session()
    {
        session_ = ssh_new();
        if (session_ == nullptr)
            throw std::runtime_error("ssh_new fehlgeschlagen");
        ssh_options_set(session_, SSH_OPTIONS_HOST, SSH_TEST_HOST);
        ssh_options_set(session_, SSH_OPTIONS_USER, SSH_TEST_USER);
        if (ssh_connect(session_) != SSH_OK)
            throw std::runtime_error(ssh_get_error(session_));
        if (!verify_ssh_host_key(session_, SSH_TEST_HOST))
            throw std::runtime_error("Host-Key-Pruefung fehlgeschlagen");
        if (ssh_userauth_password(session_, nullptr, SSH_TEST_PASSWORD) != SSH_AUTH_SUCCESS)
            throw std::runtime_error(ssh_get_error(session_));
        sftp_ = sftp_new(session_);
        if (sftp_ == nullptr || sftp_init(sftp_) != SSH_OK)
            throw std::runtime_error("SFTP-Initialisierung fehlgeschlagen");
    }

    ~Session()
    {
        if (sftp_ != nullptr)
            sftp_free(sftp_);
        if (session_ != nullptr) {
            ssh_disconnect(session_);
            ssh_free(session_);
        }
    }
};

std::string RemoteSftp::homeDir() const
{
    char* path = sftp_canonicalize_path(session_->sftp_, ".");
    if (path == nullptr)
        throw std::runtime_error("Login-Verzeichnis nicht ermittelbar");
    std::string home(path);
    ssh_string_free_char(path);
    return home;
}

std::string RemoteSftp::scratchDir() const  // [SshTestServer.md#scratchDir]
{
    return "/tmp";
}

bool RemoteSftp::makeDir(const std::string& path) const
{
    return sftp_mkdir(session_->sftp_, path.c_str(), 0755) == SSH_OK;
}

bool RemoteSftp::createFile(const std::string& path, const std::string& content) const
{
    sftp_file file = sftp_open(session_->sftp_, path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == nullptr)
        return false;
    const bool written = content.empty() || sftp_write(file, content.data(), content.size()) == static_cast<ssize_t>(content.size());
    sftp_close(file);
    return written;
}

std::string RemoteSftp::readFile(const std::string& path) const
{
    sftp_file file = sftp_open(session_->sftp_, path.c_str(), O_RDONLY, 0);
    if (file == nullptr)
        return std::string();
    std::string content;
    char buffer[4096];
    for (ssize_t read = sftp_read(file, buffer, sizeof(buffer)); read > 0; read = sftp_read(file, buffer, sizeof(buffer)))
        content.append(buffer, read);
    sftp_close(file);
    return content;
}

bool RemoteSftp::exists(const std::string& path) const
{
    sftp_attributes attributes = sftp_stat(session_->sftp_, path.c_str());
    if (attributes == nullptr)
        return false;
    sftp_attributes_free(attributes);
    return true;
}

void RemoteSftp::removeFile(const std::string& path) const
{
    sftp_unlink(session_->sftp_, path.c_str());
}

void RemoteSftp::removeDir(const std::string& path) const
{
    sftp_rmdir(session_->sftp_, path.c_str());
}

#endif

RemoteSftp::RemoteSftp(): session_(std::make_unique<Session>()) {}

RemoteSftp::~RemoteSftp() = default;

namespace {

int currentProcessId()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

}

void SshGuiTest::SetUp()
{
    GuiTest::SetUp();
    try {
        sftp_ = std::make_unique<RemoteSftp>();
    } catch (const std::exception& ex) {
        GTEST_SKIP() << "Kein SSH-Testserver auf " << SSH_TEST_HOST << " erreichbar: " << ex.what();
    }
    probeDir_ = sftp_->scratchDir() + "/autofilesync_guitest_" + std::to_string(currentProcessId());  // [SshTestServer.md#probeDir]
    sftp_->removeDir(probeDir_);
    if (!sftp_->makeDir(probeDir_))
        FAIL() << "Das Testverzeichnis auf dem Server ließ sich nicht anlegen: " << probeDir_;
}

void SshGuiTest::TearDown()
{
    if (sftp_ != nullptr && !probeDir_.empty())
        sftp_->removeDir(probeDir_);  // [SshTestServer.md#probeDir]
    sftp_.reset();
    GuiTest::TearDown();
}

}
