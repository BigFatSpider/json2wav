// Copyright Dan Price 2026.

#include "JsonToWav.h"
#include "Memory.h"
#include <vector>
#include <string>

int main(int argc, char** argv)
{
#ifdef ALBUMBOT_DEBUGNEW
	json2wav::PrintAllocTimes("at start of main()");
#endif

	if (argc <= 1)
		return -1;
	static const std::string logparam0("-l");
	static const std::string logparam1("--log");
	bool bLog = false;
	json2wav::Vector<std::string> filenames;
	for (int i = 1; i < argc; ++i)
	{
		if (logparam0 == argv[i] || logparam1 == argv[i])
			bLog = true;
		else
			filenames.push_back(argv[i]);
	}
	int result = 0;
	for (const std::string& filename : filenames)
	{
		result = json2wav::JsonToWav(filename.c_str(), bLog);
		if (result != 0)
			return result;
	}
	return result;
}

