#pragma once

#include "GuiTest.h"
#include "SshAccount.h"
#include <memory>
#include <string>

namespace guitest {  // [SshTestServer.md#SshTestServer]

constexpr char SSH_TEST_HOST[] = "localhost";
constexpr char SSH_TEST_USER[] = "testuser";
constexpr char SSH_TEST_PASSWORD[] = "testpassword";

AFS_Core::SshAccount sshTestAccount(const std::string& name);

class RemoteSftp {  // [SshTestServer.md#RemoteSftp]
public:
    RemoteSftp();
    ~RemoteSftp();
    RemoteSftp(const RemoteSftp&) = delete;
    RemoteSftp& operator=(const RemoteSftp&) = delete;
    std::string homeDir() const;
    std::string scratchDir() const;  // [SshTestServer.md#scratchDir]
    bool makeDir(const std::string& path) const;
    bool createFile(const std::string& path, const std::string& content = std::string()) const;
    std::string readFile(const std::string& path) const;
    bool exists(const std::string& path) const;
    void removeFile(const std::string& path) const;
    void removeDir(const std::string& path) const;
private:
    struct Session;
    std::unique_ptr<Session> session_;
};

class SshGuiTest: public GuiTest {  // [SshTestServer.md#SshGuiTest]
protected:
    void SetUp() override;
    void TearDown() override;
    RemoteSftp& sftp() { return *sftp_; }
    std::string probeDir() const { return probeDir_; }  // [SshTestServer.md#probeDir]
private:
    std::unique_ptr<RemoteSftp> sftp_;
    std::string probeDir_;
};

}
