// Copyright Dan Price 2026.

#pragma once

#include "DrumHit.gen.h"

namespace DrumHit
{
	constexpr const size_t NumOrders = splines.size();
	constexpr const size_t NumZeroes = (NumOrders > 0) ? splines[0].size() : 0;
}

