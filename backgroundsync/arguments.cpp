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
#include "arguments.h"
#include "Logging.h"
#include <iostream>

using std::string;

Arguments::Arguments()
{}

Arguments& Arguments::instance()
{
	static Arguments obj;
	return obj;
}

void Arguments::init(int argc, char* argv[])
{
    loglevel_="INFO";
	bool nextIsLogFilename = false;
    bool nextIsLoglevel = false;
	for (int i = 1; i < argc; ++i)
	{
		if (nextIsLogFilename)
		{
			logFilename_ = argv[i];
			nextIsLogFilename = false;
		}
        else if (nextIsLoglevel)
        {
            loglevel_= argv[i];
            nextIsLoglevel = false;
        }
        else if (string(argv[i]) == "-logfile")
			nextIsLogFilename = true;
        else if (string(argv[i]) == "-loglevel")
            nextIsLoglevel = true;
        else if (string(argv[i]) == "-singlesync")
            singleSync_ = true;
        else
			std::cerr << "unknown command line option [" << i << "]: " << argv[i] << std::endl;
	}
}
