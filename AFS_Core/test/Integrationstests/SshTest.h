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

} // namespace SSHTest

#define SKIP_WITHOUT_SSH_SERVER()                                              \
	do {                                                                       \
		if (!SSHTest::ssh_available())                                          \
			GTEST_SKIP() << "Kein SSH-Testserver auf " << SSHTest::SSH_HOST    \
			             << " erreichbar - siehe SSH_TESTS_README.md";         \
	} while (0)  // [SshTest.md#SKIP_WITHOUT_SSH_SERVER]