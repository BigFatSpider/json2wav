// Copyright Dan Price. All rights reserved.

#include "Quintic.h"
#include "Septic.h"
#include "Nonic.h"

namespace json2wav
{
	constexpr const size_t BLEP_LENGTH = 60;
	constexpr const size_t BLEP_ZERO_CROSSINGS = 25;
	constexpr const size_t BLEP_POLYS = BLEP_LENGTH - 1;
	constexpr const size_t BLEP_PEEK = BLEP_POLYS / 2;
	using BlepPolyType = Septic64;
	constexpr const size_t BLEP_LENGTH_FAST = 20;
	constexpr const size_t BLEP_ZERO_CROSSINGS_FAST = 7;
	constexpr const size_t BLEP_POLYS_FAST = BLEP_LENGTH_FAST - 1;
	constexpr const size_t BLEP_PEEK_FAST = BLEP_POLYS_FAST / 2;
	using BlepPolyType_Fast = Nonic64;
	constexpr const size_t BLEP_LENGTH_XFAST = 16;
	constexpr const size_t BLEP_ZERO_CROSSINGS_XFAST = 5;
	constexpr const size_t BLEP_POLYS_XFAST = BLEP_LENGTH_XFAST - 1;
	constexpr const size_t BLEP_PEEK_XFAST = BLEP_POLYS_XFAST / 2;
	using BlepPolyType_Xfast = Quintic64;
	constexpr const size_t MBLEP_LENGTH = 60;
	constexpr const size_t MBLEP_ZERO_CROSSINGS = 25;
	constexpr const size_t MBLEP_POLYS = MBLEP_LENGTH - 1;
	constexpr const size_t MBLEP_PEEK = MBLEP_POLYS / 2;
	using MBlepPolyType = Septic64;
	constexpr const size_t MBLEP_LENGTH_FAST = 20;
	constexpr const size_t MBLEP_ZERO_CROSSINGS_FAST = 7;
	constexpr const size_t MBLEP_POLYS_FAST = MBLEP_LENGTH_FAST - 1;
	constexpr const size_t MBLEP_PEEK_FAST = MBLEP_POLYS_FAST / 2;
	using MBlepPolyType_Fast = Nonic64;
	constexpr const size_t MBLEP_LENGTH_XFAST = 16;
	constexpr const size_t MBLEP_ZERO_CROSSINGS_XFAST = 5;
	constexpr const size_t MBLEP_POLYS_XFAST = MBLEP_LENGTH_XFAST - 1;
	constexpr const size_t MBLEP_PEEK_XFAST = MBLEP_POLYS_XFAST / 2;
	using MBlepPolyType_Xfast = Quintic64;
	constexpr const size_t RBLEP_LENGTH = 60;
	constexpr const size_t RBLEP_ZERO_CROSSINGS = 25;
	constexpr const size_t RBLEP_POLYS = RBLEP_LENGTH - 1;
	constexpr const size_t RBLEP_PEEK = RBLEP_POLYS / 2;
	using RBlepPolyType = Septic64;
	constexpr const size_t RBLEP_LENGTH_FAST = 20;
	constexpr const size_t RBLEP_ZERO_CROSSINGS_FAST = 7;
	constexpr const size_t RBLEP_POLYS_FAST = RBLEP_LENGTH_FAST - 1;
	constexpr const size_t RBLEP_PEEK_FAST = RBLEP_POLYS_FAST / 2;
	using RBlepPolyType_Fast = Nonic64;
	constexpr const size_t RBLEP_LENGTH_XFAST = 16;
	constexpr const size_t RBLEP_ZERO_CROSSINGS_XFAST = 5;
	constexpr const size_t RBLEP_POLYS_XFAST = RBLEP_LENGTH_XFAST - 1;
	constexpr const size_t RBLEP_PEEK_XFAST = RBLEP_POLYS_XFAST / 2;
	using RBlepPolyType_Xfast = Quintic64;
	constexpr const size_t HBLEP_LENGTH = 60;
	constexpr const size_t HBLEP_ZERO_CROSSINGS = 25;
	constexpr const size_t HBLEP_POLYS = HBLEP_LENGTH - 1;
	constexpr const size_t HBLEP_PEEK = HBLEP_POLYS / 2;
	using HBlepPolyType = Septic64;
	constexpr const size_t HBLEP_LENGTH_FAST = 20;
	constexpr const size_t HBLEP_ZERO_CROSSINGS_FAST = 7;
	constexpr const size_t HBLEP_POLYS_FAST = HBLEP_LENGTH_FAST - 1;
	constexpr const size_t HBLEP_PEEK_FAST = HBLEP_POLYS_FAST / 2;
	using HBlepPolyType_Fast = Nonic64;
	constexpr const size_t HBLEP_LENGTH_XFAST = 16;
	constexpr const size_t HBLEP_ZERO_CROSSINGS_XFAST = 5;
	constexpr const size_t HBLEP_POLYS_XFAST = HBLEP_LENGTH_XFAST - 1;
	constexpr const size_t HBLEP_PEEK_XFAST = HBLEP_POLYS_XFAST / 2;
	using HBlepPolyType_Xfast = Quintic64;
}

