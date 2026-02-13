// Copyright Dan Price. All rights reserved.

#include "JsonToWav.h"
#include "JsonInterpreter.h"
#include "JsonParser.h"
#include <fstream>
#include <iostream>
#include <string>

namespace json2wav
{
	int JsonToWav(const char* const filename, const bool bLog)
	{
#ifdef ALBUMBOT_DEBUGNEW
		json2wav::PrintAllocTimes("at start of JsonToWav()");
#endif
		const std::string strfilename(filename);
		std::ifstream jsonfile(strfilename);
		if (!jsonfile)
			return -1;
		JsonParser p;
		if (bLog)
		{
			JsonInterpreter_Logging i(strfilename.substr(0, strfilename.find_last_of(".")));
			const bool bParsed = p.parse(jsonfile, i);
			if (!bParsed)
			{
				std::cerr << "Parse error; invalid JSON.\n";
				return -2;
			}
		}
		else
		{
			JsonInterpreter i(strfilename.substr(0, strfilename.find_last_of(".")));
			const bool bParsed = p.parse(jsonfile, i);
			if (!bParsed)
			{
				std::cerr << "Parse error; invalid JSON.\n";
				return -2;
			}
		}
		return 0;
	}
}

