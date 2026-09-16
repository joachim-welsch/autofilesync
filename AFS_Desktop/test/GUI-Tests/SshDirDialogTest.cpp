#include "GuiTest.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "SshDirDialog.h"
#include "SshDirInspector.h"
#include "SshAccount.h"
#include "SshHostKeyVerification.h"
#include <wx/listctrl.h>
#include <wx/uiaction.h>
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
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

using namespace AFS_Core;

namespace guitest {
namespace {

constexpr char TEST_HOST[] = "localhost";
constexpr char TEST_USER[] = "testuser";
constexpr char TEST_PASSWORD[] = "testpassword";
constexpr char SORT_PROBE_DIR_NAME[] = "autofilesync_guitest_sshdirdialog_sort_probe";
constexpr std::array<const char*, 3> SORT_PROBE_FILES_UNSORTED = {"zeta.txt", "alpha.txt", "mango.txt"};

SshAccount test_account()
{
    SshAccount account;
    account.set_name("guitest-sshdirdialog");
    account.set_host(TEST_HOST);
    account.set_user(TEST_USER);
    account.set_password(TEST_PASSWORD);
    return account;
}

#ifdef _WIN32

using SftpHandle = LIBSSH2_SFTP*;

class RawSftpSession {  // [SshDirDialogTest.md#RawSftpSession]
public:
    RawSftpSession()
    {
        try {
            open();
        } catch (...) {
            release();  // the destructor does not run for a throwing constructor
            throw;
        }
    }

    ~RawSftpSession()
    {
        release();
    }

    RawSftpSession(const RawSftpSession&) = delete;
    RawSftpSession& operator=(const RawSftpSession&) = delete;

    SftpHandle get() const { return sftp_; }

    std::string home_dir() const
    {
        char buffer[1024];
        const int length = libssh2_sftp_realpath(sftp_, ".", buffer, sizeof(buffer));
        if (length < 0)
            fail("Login-Verzeichnis nicht ermittelbar");
        return std::string(buffer, length);
    }

private:
    SOCKET socket_ = INVALID_SOCKET;
    LIBSSH2_SESSION* session_ = nullptr;
    LIBSSH2_SFTP* sftp_ = nullptr;
    bool winsock_started_ = false;

    void open()
    {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
            throw std::runtime_error("WSAStartup fehlgeschlagen");
        winsock_started_ = true;
        connect_socket();
        ensure_libssh2_initialized();
        session_ = libssh2_session_init();
        if (session_ == nullptr)
            throw std::runtime_error("libssh2_session_init fehlgeschlagen");
        if (libssh2_session_handshake(session_, socket_) != 0)
            fail("SSH-Handshake fehlgeschlagen");
        if (!verify_ssh_host_key(session_, TEST_HOST))
            throw std::runtime_error("Host-Key-Pruefung fehlgeschlagen");
        if (libssh2_userauth_password(session_, TEST_USER, TEST_PASSWORD) != 0)
            fail("Anmeldung fehlgeschlagen");
        sftp_ = libssh2_sftp_init(session_);
        if (sftp_ == nullptr)
            fail("SFTP-Initialisierung fehlgeschlagen");
    }

    void connect_socket()
    {
        addrinfo hints{};
        hints.ai_family = AF_INET;  // same address family SshDirInspector connects with
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* address = nullptr;
        if (getaddrinfo(TEST_HOST, "22", &hints, &address) != 0)
            throw std::runtime_error("Host nicht aufloesbar: " + std::string(TEST_HOST));
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
        if (winsock_started_)
            WSACleanup();
        sftp_ = nullptr;
        session_ = nullptr;
        socket_ = INVALID_SOCKET;
        winsock_started_ = false;
    }
};

std::string sort_probe_dir(const RawSftpSession& session)  // [SshDirDialogTest.md#sort_probe_dir]
{
    return session.home_dir() + "/" + SORT_PROBE_DIR_NAME;
}

void remove_file(SftpHandle sftp, const std::string& path)
{
    libssh2_sftp_unlink(sftp, path.c_str());
}

void remove_dir(SftpHandle sftp, const std::string& path)
{
    libssh2_sftp_rmdir(sftp, path.c_str());
}

bool make_dir(SftpHandle sftp, const std::string& path)
{
    return libssh2_sftp_mkdir(sftp, path.c_str(), 0755) == 0;
}

bool create_empty_file(SftpHandle sftp, const std::string& path)
{
    LIBSSH2_SFTP_HANDLE* file = libssh2_sftp_open(sftp, path.c_str(),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC, 0644);
    if (file == nullptr)
        return false;
    libssh2_sftp_close(file);
    return true;
}

#else

using SftpHandle = sftp_session;

class RawSftpSession {  // [SshDirDialogTest.md#RawSftpSession]
public:
    RawSftpSession()
    {
        session_ = ssh_new();
        if (session_ == nullptr)
            throw std::runtime_error("ssh_new fehlgeschlagen");
        ssh_options_set(session_, SSH_OPTIONS_HOST, TEST_HOST);
        ssh_options_set(session_, SSH_OPTIONS_USER, TEST_USER);
        if (ssh_connect(session_) != SSH_OK)
            throw std::runtime_error(ssh_get_error(session_));
        if (!verify_ssh_host_key(session_, TEST_HOST))
            throw std::runtime_error("Host-Key-Pruefung fehlgeschlagen");
        if (ssh_userauth_password(session_, nullptr, TEST_PASSWORD) != SSH_AUTH_SUCCESS)
            throw std::runtime_error(ssh_get_error(session_));
        sftp_ = sftp_new(session_);
        if (sftp_ == nullptr || sftp_init(sftp_) != SSH_OK)
            throw std::runtime_error("SFTP-Initialisierung fehlgeschlagen");
    }

