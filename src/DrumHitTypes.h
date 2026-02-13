// Copyright Dan Price 2026.

#pragma once

#include "Cubic.h"
#include "Memory.h"

namespace DrumHit
{
	using PolyType = json2wav::Cubic64;
	using FloatType = double;
	template<typename T, size_t n>
	using Array = json2wav::Array<T, n>;
}

