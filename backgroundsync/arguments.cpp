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
