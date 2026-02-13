// Copyright Dan Price 2026.

#pragma once

#include "MetaArray.h"

namespace json2wav
{
	namespace Math
	{
		template<typename T, uint_fast8_t order> struct Binomial
		//{ static_assert(false, "json2wav::Binomial is not implemented for order 0 or orders > 16"); }
		;
		template<typename T> struct Binomial<T, 1>
		{ using type = Math::ArrayHolder<T, 1, 1>; };
		template<typename T> struct Binomial<T, 2>
		{ using type = Math::ArrayHolder<T, 1, 2, 1>; };
		template<typename T> struct Binomial<T, 3>
		{ using type = Math::ArrayHolder<T, 1, 3, 3, 1>; };
		template<typename T> struct Binomial<T, 4>
		{ using type = Math::ArrayHolder<T, 1, 4, 6, 4, 1>; };
		template<typename T> struct Binomial<T, 5>
		{ using type = Math::ArrayHolder<T, 1, 5, 10, 10, 5, 1>; };
		template<typename T> struct Binomial<T, 6>
		{ using type = Math::ArrayHolder<T, 1, 6, 15, 20, 15, 6, 1>; };
		template<typename T> struct Binomial<T, 7>
		{ using type = Math::ArrayHolder<T, 1, 7, 21, 35, 35, 21, 7, 1>; };
		template<typename T> struct Binomial<T, 8>
		{ using type = Math::ArrayHolder<T, 1, 8, 28, 56, 70, 56, 28, 8, 1>; };
		template<typename T> struct Binomial<T, 9>
		{ using type = Math::ArrayHolder<T, 1, 9, 36, 84, 126, 126, 84, 36, 9, 1>; };
		template<typename T> struct Binomial<T, 10>
		{ using type = Math::ArrayHolder<T, 1, 10, 45, 120, 210, 252, 210, 120, 45, 10, 1>; };
		template<typename T> struct Binomial<T, 11>
		{ using type = Math::ArrayHolder<T, 1, 11, 55, 165, 330, 462, 462, 330, 165, 55, 11, 1>; };
		template<typename T> struct Binomial<T, 12>
		{ using type = Math::ArrayHolder<T, 1, 12, 66, 220, 495, 792, 924, 792, 495, 220, 66, 12, 1>; };
		template<typename T> struct Binomial<T, 13>
		{ using type = Math::ArrayHolder<T, 1, 13, 78, 286, 715, 1287, 1716, 1716, 1287, 715, 286, 78, 13, 1>; };
		template<typename T> struct Binomial<T, 14>
		{ using type = Math::ArrayHolder<T, 1, 14, 91, 364, 1001, 2002, 3003, 3432, 3003, 2002, 1001, 364, 91, 14, 1>; };
		template<typename T> struct Binomial<T, 15>
		{ using type = Math::ArrayHolder<T, 1, 15, 105, 455, 1365, 3003, 5005, 6435, 6435, 5005, 3003, 1365, 455, 105, 15, 1>; };
		template<typename T> struct Binomial<T, 16>
		{ using type = Math::ArrayHolder<T, 1, 16, 120, 560, 1820, 4368, 8008, 11440, 12870, 11440, 8008, 4368, 1820, 560, 120, 16, 1>; };

		template<typename T, uint_fast8_t n>
		using Binomial_t = typename Binomial<T, n>::type;
	}
}

