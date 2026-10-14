/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
#pragma once

#include "GuiTest.h"
#include "SshAccount.h"
#include <memory>
#include <string>

namespace guitest {  // [SshTestServer.md#SshTestServer]

constexpr char SSH_TEST_HOST[] = "localhost";
constexpr char SSH_TEST_USER[] = "testuser";
constexpr char SSH_TEST_PASSWORD[] = "testpassword";

AFS_Core::SshAccount ssh_test_account(const std::string& name);

class RemoteSftp {  // [SshTestServer.md#RemoteSftp]
public:
    RemoteSftp();
    ~RemoteSftp();
    RemoteSftp(const RemoteSftp&) = delete;
    RemoteSftp& operator=(const RemoteSftp&) = delete;
    std::string home_dir() const;
    std::string scratch_dir() const;  // [SshTestServer.md#scratch_dir]
    bool make_dir(const std::string& path) const;
    bool create_file(const std::string& path, const std::string& content = std::string()) const;
    std::string read_file(const std::string& path) const;
    bool exists(const std::string& path) const;
    void remove_file(const std::string& path) const;
    void remove_dir(const std::string& path) const;
private:
    struct Session;
    std::unique_ptr<Session> session;
};

class SshGuiTest: public GuiTest {  // [SshTestServer.md#SshGuiTest]
protected:
    void SetUp() override;
    void TearDown() override;
    RemoteSftp& sftp() { return *sftp_; }
    std::string probe_dir() const { return probe_dir_; }  // [SshTestServer.md#probe_dir]
private:
    std::unique_ptr<RemoteSftp> sftp_;
    std::string probe_dir_;
};

}
