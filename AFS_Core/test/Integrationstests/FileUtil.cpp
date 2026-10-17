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
#include "pch.h"
#include "FileUtil.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <algorithm>

using namespace std::chrono;
using std::wstring;
using std::cerr;
using std::endl;
using std::exception;

namespace fs = std::filesystem;

namespace tu {

	void write_text_file(const fs::path& file, const std::string& content, std::chrono::seconds age)
	{
		fs::create_directories(file.parent_path());
		std::ofstream(file) << content;
		if (age != std::chrono::seconds(0))  // [FileUtil.md#write_text_file]
			fs::last_write_time(file, fs::file_time_type::clock::now() - age);
	}

	std::string read_text_file(const fs::path& file)
	{
		std::ifstream in(file);
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}

	void write_large_file(const fs::path& file, std::size_t bytes)
	{
		fs::create_directories(file.parent_path());
		std::ofstream out(file, std::ios::binary);
		std::string block(1u << 20, 'x');
		for (std::size_t written = 0; written < bytes; written += block.size())  // [FileUtil.md#write_large_file]
			out.write(block.data(), static_cast<std::streamsize>(std::min(block.size(), bytes - written)));
	}

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
		return temp_dir();  // [FileUtil.md#test-dirs-follow-the-temp-dir]
#else
		return "C:\\temp";
#endif
	}

#ifdef _UNIX
	std::string test_source_dir()
	{
		return (fs::path(test_temp_dir()) / "test" / "source").string();
	}

	std::string test_dest_dir()
	{
		return (fs::path(test_temp_dir()) / "test" / "dest").string();
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

	bool path_contains(std::wstring haystack, std::wstring needle)  // [FileUtil.md#path_contains]
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