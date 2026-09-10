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

	// True when a real SSH server is reachable at SSH_HOST and accepts
	// SSH_USER/SSH_PASSWORD. Probed once per process, so enabling the SSH sync
	// tests costs a single connection attempt on machines that have no server.
	bool ssh_available();

} // namespace SSHTest

// The SSH sync tests need a real server (see SSH_TESTS_README.md). They are
// enabled rather than DISABLED_, so they run wherever the test server exists
// and skip - instead of failing - wherever it does not.
#define SKIP_WITHOUT_SSH_SERVER()                                              \
	do {                                                                       \
		if (!SSHTest::ssh_available())                                          \
			GTEST_SKIP() << "Kein SSH-Testserver auf " << SSHTest::SSH_HOST    \
			             << " erreichbar - siehe SSH_TESTS_README.md";         \
	} while (0)
