// Copyright Dan Price. All rights reserved.

#pragma once

#include "MetaArray.h"

namespace json2wav
{
	namespace Math
	{
		template<typename T, uint_fast8_t order> struct BesselPolyReverse
		//{ static_assert(false, "json2wav::BesselPolyReverse is not implemented for order 0 or orders > 10"); }
		;
		template<typename T> struct BesselPolyReverse<T, 1>
		{ using type = Math::ArrayHolder<T, 1, 1>; };
		template<typename T> struct BesselPolyReverse<T, 2>
		{ using type = Math::ArrayHolder<T, 1, 3, 3>; };
		template<typename T> struct BesselPolyReverse<T, 3>
		{ using type = Math::ArrayHolder<T, 1, 6, 15, 15>; };
		template<typename T> struct BesselPolyReverse<T, 4>
		{ using type = Math::ArrayHolder<T, 1, 10, 45, 105, 105>; };
		template<typename T> struct BesselPolyReverse<T, 5>
		{ using type = Math::ArrayHolder<T, 1, 15, 105, 420, 945, 945>; };
		template<typename T> struct BesselPolyReverse<T, 6>
		{ using type = Math::ArrayHolder<T, 1, 21, 210, 1260, 4725, 10395, 10395>; };
		template<typename T> struct BesselPolyReverse<T, 7>
		{ using type = Math::ArrayHolder<T, 1, 28, 378, 3150, 17325, 62370, 135135, 135135>; };
		template<typename T> struct BesselPolyReverse<T, 8>
		{ using type = Math::ArrayHolder<T, 1, 36, 630, 6930, 51975, 270270, 945945, 2027025, 2027025>; };
		template<typename T> struct BesselPolyReverse<T, 9>
		{ using type = Math::ArrayHolder<T, 1, 45, 990, 13860, 135135, 945945, 4729725, 16216200, 34459425, 34459425>; };
		template<typename T> struct BesselPolyReverse<T, 10>
		{ using type = Math::ArrayHolder<T, 1, 55, 1485, 25740, 315315, 2837835, 18918900, 91891800, 310134825, 654729075, 654729075>; };

		template<typename T, uint_fast8_t n>
		using BesselPolyReverse_t = typename BesselPolyReverse<T, n>::type;
	}
}