    ~RawSftpSession()
    {
        if (sftp_ != nullptr)
            sftp_free(sftp_);
        if (session_ != nullptr) {
            ssh_disconnect(session_);
            ssh_free(session_);
        }
    }

    RawSftpSession(const RawSftpSession&) = delete;
    RawSftpSession& operator=(const RawSftpSession&) = delete;

    SftpHandle get() const { return sftp_; }

private:
    ssh_session session_ = nullptr;
    sftp_session sftp_ = nullptr;
};

std::string sort_probe_dir([[maybe_unused]] const RawSftpSession& session)  // [SshDirDialogTest.md#sort_probe_dir]
{
    return std::string("/tmp/") + SORT_PROBE_DIR_NAME;
}

void remove_file(SftpHandle sftp, const std::string& path)
{
    sftp_unlink(sftp, path.c_str());
}

void remove_dir(SftpHandle sftp, const std::string& path)
{
    sftp_rmdir(sftp, path.c_str());
}

bool make_dir(SftpHandle sftp, const std::string& path)
{
    return sftp_mkdir(sftp, path.c_str(), 0755) == SSH_OK;
}

bool create_empty_file(SftpHandle sftp, const std::string& path)
{
    sftp_file file = sftp_open(sftp, path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == nullptr)
        return false;
    sftp_close(file);
    return true;
}

#endif

std::string file_in(const std::string& dir, const char* name)
{
    return dir + "/" + name;
}

void remove_sort_probe_dir(SftpHandle sftp, const std::string& dir)  // [SshDirDialogTest.md#remove_sort_probe_dir]
{
    for (const char* name : SORT_PROBE_FILES_UNSORTED)
        remove_file(sftp, file_in(dir, name));
    remove_dir(sftp, dir);
}

void create_sort_probe_dir(SftpHandle sftp, const std::string& dir)
{
    remove_sort_probe_dir(sftp, dir);
    if (!make_dir(sftp, dir))
        throw std::runtime_error("Anlegen des Testverzeichnisses fehlgeschlagen: " + dir);
    for (const char* name : SORT_PROBE_FILES_UNSORTED)
        if (!create_empty_file(sftp, file_in(dir, name)))
            throw std::runtime_error("Anlegen der Testdatei fehlgeschlagen: " + std::string(name));
}

}

class SshDirDialogTest: public GuiTest {  // [SshDirDialogTest.md#SshDirDialogTest]
protected:
    std::string probe_dir_;

    void SetUp() override
    {
        GuiTest::SetUp();
        try {
            RawSftpSession session;
            probe_dir_ = sort_probe_dir(session);
            create_sort_probe_dir(session.get(), probe_dir_);
        } catch (const std::exception& ex) {
            GTEST_SKIP() << "Kein SSH-Testserver auf " << TEST_HOST
                << " erreichbar oder Fixture konnte nicht angelegt werden: " << ex.what();
        }
    }

    void TearDown() override
    {
        try {
            if (!probe_dir_.empty()) {
                RawSftpSession session;
                remove_sort_probe_dir(session.get(), probe_dir_);
            }
        } catch (const std::exception&) {  // server already unreachable - nothing left to clean up on it
        }
        GuiTest::TearDown();
    }
};

TEST_F(SshDirDialogTest, HomeButtonNavigatesToLoginDirectory)
{
    const SshAccount account = test_account();
    const wxString expectedHome = wxString::FromUTF8(SshDirInspector(account).get_home_dir());

    ModalDialogWatcher::instance().expect(L"SSH-Verzeichnis wählen", [&](wxDialog& dialog) {
        wxWindow* homeButton = find(dialog, ID_SSH_DIR_HOME_BUTTON);
        wxWindow* pathBox = find(dialog, ID_SSH_DIR_PATH_BOX);

        ASSERT_TRUE(clickOn(homeButton));
        ASSERT_TRUE(waitUntil([&] { return valueOf(pathBox) == expectedHome; }));
        EXPECT_EQ(valueOf(pathBox), expectedHome);

        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dlg(&mainWindow(), L"SSH-Verzeichnis wählen", account, wxString::FromUTF8(probe_dir_));  // [SshDirDialogTest.md#home-start-dir]
    dlg.ShowModal();
}

TEST_F(SshDirDialogTest, SortButtonTogglesAscendingAndDescending)
{
    const SshAccount account = test_account();

    ModalDialogWatcher::instance().expect(L"SSH-Verzeichnis wählen", [](wxDialog& dialog) {
        wxWindow* sortButton = find(dialog, ID_SSH_DIR_SORT_BUTTON);
        auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSH_DIR_LIST));
        ASSERT_NE(list, nullptr);
        ASSERT_TRUE(waitUntil([&] { return list->GetItemCount() == 3; }));

        auto namesInOrder = [&] {
            std::vector<wxString> names;
            for (int i = 0; i < list->GetItemCount(); ++i)
                names.push_back(list->GetItemText(i));
            return names;
        };

        ASSERT_TRUE(clickOn(sortButton));
        pump(100);
        EXPECT_EQ(namesInOrder(), (std::vector<wxString>{"alpha.txt", "mango.txt", "zeta.txt"}));

        ASSERT_TRUE(clickOn(sortButton));
        pump(100);
        EXPECT_EQ(namesInOrder(), (std::vector<wxString>{"zeta.txt", "mango.txt", "alpha.txt"}));

        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dlg(&mainWindow(), L"SSH-Verzeichnis wählen", account, wxString::FromUTF8(probe_dir_));
    dlg.ShowModal();
}

}
