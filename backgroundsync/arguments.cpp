/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of backgroundsync, the background service of AutoFileSync.
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
