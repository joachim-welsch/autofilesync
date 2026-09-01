#include "pch.h"
#include "FileUtil.h"
#include <filesystem>
#include <iostream>
#include <chrono>

using namespace std::chrono;
using std::wstring;
using std::cerr;
using std::endl;
using std::exception;

namespace fs = std::filesystem;

namespace tu {

	bool eraseTestFile(const wstring& fn)
	{
		if (std::filesystem::exists(fn))
		{
			return std::filesystem::remove(fn);
		}
		return true;
	}

	bool copyFile(const wstring& source, const wstring& destination)
	{
		eraseTestFile(destination);
		try {
			fs::create_directories(fs::path(destination).parent_path());
			fs::copy_file(source, destination);
		}
		catch (exception& ex) {
			cerr << ex.what() << endl;
			return false;
		}
		return true;
	}

	bool copySourceToDest(const wstring& source, const wstring& destination)
	{
		return copyFile(source, destination);
	}

	void setFileDate(const std::wstring& filePath, int newyear, int newmonth, int newday) {
		try {
			fs::path path(filePath);
			auto date = year(newyear) / month(newmonth) / day(newday);
			auto timepoint = sys_days(date);
			auto file_time = fs::file_time_type::clock::now() + (timepoint - std::chrono::system_clock::now());
			fs::last_write_time(path, file_time);
			std::wcout << L"Dateidatum erfolgreich gesetzt." << std::endl;
		}
		catch (const fs::filesystem_error& e) {
			std::wcerr << L"Fehler beim Setzen des Dateidatums: " << e.what() << std::endl;
		}
	}

	std::wstring tempDirW()
	{
		return fs::temp_directory_path().wstring();
	}

	std::string tempDir()
	{
		return fs::temp_directory_path().string();
	}

	std::string testTempDir()
	{
#ifdef _UNIX
		return "/tmp";
#else
		return "C:\\temp";
#endif
	}

#ifdef _UNIX
	std::string testSourceDir()
	{ 
		return "/tmp/test/source";
	}

	std::string testDestDir()
	{
		return "/tmp/test/dest";
	}
#else
	std::string testSourceDir()
	{
		return "C:\\temp\\test\\source";
	}

	std::string testDestDir()
	{
		return "C:\\temp\\test\\dest";
	}
#endif

	// Windows liefert '\' und ggf. abweichende Groß-/Kleinschreibung zurück.
	bool pathContains(std::wstring haystack, std::wstring needle)
	{
		auto normalize = [](std::wstring& s) {
			std::replace(s.begin(), s.end(), L'\\', L'/');
#ifdef _WIN32
			std::transform(s.begin(), s.end(), s.begin(),
				[](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
#endif
			};
		normalize(haystack);
		normalize(needle);
		return haystack.find(needle) != std::wstring::npos;
	}
} // namespace testutil