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

	bool erase_test_file(const wstring& fn)
	{
		if (std::filesystem::exists(fn))
		{
			return std::filesystem::remove(fn);
		}
		return true;
	}

	bool copy_file(const wstring& source, const wstring& destination)
	{
		erase_test_file(destination);
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

	bool copy_source_to_dest(const wstring& source, const wstring& destination)
	{
		return copy_file(source, destination);
	}

	void set_file_date(const std::wstring& file_path, int newyear, int newmonth, int newday) {
		try {
			fs::path path(file_path);
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

	std::wstring temp_dir_w()
	{
		return fs::temp_directory_path().wstring();
	}

	std::string temp_dir()
	{
		return fs::temp_directory_path().string();
	}

	std::string test_temp_dir()
	{
#ifdef _UNIX
		return "/tmp";
#else
		return "C:\\temp";
#endif
	}

#ifdef _UNIX
	std::string test_source_dir()
	{ 
		return "/tmp/test/source";
	}

	std::string test_dest_dir()
	{
		return "/tmp/test/dest";
	}
#else
	std::string test_source_dir()
	{
		return "C:\\temp\\test\\source";
	}

	std::string test_dest_dir()
	{
		return "C:\\temp\\test\\dest";
	}
#endif

	// Windows liefert '\' und ggf. abweichende Groß-/Kleinschreibung zurück.
	bool path_contains(std::wstring haystack, std::wstring needle)
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