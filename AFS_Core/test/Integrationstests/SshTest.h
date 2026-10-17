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

#include <string>

namespace SSHTest {

	constexpr char SSH_HOST[] = "localhost";
	constexpr char SSH_USER[] = "testuser";
	constexpr char SSH_PASSWORD[] = "testpassword";
#ifdef _UNIX
	constexpr char HOSTS_PATH[] = "/etc/hosts";
	constexpr char ETC_PATH[] = "/etc";
	constexpr char PATH_SEPARATOR[] = "/";
	constexpr wchar_t PATH_SEPARATOR_W[] = L"/";
#else
	constexpr char HOSTS_PATH[] = "\\Windows\\System32\\drivers\\etc\\hosts";
	constexpr char ETC_PATH[] = "\\Windows\\System32\\drivers\\etc";
	constexpr char PATH_SEPARATOR[] = "\\";
	constexpr wchar_t PATH_SEPARATOR_W[] = L"\\";
	std::wstring win_to_sftp_pathW(const std::wstring& win);
#endif

	bool ssh_available();  // [SshTest.md#ssh_available-contract]
	void remove_remote_tree(const std::string& path);  // [SshTest.md#remove_remote_tree]

} // namespace SSHTest

#define SKIP_WITHOUT_SSH_SERVER()                                              \
	do {                                                                       \
		if (!SSHTest::ssh_available())                                          \
			GTEST_SKIP() << "Kein SSH-Testserver auf " << SSHTest::SSH_HOST    \
						 << " erreichbar - siehe doc/SSH_TESTS_README.md";         \
	} while (0)  // [SshTest.md#SKIP_WITHOUT_SSH_SERVER]
