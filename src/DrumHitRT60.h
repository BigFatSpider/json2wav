// Copyright Dan Price. All rights reserved.

#pragma once

#include "Bessel.h"
#include <functional>
#include <string>

namespace json2wav
{
	inline std::function<float(size_t, size_t)> GetRT60(const std::string& modecay, const float freq)
	{
		if (modecay == "halfup1")
			return [freq](size_t order, size_t zero)
			{ return 20000.0f/(freq*bessel_harmonics_by_order[order][zero]); };
		else if (modecay == "halfup10")
			return [freq](size_t order, size_t zero)
			{ return 2000.0f/(freq*bessel_harmonics_by_order[order][zero]); };
		else if (modecay == "halfup100")
			return [freq](size_t order, size_t zero)
			{ return 200.0f/(freq*bessel_harmonics_by_order[order][zero]); };
		else if (modecay == "halfup1000")
			return [freq](size_t order, size_t zero)
			{ return 20.0f/(freq*bessel_harmonics_by_order[order][zero]); };
		return [](size_t, size_t) { return 1024.0f; };
	}
}

