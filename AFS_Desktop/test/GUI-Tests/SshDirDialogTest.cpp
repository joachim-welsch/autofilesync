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
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <array>
#include <stdexcept>
#include <fcntl.h>

namespace guitest {
namespace {

constexpr char kTestHost[] = "localhost";
constexpr char kTestUser[] = "testuser";
constexpr char kTestPassword[] = "testpassword";
// Own directory per test run, distinct from the fixtures the Integrationtests
// project keeps under the same account, so the two suites cannot collide.
constexpr char kSortProbeDir[] = "/tmp/autofilesync_guitest_sshdirdialog_sort_probe";
constexpr std::array<const char*, 3> kSortProbeFilesUnsorted = {"zeta.txt", "alpha.txt", "mango.txt"};

SshAccount testAccount()
{
    SshAccount account;
    account.setName("guitest-sshdirdialog");
    account.setHost(kTestHost);
    account.setUser(kTestUser);
    account.setPassword(kTestPassword);
    return account;
}

// Bare libssh/SFTP session for fixture setup: SshDirInspector only lists and
// reads directories, it has no upload/mkdir - those are only needed here, to
// build the small remote directory the sort test navigates into.
class RawSftpSession {
public:
    RawSftpSession()
    {
        session_ = ssh_new();
        if (session_ == nullptr)
            throw std::runtime_error("ssh_new fehlgeschlagen");
        ssh_options_set(session_, SSH_OPTIONS_HOST, kTestHost);
        ssh_options_set(session_, SSH_OPTIONS_USER, kTestUser);
        if (ssh_connect(session_) != SSH_OK)
            throw std::runtime_error(ssh_get_error(session_));
        if (!verifySshHostKey(session_, kTestHost))
            throw std::runtime_error("Host-Key-Pruefung fehlgeschlagen");
        if (ssh_userauth_password(session_, nullptr, kTestPassword) != SSH_AUTH_SUCCESS)
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

    sftp_session get() const { return sftp_; }

private:
    ssh_session session_ = nullptr;
    sftp_session sftp_ = nullptr;
};

std::string fileInProbeDir(const char* name)
{
    return std::string(kSortProbeDir) + "/" + name;
}

// Best-effort: an interrupted previous run may have left the directory behind,
// so removal happens before creation too and neither treats a missing file as
// an error.
void removeSortProbeDir(sftp_session sftp)
{
    for (const char* name : kSortProbeFilesUnsorted)
        sftp_unlink(sftp, fileInProbeDir(name).c_str());
    sftp_rmdir(sftp, kSortProbeDir);
}

void createSortProbeDir(sftp_session sftp)
{
    removeSortProbeDir(sftp);
    if (sftp_mkdir(sftp, kSortProbeDir, 0755) != SSH_OK)
        throw std::runtime_error("Anlegen des Testverzeichnisses fehlgeschlagen: " + std::string(kSortProbeDir));
    for (const char* name : kSortProbeFilesUnsorted) {
        sftp_file file = sftp_open(sftp, fileInProbeDir(name).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file == nullptr)
            throw std::runtime_error("Anlegen der Testdatei fehlgeschlagen: " + std::string(name));
        sftp_close(file);
    }
}

}

// Drives the Home and Sort-by-name buttons SshDirDialog got alongside Up and
// Refresh: Home should jump to the account's login directory, and Sort should
// reorder the currently shown entries ascending, then descending, without a
// new server round trip. Needs the local SSH test server from
// test/Integrationstests/SSH_TESTS_README.md and skips itself when that is
// not reachable, the same way the SSH Integrationtests do.
class SshDirDialogTest: public GuiTest {
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        try {
            RawSftpSession session;
            createSortProbeDir(session.get());
        } catch (const std::exception& ex) {
            GTEST_SKIP() << "Kein SSH-Testserver auf " << kTestHost
                << " erreichbar oder Fixture konnte nicht angelegt werden: " << ex.what();
        }
    }

    void TearDown() override
    {
        try {
            RawSftpSession session;
            removeSortProbeDir(session.get());
        } catch (const std::exception&) {
            // Server already unreachable - nothing left to clean up on it.
        }
        GuiTest::TearDown();
    }
};

TEST_F(SshDirDialogTest, HomeButtonNavigatesToLoginDirectory)
{
    const SshAccount account = testAccount();
    const wxString expectedHome = wxString::FromUTF8(SshDirInspector(account).getHomeDir());

    ModalDialogWatcher::instance().expect(L"SSH-Verzeichnis wählen", [&](wxDialog& dialog) {
        wxWindow* homeButton = find(dialog, ID_SSH_DIR_HOME_BUTTON);
        wxWindow* pathBox = find(dialog, ID_SSH_DIR_PATH_BOX);

        ASSERT_TRUE(clickOn(homeButton));
        ASSERT_TRUE(waitUntil([&] { return valueOf(pathBox) == expectedHome; }));
        EXPECT_EQ(valueOf(pathBox), expectedHome);

        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dlg(&mainWindow(), L"SSH-Verzeichnis wählen", account, "/tmp");
    dlg.ShowModal();
}

TEST_F(SshDirDialogTest, SortButtonTogglesAscendingAndDescending)
{
    const SshAccount account = testAccount();

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

    SshDirDialog dlg(&mainWindow(), L"SSH-Verzeichnis wählen", account, kSortProbeDir);
    dlg.ShowModal();
}

}
