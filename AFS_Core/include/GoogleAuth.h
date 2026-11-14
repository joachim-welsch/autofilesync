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
#pragma once
#include <chrono>
#include <string>
#include "SshAccount.h"

namespace AFS_Core {

namespace google_auth {  // [GoogleAuth.md#google_auth]

struct Settings {  // [GoogleAuth.md#settings]
    std::string client_id;
    std::string client_secret;
    std::string auth_url = "https://accounts.google.com/o/oauth2/v2/auth";
    std::string token_url = "https://oauth2.googleapis.com/token";
    std::string api_url = "https://www.googleapis.com";
};

Settings& settings();  // [GoogleAuth.md#settings]
bool configured();

class Login {  // [GoogleAuth.md#Login]
    int listener_ = -1;
    int port_ = 0;
    std::string verifier_;
    std::string state_;
    std::string code_;
    std::string error_;
    std::string redirect_uri() const;
    void answer_browser(int connection);
public:
    Login();
    ~Login();
    Login(const Login&) = delete;
    Login& operator=(const Login&) = delete;
    std::string browser_url() const;
    bool finished(std::chrono::milliseconds patience);  // [GoogleAuth.md#finished]
    std::string refresh_token() const;  // [GoogleAuth.md#refresh_token]
};

std::string access_token(const SshAccount& account);  // [GoogleAuth.md#access_token]

}

} // namespace AFS_Core
