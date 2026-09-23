/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of backgroundsync, the background service of AutoFileSync.
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
#include "stdafx.h"
#include "delete_files.h"
#include <synchronize.h>
#include <string>
#include <filesystem>
#ifndef _UNIX
#include <Windows.h>
#include <wincon.h>
#include <direct.h>
#else
#endif

#include <winutil.h>
#include <util.h>
#include <iostream>

namespace fs=std::filesystem;
using std::string;
using namespace util;

DeleteFiles::DeleteFiles(const SyncItem& si): si_(si)
{}

#ifdef WIN32
void DeleteFiles::operator()()
{
	//QCoreApplication::processEvents();
	if (Synchronize::stopped())
		return;
	WIN32_FIND_DATA FData;
	string search = si_.source() + "\\" + si_.wildcard();
	HANDLE  hSearch(FindFirstFile(string_to_wstring(search).c_str(), &FData));
	BOOL MoreFiles = FALSE;
	if (hSearch == INVALID_HANDLE_VALUE) {
		FindClose(hSearch);
		return;
	}
	do {
		const string fname = wstring_to_string(FData.cFileName);
		if ((FData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && fname != "." && fname != "..") {
			const string source = withTrailingChar(si_.source(), '\\') + fname;
			int rc=remove(source.c_str());
			if (rc != 0)
				std::cerr << LastErrorText() << std::endl;
		}
		MoreFiles = FindNextFile(hSearch, &FData);
	} while (MoreFiles && !Synchronize::stopped());
	FindClose(hSearch);
}
#else
void DeleteFiles::operator()()
{
    if (!si_.wildcard().empty()) {
        std::cerr << "Not implemented: delete files withwildcard\n";
        return;
    }
    fs::remove_all(si_.source());
    // TODO wildcard implemenieren
    //string search = si_.source() + "\\" + si_.wildcard();
//    if(fs::is_directory(si_.source())) {
//        for(auto& p: fs::directory_iterator(src)) {
//            const string filename=p.path().filename();
//            const string src= dir + "/" + filename;
//            const string dest=util::withTrailingChar(destination, '/')+filename;
//            SyncFile(SyncItem(si.typeStr(), src, dest, "", si.flags(), si.compareItems()), copyCount)();
//        }
//    }
}
#endif
