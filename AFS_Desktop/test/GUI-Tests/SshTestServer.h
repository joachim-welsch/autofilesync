/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
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
