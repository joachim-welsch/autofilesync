#include "pch.h"
#include "SshTest.h"
#include "SshAccount.h"
#include "SshFileInspector.h"
#include <algorithm>
#include <cwctype>
#include <exception>

std::wstring win_to_sftp_pathW(const std::wstring& win)
{
	std::wstring p = win;
	// Backslashes -> Slashes
	std::replace(p.begin(), p.end(), L'\\', L'/');
	// "C:/..." -> "/C:/..."
	if (p.size() >= 2 && std::iswalpha(p[0]) && p[1] == L':')
		p.insert(p.begin(), L'/');
	return p;
}

namespace SSHTest {

bool sshAvailable()
{
	// SshFileInspector's constructor connects, verifies the host key and
	// authenticates, throwing on any failure - so building one is the cheapest
	// honest answer to "is the test server there?". Cached: the SSH tests would
	// otherwise pay a full connection setup per test just to decide to skip.
	static const bool available = [] {
		SshAccount account;
		account.setName("ssh_probe");
		account.setUser(SSH_USER);
		account.setHost(SSH_HOST);
		account.setPassword(SSH_PASSWORD);
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
	return available;
}

} // namespace SSHTest