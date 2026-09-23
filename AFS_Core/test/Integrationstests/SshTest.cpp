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
#include "SshTest.h"
#include "SshAccount.h"
#include "SshFileInspector.h"
#include "SftpConnection.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <exception>

using namespace AFS_Core;

std::wstring win_to_sftp_path_w(const std::wstring& win)
{
	std::wstring p = win;
	std::replace(p.begin(), p.end(), L'\\', L'/');
	if (p.size() >= 2 && std::iswalpha(p[0]) && p[1] == L':')  // "C:/..." becomes "/C:/..."
		p.insert(p.begin(), L'/');
	return p;
}

namespace SSHTest {

bool ssh_available()
{
	static const bool AVAILABLE = [] {  // [SshTest.md#ssh_available]
		SshAccount account;
		account.set_name("ssh_probe");
		account.set_user(SSH_USER);
		account.set_host(SSH_HOST);
		account.set_password(SSH_PASSWORD);
		try {
			SshFileInspector probe(account);
			return true;
		}
		catch (const std::exception&) {
			return false;
		}
		catch (...) {
			return false;
		}
	}();
	return AVAILABLE;
}

#ifdef _UNIX
namespace {
SshAccount test_account()
{
	SshAccount account;
	account.set_name("ssh_cleanup");
	account.set_user(SSH_USER);
	account.set_host(SSH_HOST);
	account.set_password(SSH_PASSWORD);
	return account;
}

void remove_tree(const SftpConnection& connection, const std::string& path)
{
	const SftpAttributes attributes = connection.stat(path);
	if (!attributes)
		return;
	if (attributes->type != SSH_FILEXFER_TYPE_DIRECTORY) {
		sftp_unlink(connection.sftp(), path.c_str());
		return;
	}
	std::vector<std::string> children;
	{
		const SftpDir dir = connection.open_dir(path);
		while (dir) {
			const SftpAttributes entry = connection.read_dir(dir.get());
			if (!entry)
				break;
			const std::string name = entry->name;
			if (name != "." && name != "..")
				children.push_back(path + "/" + name);
		}
	}
	for (const std::string& child: children)
		remove_tree(connection, child);
	sftp_rmdir(connection.sftp(), path.c_str());
}
}

void remove_remote_tree(const std::string& path)
{
	try {
		const SftpConnection connection(test_account());
		remove_tree(connection, path);
	}
	catch (const std::exception&) {  // no server, nothing of it to remove
	}
}
#endif

} // namespace SSHTest