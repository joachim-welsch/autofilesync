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

namespace oauth {  // [GoogleAuth.md#oauth]

struct Settings {  // [GoogleAuth.md#settings]
    std::string client_id;
    std::string client_secret;
    std::string auth_url;
    std::string token_url;
    std::string api_url;
};

struct Provider {  // [GoogleAuth.md#Provider]
    Settings& (*settings)();
    const char* scope;
    const char* consent_parameters;
    const char* redirect_host;
};

const Provider& google();
const Provider& microsoft();  // [GoogleAuth.md#microsoft]
const Provider& provider_of(const SshAccount& account);
bool configured(const Provider& provider);

class Login {  // [GoogleAuth.md#Login]
    const Provider& provider_;
    int listener_ = -1;
    int port_ = 0;
    std::string verifier_;
    std::string state_;
    std::string code_;
    std::string error_;
    std::string redirect_uri() const;
    void answer_browser(int connection);
public:
    explicit Login(const Provider& provider);
    ~Login();
    Login(const Login&) = delete;
    Login& operator=(const Login&) = delete;
    std::string browser_url() const;
    bool finished(std::chrono::milliseconds patience);  // [GoogleAuth.md#finished]
    std::string refresh_token() const;  // [GoogleAuth.md#refresh_token]
};

std::string access_token(const SshAccount& account);  // [GoogleAuth.md#access_token]

}

namespace google_auth {  // [GoogleAuth.md#google_auth]

using Settings = oauth::Settings;

Settings& settings();  // [GoogleAuth.md#settings]
bool configured();

struct Login : oauth::Login {
    Login(): oauth::Login(oauth::google()) {}
};

inline std::string access_token(const SshAccount& account) { return oauth::access_token(account); }

}

namespace microsoft_auth {  // [GoogleAuth.md#microsoft]

oauth::Settings& settings();

}

} // namespace AFS_Core
