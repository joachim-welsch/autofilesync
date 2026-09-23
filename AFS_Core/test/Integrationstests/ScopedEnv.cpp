/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#include "pch.h"
#include "ScopedEnv.h"
#ifdef _WIN32
  #include <windows.h>
#endif

ScopedEnv::ScopedEnv(std::string name, const char* value)   // value == nullptr -> unset the variable
    : m_name(std::move(name))
{
    if (const char* old = std::getenv(m_name.c_str()))
        m_saved = old;                            // copied now: apply() below changes what getenv() returned
    apply(m_name.c_str(), value);
}

ScopedEnv::~ScopedEnv()
{ 
    apply(m_name.c_str(), m_saved ? m_saved->c_str() : nullptr); 
}

void ScopedEnv::apply(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value ? value : "");   // "" unsets it in the CRT block
    SetEnvironmentVariableA(name, value);  // nullptr unsets it in the Win32 block
#else
    if (value) setenv(name, value, 1); else unsetenv(name);
#endif
}