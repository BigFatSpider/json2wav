// Copyright Dan Price. All rights reserved.

#include "Random.h"

namespace
{
	using namespace json2wav;

	Seed& getgseed()
	{
		static Seed gseed(RandomSeed());
		return gseed;
	}
}

namespace json2wav
{
	SharedPtr<RNGShared<float>>& getpgrng()
	{
		static SharedPtr<RNGShared<float>> pgrng(MakeShared<RNGShared<float>>(getgseed()));
		return pgrng;
	}

	SharedPtr<RNGShared<double>>& getpgrng64()
	{
		static SharedPtr<RNGShared<double>> pgrng64(MakeShared<RNGShared<double>>(getgseed()));
		return pgrng64;
	}

	RNGShared<float>& grng(*getpgrng());
	RNGShared<double>& grng64(*getpgrng64());
}

