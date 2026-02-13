// Copyright Dan Price 2026.

#pragma once

#include "IAudioObject.h"
#include "Memory.h"
#include "Oversampler.h"
#include "GaussBoost.h"
#include <type_traits>

namespace json2wav
{
	template<typename T, size_t n> inline constexpr T cheby_poly(T x)
	{
		constexpr const T one(1);
		constexpr const T two(2);
		if constexpr (n == 0)
			return one;
		if constexpr (n == 1)
			return x;
		constexpr const size_t halfn = n >> 1;
		const T chebyhalfn(cheby_poly<T, halfn>(x));
		if constexpr (n & 1)
			return two*chebyhalfn*cheby_poly<T, halfn + 1>(x) - x;
		return two*chebyhalfn*chebyhalfn - one;
	}

	template<size_t order> struct ChebyDistNums
	{
		static constexpr const size_t n_harmonics = 1 << order;
		static constexpr const size_t n_buf_mult = n_harmonics >> 1;
	};

	template<size_t n, typename T> struct sqinv
	{
		static_assert(n > 0, "json2wav::sqinv can't divide by zero");
		static constexpr const T value = static_cast<T>(static_cast<long double>(1)/static_cast<long double>(n*n));
	};

	template<size_t n, typename T> struct cubinv
	{
		static_assert(n > 0, "json2wav::cubinv can't divide by zero");
		static constexpr const T value = static_cast<T>(static_cast<long double>(1)/static_cast<long double>(n*n*n));
	};

	template<size_t n, typename T> struct quainv
	{
		static_assert(n > 0, "json2wav::quainv can't divide by zero");
		static constexpr const T value = static_cast<T>(static_cast<long double>(1)/static_cast<long double>(n*n*n*n));
	};

	template<size_t order, typename T> struct sqinvnorm;
	template<size_t order, typename T> struct cubinvnorm;
	template<size_t order, typename T> struct quainvnorm;
	template<typename T> struct sqinvnorm<2, T>
	{
		static constexpr const T norm = T(18)/T(25);
		static constexpr const T offset = 0.3125;
	};
	template<typename T> struct sqinvnorm<3, T>
	{
		static constexpr const T norm = static_cast<T>(static_cast<long double>(11025)/static_cast<long double>(16141));
		static constexpr const T offset = static_cast<T>(static_cast<long double>(205)/static_cast<long double>(576));
	};
	template<typename T> struct sqinvnorm<4, T>
	{
		static constexpr const T norm = static_cast<T>(static_cast<long double>(81162081)/static_cast<long double>(121726426));
		static constexpr const T offset = static_cast<T>(static_cast<long double>(1077749)/static_cast<long double>(2822400));
	};

#define DEFINE_CHEBY_INV_NORM_ORDER_4(func) \
	template<typename T> struct func##norm<4, T>\
	{\
		using ld_t = long double;\
		static constexpr const ld_t z = 0;\
		static constexpr const ld_t o = 1;\
		static constexpr const ld_t ldoffset = -(cheby_poly<ld_t, 1>(z) + func<2, ld_t>::value*cheby_poly<ld_t, 2>(z) -\
				func<3, ld_t>::value*cheby_poly<ld_t, 3>(z) - func<4, ld_t>::value*cheby_poly<ld_t, 4>(z) +\
				func<5, ld_t>::value*cheby_poly<ld_t, 5>(z) + func<6, ld_t>::value*cheby_poly<ld_t, 6>(z) -\
				func<7, ld_t>::value*cheby_poly<ld_t, 7>(z) - func<8, ld_t>::value*cheby_poly<ld_t, 8>(z) +\
				func<9, ld_t>::value*cheby_poly<ld_t, 9>(z) + func<10, ld_t>::value*cheby_poly<ld_t, 10>(z) -\
				func<11, ld_t>::value*cheby_poly<ld_t, 11>(z) - func<12, ld_t>::value*cheby_poly<ld_t, 12>(z) +\
				func<13, ld_t>::value*cheby_poly<ld_t, 13>(z) + func<14, ld_t>::value*cheby_poly<ld_t, 14>(z) -\
				func<15, ld_t>::value*cheby_poly<ld_t, 15>(z) - func<16, ld_t>::value*cheby_poly<ld_t, 16>(z));\
		static constexpr const T norm = static_cast<T>(\
				o/(ldoffset + cheby_poly<ld_t, 1>(o) + func<2, ld_t>::value*cheby_poly<ld_t, 2>(o) -\
				func<3, ld_t>::value*cheby_poly<ld_t, 3>(o) - func<4, ld_t>::value*cheby_poly<ld_t, 4>(o) +\
				func<5, ld_t>::value*cheby_poly<ld_t, 5>(o) + func<6, ld_t>::value*cheby_poly<ld_t, 6>(o) -\
				func<7, ld_t>::value*cheby_poly<ld_t, 7>(o) - func<8, ld_t>::value*cheby_poly<ld_t, 8>(o) +\
				func<9, ld_t>::value*cheby_poly<ld_t, 9>(o) + func<10, ld_t>::value*cheby_poly<ld_t, 10>(o) -\
				func<11, ld_t>::value*cheby_poly<ld_t, 11>(o) - func<12, ld_t>::value*cheby_poly<ld_t, 12>(o) +\
				func<13, ld_t>::value*cheby_poly<ld_t, 13>(o) + func<14, ld_t>::value*cheby_poly<ld_t, 14>(o) -\
				func<15, ld_t>::value*cheby_poly<ld_t, 15>(o) - func<16, ld_t>::value*cheby_poly<ld_t, 16>(o)));\
		static constexpr const T offset = static_cast<T>(ldoffset);\
	}
	DEFINE_CHEBY_INV_NORM_ORDER_4(cubinv);
	DEFINE_CHEBY_INV_NORM_ORDER_4(quainv);

#define DEFINE_CHEBY_INV_NORM_ORDER_5(func) \
	template<typename T> struct func##norm<5, T>\
	{\
		using ld_t = long double;\
		static constexpr const ld_t z = 0;\
		static constexpr const ld_t o = 1;\
		static constexpr const ld_t ldoffset = -(cheby_poly<ld_t, 1>(z) + func<2, ld_t>::value*cheby_poly<ld_t, 2>(z) -\
				func<3, ld_t>::value*cheby_poly<ld_t, 3>(z) - func<4, ld_t>::value*cheby_poly<ld_t, 4>(z) +\
				func<5, ld_t>::value*cheby_poly<ld_t, 5>(z) + func<6, ld_t>::value*cheby_poly<ld_t, 6>(z) -\
				func<7, ld_t>::value*cheby_poly<ld_t, 7>(z) - func<8, ld_t>::value*cheby_poly<ld_t, 8>(z) +\
				func<9, ld_t>::value*cheby_poly<ld_t, 9>(z) + func<10, ld_t>::value*cheby_poly<ld_t, 10>(z) -\
				func<11, ld_t>::value*cheby_poly<ld_t, 11>(z) - func<12, ld_t>::value*cheby_poly<ld_t, 12>(z) +\
				func<13, ld_t>::value*cheby_poly<ld_t, 13>(z) + func<14, ld_t>::value*cheby_poly<ld_t, 14>(z) -\
				func<15, ld_t>::value*cheby_poly<ld_t, 15>(z) - func<16, ld_t>::value*cheby_poly<ld_t, 16>(z) +\
				func<17, ld_t>::value*cheby_poly<ld_t, 17>(z) + func<18, ld_t>::value*cheby_poly<ld_t, 18>(z) -\
				func<19, ld_t>::value*cheby_poly<ld_t, 19>(z) - func<20, ld_t>::value*cheby_poly<ld_t, 20>(z) +\
				func<21, ld_t>::value*cheby_poly<ld_t, 21>(z) + func<22, ld_t>::value*cheby_poly<ld_t, 22>(z) -\
				func<23, ld_t>::value*cheby_poly<ld_t, 23>(z) - func<24, ld_t>::value*cheby_poly<ld_t, 24>(z) +\
				func<25, ld_t>::value*cheby_poly<ld_t, 25>(z) + func<26, ld_t>::value*cheby_poly<ld_t, 26>(z) -\
				func<27, ld_t>::value*cheby_poly<ld_t, 27>(z) - func<28, ld_t>::value*cheby_poly<ld_t, 28>(z) +\
				func<29, ld_t>::value*cheby_poly<ld_t, 29>(z) + func<30, ld_t>::value*cheby_poly<ld_t, 30>(z) -\
				func<31, ld_t>::value*cheby_poly<ld_t, 31>(z) - func<32, ld_t>::value*cheby_poly<ld_t, 32>(z));\
		static constexpr const T norm = static_cast<T>(\
				o/(ldoffset + cheby_poly<ld_t, 1>(o) + func<2, ld_t>::value*cheby_poly<ld_t, 2>(o) -\
				func<3, ld_t>::value*cheby_poly<ld_t, 3>(o) - func<4, ld_t>::value*cheby_poly<ld_t, 4>(o) +\
				func<5, ld_t>::value*cheby_poly<ld_t, 5>(o) + func<6, ld_t>::value*cheby_poly<ld_t, 6>(o) -\
				func<7, ld_t>::value*cheby_poly<ld_t, 7>(o) - func<8, ld_t>::value*cheby_poly<ld_t, 8>(o) +\
				func<9, ld_t>::value*cheby_poly<ld_t, 9>(o) + func<10, ld_t>::value*cheby_poly<ld_t, 10>(o) -\
				func<11, ld_t>::value*cheby_poly<ld_t, 11>(o) - func<12, ld_t>::value*cheby_poly<ld_t, 12>(o) +\
				func<13, ld_t>::value*cheby_poly<ld_t, 13>(o) + func<14, ld_t>::value*cheby_poly<ld_t, 14>(o) -\
				func<15, ld_t>::value*cheby_poly<ld_t, 15>(o) - func<16, ld_t>::value*cheby_poly<ld_t, 16>(o) +\
				func<17, ld_t>::value*cheby_poly<ld_t, 17>(o) + func<18, ld_t>::value*cheby_poly<ld_t, 18>(o) -\
				func<19, ld_t>::value*cheby_poly<ld_t, 19>(o) - func<20, ld_t>::value*cheby_poly<ld_t, 20>(o) +\
				func<21, ld_t>::value*cheby_poly<ld_t, 21>(o) + func<22, ld_t>::value*cheby_poly<ld_t, 22>(o) -\
				func<23, ld_t>::value*cheby_poly<ld_t, 23>(o) - func<24, ld_t>::value*cheby_poly<ld_t, 24>(o) +\
				func<25, ld_t>::value*cheby_poly<ld_t, 25>(o) + func<26, ld_t>::value*cheby_poly<ld_t, 26>(o) -\
				func<27, ld_t>::value*cheby_poly<ld_t, 27>(o) - func<28, ld_t>::value*cheby_poly<ld_t, 28>(o) +\
				func<29, ld_t>::value*cheby_poly<ld_t, 29>(o) + func<30, ld_t>::value*cheby_poly<ld_t, 30>(o) -\
				func<31, ld_t>::value*cheby_poly<ld_t, 31>(o) - func<32, ld_t>::value*cheby_poly<ld_t, 32>(o)));\
		static constexpr const T offset = static_cast<T>(ldoffset);\
	}
	DEFINE_CHEBY_INV_NORM_ORDER_5(sqinv);
	DEFINE_CHEBY_INV_NORM_ORDER_5(cubinv);
	DEFINE_CHEBY_INV_NORM_ORDER_5(quainv);

#define DEFINE_CHEBY_INV_NORM_ORDER_6(func) \
	template<typename T> struct func##norm<6, T>\
	{\
		using ld_t = long double;\
		static constexpr const ld_t z = 0;\
		static constexpr const ld_t o = 1;\
		static constexpr const ld_t ldoffset = -(cheby_poly<ld_t, 1>(z) + func<2, ld_t>::value*cheby_poly<ld_t, 2>(z) -\
				func<3, ld_t>::value*cheby_poly<ld_t, 3>(z) - func<4, ld_t>::value*cheby_poly<ld_t, 4>(z) +\
				func<5, ld_t>::value*cheby_poly<ld_t, 5>(z) + func<6, ld_t>::value*cheby_poly<ld_t, 6>(z) -\
				func<7, ld_t>::value*cheby_poly<ld_t, 7>(z) - func<8, ld_t>::value*cheby_poly<ld_t, 8>(z) +\
				func<9, ld_t>::value*cheby_poly<ld_t, 9>(z) + func<10, ld_t>::value*cheby_poly<ld_t, 10>(z) -\
				func<11, ld_t>::value*cheby_poly<ld_t, 11>(z) - func<12, ld_t>::value*cheby_poly<ld_t, 12>(z) +\
				func<13, ld_t>::value*cheby_poly<ld_t, 13>(z) + func<14, ld_t>::value*cheby_poly<ld_t, 14>(z) -\
				func<15, ld_t>::value*cheby_poly<ld_t, 15>(z) - func<16, ld_t>::value*cheby_poly<ld_t, 16>(z) +\
				func<17, ld_t>::value*cheby_poly<ld_t, 17>(z) + func<18, ld_t>::value*cheby_poly<ld_t, 18>(z) -\
				func<19, ld_t>::value*cheby_poly<ld_t, 19>(z) - func<20, ld_t>::value*cheby_poly<ld_t, 20>(z) +\
				func<21, ld_t>::value*cheby_poly<ld_t, 21>(z) + func<22, ld_t>::value*cheby_poly<ld_t, 22>(z) -\
				func<23, ld_t>::value*cheby_poly<ld_t, 23>(z) - func<24, ld_t>::value*cheby_poly<ld_t, 24>(z) +\
				func<25, ld_t>::value*cheby_poly<ld_t, 25>(z) + func<26, ld_t>::value*cheby_poly<ld_t, 26>(z) -\
				func<27, ld_t>::value*cheby_poly<ld_t, 27>(z) - func<28, ld_t>::value*cheby_poly<ld_t, 28>(z) +\
				func<29, ld_t>::value*cheby_poly<ld_t, 29>(z) + func<30, ld_t>::value*cheby_poly<ld_t, 30>(z) -\
				func<31, ld_t>::value*cheby_poly<ld_t, 31>(z) - func<32, ld_t>::value*cheby_poly<ld_t, 32>(z) +\
				func<33, ld_t>::value*cheby_poly<ld_t, 33>(z) + func<34, ld_t>::value*cheby_poly<ld_t, 34>(z) -\
				func<35, ld_t>::value*cheby_poly<ld_t, 35>(z) - func<36, ld_t>::value*cheby_poly<ld_t, 36>(z) +\
				func<37, ld_t>::value*cheby_poly<ld_t, 37>(z) + func<38, ld_t>::value*cheby_poly<ld_t, 38>(z) -\
				func<39, ld_t>::value*cheby_poly<ld_t, 39>(z) - func<40, ld_t>::value*cheby_poly<ld_t, 40>(z) +\
				func<41, ld_t>::value*cheby_poly<ld_t, 41>(z) + func<42, ld_t>::value*cheby_poly<ld_t, 42>(z) -\
				func<43, ld_t>::value*cheby_poly<ld_t, 43>(z) - func<44, ld_t>::value*cheby_poly<ld_t, 44>(z) +\
				func<45, ld_t>::value*cheby_poly<ld_t, 45>(z) + func<46, ld_t>::value*cheby_poly<ld_t, 46>(z) -\
				func<47, ld_t>::value*cheby_poly<ld_t, 47>(z) - func<48, ld_t>::value*cheby_poly<ld_t, 48>(z) +\
				func<49, ld_t>::value*cheby_poly<ld_t, 49>(z) + func<50, ld_t>::value*cheby_poly<ld_t, 50>(z) -\
				func<51, ld_t>::value*cheby_poly<ld_t, 51>(z) - func<52, ld_t>::value*cheby_poly<ld_t, 52>(z) +\
				func<53, ld_t>::value*cheby_poly<ld_t, 53>(z) + func<54, ld_t>::value*cheby_poly<ld_t, 54>(z) -\
				func<55, ld_t>::value*cheby_poly<ld_t, 55>(z) - func<56, ld_t>::value*cheby_poly<ld_t, 56>(z) +\
				func<57, ld_t>::value*cheby_poly<ld_t, 57>(z) + func<58, ld_t>::value*cheby_poly<ld_t, 58>(z) -\
				func<59, ld_t>::value*cheby_poly<ld_t, 59>(z) - func<60, ld_t>::value*cheby_poly<ld_t, 60>(z) +\
				func<61, ld_t>::value*cheby_poly<ld_t, 61>(z) + func<62, ld_t>::value*cheby_poly<ld_t, 62>(z) -\
				func<63, ld_t>::value*cheby_poly<ld_t, 63>(z) - func<64, ld_t>::value*cheby_poly<ld_t, 64>(z));\
		static constexpr const T norm = static_cast<T>(\
				o/(ldoffset + cheby_poly<ld_t, 1>(o) + func<2, ld_t>::value*cheby_poly<ld_t, 2>(o) -\
				func<3, ld_t>::value*cheby_poly<ld_t, 3>(o) - func<4, ld_t>::value*cheby_poly<ld_t, 4>(o) +\
				func<5, ld_t>::value*cheby_poly<ld_t, 5>(o) + func<6, ld_t>::value*cheby_poly<ld_t, 6>(o) -\
				func<7, ld_t>::value*cheby_poly<ld_t, 7>(o) - func<8, ld_t>::value*cheby_poly<ld_t, 8>(o) +\
				func<9, ld_t>::value*cheby_poly<ld_t, 9>(o) + func<10, ld_t>::value*cheby_poly<ld_t, 10>(o) -\
				func<11, ld_t>::value*cheby_poly<ld_t, 11>(o) - func<12, ld_t>::value*cheby_poly<ld_t, 12>(o) +\
				func<13, ld_t>::value*cheby_poly<ld_t, 13>(o) + func<14, ld_t>::value*cheby_poly<ld_t, 14>(o) -\
				func<15, ld_t>::value*cheby_poly<ld_t, 15>(o) - func<16, ld_t>::value*cheby_poly<ld_t, 16>(o) +\
				func<17, ld_t>::value*cheby_poly<ld_t, 17>(o) + func<18, ld_t>::value*cheby_poly<ld_t, 18>(o) -\
				func<19, ld_t>::value*cheby_poly<ld_t, 19>(o) - func<20, ld_t>::value*cheby_poly<ld_t, 20>(o) +\
				func<21, ld_t>::value*cheby_poly<ld_t, 21>(o) + func<22, ld_t>::value*cheby_poly<ld_t, 22>(o) -\
				func<23, ld_t>::value*cheby_poly<ld_t, 23>(o) - func<24, ld_t>::value*cheby_poly<ld_t, 24>(o) +\
				func<25, ld_t>::value*cheby_poly<ld_t, 25>(o) + func<26, ld_t>::value*cheby_poly<ld_t, 26>(o) -\
				func<27, ld_t>::value*cheby_poly<ld_t, 27>(o) - func<28, ld_t>::value*cheby_poly<ld_t, 28>(o) +\
				func<29, ld_t>::value*cheby_poly<ld_t, 29>(o) + func<30, ld_t>::value*cheby_poly<ld_t, 30>(o) -\
				func<31, ld_t>::value*cheby_poly<ld_t, 31>(o) - func<32, ld_t>::value*cheby_poly<ld_t, 32>(o) +\
				func<33, ld_t>::value*cheby_poly<ld_t, 33>(o) + func<34, ld_t>::value*cheby_poly<ld_t, 34>(o) -\
				func<35, ld_t>::value*cheby_poly<ld_t, 35>(o) - func<36, ld_t>::value*cheby_poly<ld_t, 36>(o) +\
				func<37, ld_t>::value*cheby_poly<ld_t, 37>(o) + func<38, ld_t>::value*cheby_poly<ld_t, 38>(o) -\
				func<39, ld_t>::value*cheby_poly<ld_t, 39>(o) - func<40, ld_t>::value*cheby_poly<ld_t, 40>(o) +\
				func<41, ld_t>::value*cheby_poly<ld_t, 41>(o) + func<42, ld_t>::value*cheby_poly<ld_t, 42>(o) -\
				func<43, ld_t>::value*cheby_poly<ld_t, 43>(o) - func<44, ld_t>::value*cheby_poly<ld_t, 44>(o) +\
				func<45, ld_t>::value*cheby_poly<ld_t, 45>(o) + func<46, ld_t>::value*cheby_poly<ld_t, 46>(o) -\
				func<47, ld_t>::value*cheby_poly<ld_t, 47>(o) - func<48, ld_t>::value*cheby_poly<ld_t, 48>(o) +\
				func<49, ld_t>::value*cheby_poly<ld_t, 49>(o) + func<50, ld_t>::value*cheby_poly<ld_t, 50>(o) -\
				func<51, ld_t>::value*cheby_poly<ld_t, 51>(o) - func<52, ld_t>::value*cheby_poly<ld_t, 52>(o) +\
				func<53, ld_t>::value*cheby_poly<ld_t, 53>(o) + func<54, ld_t>::value*cheby_poly<ld_t, 54>(o) -\
				func<55, ld_t>::value*cheby_poly<ld_t, 55>(o) - func<56, ld_t>::value*cheby_poly<ld_t, 56>(o) +\
				func<57, ld_t>::value*cheby_poly<ld_t, 57>(o) + func<58, ld_t>::value*cheby_poly<ld_t, 58>(o) -\
				func<59, ld_t>::value*cheby_poly<ld_t, 59>(o) - func<60, ld_t>::value*cheby_poly<ld_t, 60>(o) +\
				func<61, ld_t>::value*cheby_poly<ld_t, 61>(o) + func<62, ld_t>::value*cheby_poly<ld_t, 62>(o) -\
				func<63, ld_t>::value*cheby_poly<ld_t, 63>(o) - func<64, ld_t>::value*cheby_poly<ld_t, 64>(o)));\
		static constexpr const T offset = static_cast<T>(ldoffset);\
	}
	DEFINE_CHEBY_INV_NORM_ORDER_6(sqinv);
	DEFINE_CHEBY_INV_NORM_ORDER_6(cubinv);
	DEFINE_CHEBY_INV_NORM_ORDER_6(quainv);

	enum class EChebyDistWaveShaper
	{
		InverseSquare,
		InverseSquareGaussianBoost,
		InverseCube,
		InverseQuart
	};

	template<EChebyDistWaveShaper eWaveShaper, size_t order, typename T> struct cheby_coeff;

	template<size_t order, typename T> struct cheby_coeff<EChebyDistWaveShaper::InverseSquare, order, T>
	{
		static constexpr const T value = sqinv<order, T>::value;
		static constexpr const T norm = sqinvnorm<order, T>::norm;
		static constexpr const T offset = sqinvnorm<order, T>::offset;
	};

	template<size_t order, typename T> struct cheby_coeff<EChebyDistWaveShaper::InverseSquareGaussianBoost, order, T>
	{
		static constexpr const T value = gauss_boost<order, T>::value;
		static constexpr const T norm = gauss_boost<order, T>::norm;
		static constexpr const T offset = gauss_boost<order, T>::offset;
	};

	template<size_t order, typename T> struct cheby_coeff<EChebyDistWaveShaper::InverseCube, order, T>
	{
		static constexpr const T value = cubinv<order, T>::value;
		static constexpr const T norm = cubinvnorm<order, T>::norm;
		static constexpr const T offset = cubinvnorm<order, T>::offset;
	};

	template<size_t order, typename T> struct cheby_coeff<EChebyDistWaveShaper::InverseQuart, order, T>
	{
		static constexpr const T value = quainv<order, T>::value;
		static constexpr const T norm = quainvnorm<order, T>::norm;
		static constexpr const T offset = quainvnorm<order, T>::offset;
	};

	template<size_t order> struct ChebyDistProcImpl;
	template<> struct ChebyDistProcImpl<2>
	{
		// order 2: 2^2 = 4 harmonics
		template<EChebyDistWaveShaper eWaveShaper, typename T> static constexpr T Process(T x)
		{
			return cheby_coeff<eWaveShaper, 2, T>::norm*(cheby_coeff<eWaveShaper, 2, T>::offset +
					((cheby_coeff<eWaveShaper, 1, T>::value*cheby_poly<T, 1>(x) + cheby_coeff<eWaveShaper, 2, T>::value*cheby_poly<T, 2>(x)) +
					(-cheby_coeff<eWaveShaper, 3, T>::value*cheby_poly<T, 3>(x) - cheby_coeff<eWaveShaper, 4, T>::value*cheby_poly<T, 4>(x))));
		}
	};
	template<> struct ChebyDistProcImpl<3>
	{
		// order 3: 2^3 = 8 harmonics
		template<EChebyDistWaveShaper eWaveShaper, typename T> static constexpr T Process(T x)
		{

			return cheby_coeff<eWaveShaper, 3, T>::norm*(cheby_coeff<eWaveShaper, 3, T>::offset +
					(((cheby_coeff<eWaveShaper, 1, T>::value*cheby_poly<T, 1>(x) + cheby_coeff<eWaveShaper, 2, T>::value*cheby_poly<T, 2>(x)) +
					(-cheby_coeff<eWaveShaper, 3, T>::value*cheby_poly<T, 3>(x) - cheby_coeff<eWaveShaper, 4, T>::value*cheby_poly<T, 4>(x))) +
					((cheby_coeff<eWaveShaper, 5, T>::value*cheby_poly<T, 5>(x) + cheby_coeff<eWaveShaper, 6, T>::value*cheby_poly<T, 6>(x)) +
					(-cheby_coeff<eWaveShaper, 7, T>::value*cheby_poly<T, 7>(x) - cheby_coeff<eWaveShaper, 8, T>::value*cheby_poly<T, 8>(x)))));
		}
	};
	template<> struct ChebyDistProcImpl<4>
	{
		// order 4: 2^4 = 16 harmonics
		template<EChebyDistWaveShaper eWaveShaper, typename T> static constexpr T Process(T x)
		{
			return cheby_coeff<eWaveShaper, 4, T>::norm*(cheby_coeff<eWaveShaper, 4, T>::offset +
					((((cheby_coeff<eWaveShaper, 1, T>::value*cheby_poly<T, 1>(x) + cheby_coeff<eWaveShaper, 2, T>::value*cheby_poly<T, 2>(x)) +
					(-cheby_coeff<eWaveShaper, 3, T>::value*cheby_poly<T, 3>(x) - cheby_coeff<eWaveShaper, 4, T>::value*cheby_poly<T, 4>(x))) +
					((cheby_coeff<eWaveShaper, 5, T>::value*cheby_poly<T, 5>(x) + cheby_coeff<eWaveShaper, 6, T>::value*cheby_poly<T, 6>(x)) +
					(-cheby_coeff<eWaveShaper, 7, T>::value*cheby_poly<T, 7>(x) - cheby_coeff<eWaveShaper, 8, T>::value*cheby_poly<T, 8>(x)))) +
					(((cheby_coeff<eWaveShaper, 9, T>::value*cheby_poly<T, 9>(x) + cheby_coeff<eWaveShaper, 10, T>::value*cheby_poly<T, 10>(x)) +
					(-cheby_coeff<eWaveShaper, 11, T>::value*cheby_poly<T, 11>(x) - cheby_coeff<eWaveShaper, 12, T>::value*cheby_poly<T, 12>(x))) +
					((cheby_coeff<eWaveShaper, 13, T>::value*cheby_poly<T, 13>(x) + cheby_coeff<eWaveShaper, 14, T>::value*cheby_poly<T, 14>(x)) +
					(-cheby_coeff<eWaveShaper, 15, T>::value*cheby_poly<T, 15>(x) - cheby_coeff<eWaveShaper, 16, T>::value*cheby_poly<T, 16>(x))))));
		}
	};
	template<> struct ChebyDistProcImpl<5>
	{
		// order 5: 2^5 = 32 harmonics
		template<EChebyDistWaveShaper eWaveShaper, typename T> static constexpr T Process(T x)
		{
			return cheby_coeff<eWaveShaper, 5, T>::norm*(cheby_coeff<eWaveShaper, 5, T>::offset +
					(((((cheby_coeff<eWaveShaper, 1, T>::value*cheby_poly<T, 1>(x) + cheby_coeff<eWaveShaper, 2, T>::value*cheby_poly<T, 2>(x)) +
					(-cheby_coeff<eWaveShaper, 3, T>::value*cheby_poly<T, 3>(x) - cheby_coeff<eWaveShaper, 4, T>::value*cheby_poly<T, 4>(x))) +
					((cheby_coeff<eWaveShaper, 5, T>::value*cheby_poly<T, 5>(x) + cheby_coeff<eWaveShaper, 6, T>::value*cheby_poly<T, 6>(x)) +
					(-cheby_coeff<eWaveShaper, 7, T>::value*cheby_poly<T, 7>(x) - cheby_coeff<eWaveShaper, 8, T>::value*cheby_poly<T, 8>(x)))) +
					(((cheby_coeff<eWaveShaper, 9, T>::value*cheby_poly<T, 9>(x) + cheby_coeff<eWaveShaper, 10, T>::value*cheby_poly<T, 10>(x)) +
					(-cheby_coeff<eWaveShaper, 11, T>::value*cheby_poly<T, 11>(x) - cheby_coeff<eWaveShaper, 12, T>::value*cheby_poly<T, 12>(x))) +
					((cheby_coeff<eWaveShaper, 13, T>::value*cheby_poly<T, 13>(x) + cheby_coeff<eWaveShaper, 14, T>::value*cheby_poly<T, 14>(x)) +
					(-cheby_coeff<eWaveShaper, 15, T>::value*cheby_poly<T, 15>(x) - cheby_coeff<eWaveShaper, 16, T>::value*cheby_poly<T, 16>(x))))) +
					((((cheby_coeff<eWaveShaper, 17, T>::value*cheby_poly<T, 17>(x) + cheby_coeff<eWaveShaper, 18, T>::value*cheby_poly<T, 18>(x)) +
					(-cheby_coeff<eWaveShaper, 19, T>::value*cheby_poly<T, 19>(x) - cheby_coeff<eWaveShaper, 20, T>::value*cheby_poly<T, 20>(x))) +
					((cheby_coeff<eWaveShaper, 21, T>::value*cheby_poly<T, 21>(x) + cheby_coeff<eWaveShaper, 22, T>::value*cheby_poly<T, 22>(x)) +
					(-cheby_coeff<eWaveShaper, 23, T>::value*cheby_poly<T, 23>(x) - cheby_coeff<eWaveShaper, 24, T>::value*cheby_poly<T, 24>(x)))) +
					(((cheby_coeff<eWaveShaper, 25, T>::value*cheby_poly<T, 25>(x) + cheby_coeff<eWaveShaper, 26, T>::value*cheby_poly<T, 26>(x)) +
					(-cheby_coeff<eWaveShaper, 27, T>::value*cheby_poly<T, 27>(x) - cheby_coeff<eWaveShaper, 28, T>::value*cheby_poly<T, 28>(x))) +
					((cheby_coeff<eWaveShaper, 29, T>::value*cheby_poly<T, 29>(x) + cheby_coeff<eWaveShaper, 30, T>::value*cheby_poly<T, 30>(x)) +
					(-cheby_coeff<eWaveShaper, 31, T>::value*cheby_poly<T, 31>(x) - cheby_coeff<eWaveShaper, 32, T>::value*cheby_poly<T, 32>(x)))))));
		}
	};
	template<> struct ChebyDistProcImpl<6>
	{
		// order 6: 2^6 = 64 harmonics
		template<EChebyDistWaveShaper eWaveShaper, typename T> static constexpr T Process(T x)
		{
			return cheby_coeff<eWaveShaper, 6, T>::norm*(cheby_coeff<eWaveShaper, 6, T>::offset +
					((((((cheby_coeff<eWaveShaper, 1, T>::value*cheby_poly<T, 1>(x) + cheby_coeff<eWaveShaper, 2, T>::value*cheby_poly<T, 2>(x)) +
					(-cheby_coeff<eWaveShaper, 3, T>::value*cheby_poly<T, 3>(x) - cheby_coeff<eWaveShaper, 4, T>::value*cheby_poly<T, 4>(x))) +
					((cheby_coeff<eWaveShaper, 5, T>::value*cheby_poly<T, 5>(x) + cheby_coeff<eWaveShaper, 6, T>::value*cheby_poly<T, 6>(x)) +
					(-cheby_coeff<eWaveShaper, 7, T>::value*cheby_poly<T, 7>(x) - cheby_coeff<eWaveShaper, 8, T>::value*cheby_poly<T, 8>(x)))) +
					(((cheby_coeff<eWaveShaper, 9, T>::value*cheby_poly<T, 9>(x) + cheby_coeff<eWaveShaper, 10, T>::value*cheby_poly<T, 10>(x)) +
					(-cheby_coeff<eWaveShaper, 11, T>::value*cheby_poly<T, 11>(x) - cheby_coeff<eWaveShaper, 12, T>::value*cheby_poly<T, 12>(x))) +
					((cheby_coeff<eWaveShaper, 13, T>::value*cheby_poly<T, 13>(x) + cheby_coeff<eWaveShaper, 14, T>::value*cheby_poly<T, 14>(x)) +
					(-cheby_coeff<eWaveShaper, 15, T>::value*cheby_poly<T, 15>(x) - cheby_coeff<eWaveShaper, 16, T>::value*cheby_poly<T, 16>(x))))) +
					((((cheby_coeff<eWaveShaper, 17, T>::value*cheby_poly<T, 17>(x) + cheby_coeff<eWaveShaper, 18, T>::value*cheby_poly<T, 18>(x)) +
					(-cheby_coeff<eWaveShaper, 19, T>::value*cheby_poly<T, 19>(x) - cheby_coeff<eWaveShaper, 20, T>::value*cheby_poly<T, 20>(x))) +
					((cheby_coeff<eWaveShaper, 21, T>::value*cheby_poly<T, 21>(x) + cheby_coeff<eWaveShaper, 22, T>::value*cheby_poly<T, 22>(x)) +
					(-cheby_coeff<eWaveShaper, 23, T>::value*cheby_poly<T, 23>(x) - cheby_coeff<eWaveShaper, 24, T>::value*cheby_poly<T, 24>(x)))) +
					(((cheby_coeff<eWaveShaper, 25, T>::value*cheby_poly<T, 25>(x) + cheby_coeff<eWaveShaper, 26, T>::value*cheby_poly<T, 26>(x)) +
					(-cheby_coeff<eWaveShaper, 27, T>::value*cheby_poly<T, 27>(x) - cheby_coeff<eWaveShaper, 28, T>::value*cheby_poly<T, 28>(x))) +
					((cheby_coeff<eWaveShaper, 29, T>::value*cheby_poly<T, 29>(x) + cheby_coeff<eWaveShaper, 30, T>::value*cheby_poly<T, 30>(x)) +
					(-cheby_coeff<eWaveShaper, 31, T>::value*cheby_poly<T, 31>(x) - cheby_coeff<eWaveShaper, 32, T>::value*cheby_poly<T, 32>(x)))))) +
					(((((cheby_coeff<eWaveShaper, 33, T>::value*cheby_poly<T, 33>(x) + cheby_coeff<eWaveShaper, 34, T>::value*cheby_poly<T, 34>(x)) +
					(-cheby_coeff<eWaveShaper, 35, T>::value*cheby_poly<T, 35>(x) - cheby_coeff<eWaveShaper, 36, T>::value*cheby_poly<T, 36>(x))) +
					((cheby_coeff<eWaveShaper, 37, T>::value*cheby_poly<T, 37>(x) + cheby_coeff<eWaveShaper, 38, T>::value*cheby_poly<T, 38>(x)) +
					(-cheby_coeff<eWaveShaper, 39, T>::value*cheby_poly<T, 39>(x) - cheby_coeff<eWaveShaper, 40, T>::value*cheby_poly<T, 40>(x)))) +
					(((cheby_coeff<eWaveShaper, 41, T>::value*cheby_poly<T, 41>(x) + cheby_coeff<eWaveShaper, 42, T>::value*cheby_poly<T, 42>(x)) +
					(-cheby_coeff<eWaveShaper, 43, T>::value*cheby_poly<T, 43>(x) - cheby_coeff<eWaveShaper, 44, T>::value*cheby_poly<T, 44>(x))) +
					((cheby_coeff<eWaveShaper, 45, T>::value*cheby_poly<T, 45>(x) + cheby_coeff<eWaveShaper, 46, T>::value*cheby_poly<T, 46>(x)) +
					(-cheby_coeff<eWaveShaper, 47, T>::value*cheby_poly<T, 47>(x) - cheby_coeff<eWaveShaper, 48, T>::value*cheby_poly<T, 48>(x))))) +
					((((cheby_coeff<eWaveShaper, 49, T>::value*cheby_poly<T, 49>(x) + cheby_coeff<eWaveShaper, 50, T>::value*cheby_poly<T, 50>(x)) +
					(-cheby_coeff<eWaveShaper, 51, T>::value*cheby_poly<T, 51>(x) - cheby_coeff<eWaveShaper, 52, T>::value*cheby_poly<T, 52>(x))) +
					((cheby_coeff<eWaveShaper, 53, T>::value*cheby_poly<T, 53>(x) + cheby_coeff<eWaveShaper, 54, T>::value*cheby_poly<T, 54>(x)) +
					(-cheby_coeff<eWaveShaper, 55, T>::value*cheby_poly<T, 55>(x) - cheby_coeff<eWaveShaper, 56, T>::value*cheby_poly<T, 56>(x)))) +
					(((cheby_coeff<eWaveShaper, 57, T>::value*cheby_poly<T, 57>(x) + cheby_coeff<eWaveShaper, 58, T>::value*cheby_poly<T, 58>(x)) +
					(-cheby_coeff<eWaveShaper, 59, T>::value*cheby_poly<T, 59>(x) - cheby_coeff<eWaveShaper, 60, T>::value*cheby_poly<T, 60>(x))) +
					((cheby_coeff<eWaveShaper, 61, T>::value*cheby_poly<T, 61>(x) + cheby_coeff<eWaveShaper, 62, T>::value*cheby_poly<T, 62>(x)) +
					(-cheby_coeff<eWaveShaper, 63, T>::value*cheby_poly<T, 63>(x) - cheby_coeff<eWaveShaper, 64, T>::value*cheby_poly<T, 64>(x))))))));

		}
	};

	template<size_t order> struct ChebyDistProc
	{
		template<EChebyDistWaveShaper eWaveShaper, typename T> static constexpr T Process(T x)
		{
			constexpr const T y0(ChebyDistProcImpl<order>::template Process<eWaveShaper, T>(static_cast<T>(0)));
			constexpr const T ynorm(static_cast<T>(1)/(ChebyDistProcImpl<order>::template Process<eWaveShaper, T>(static_cast<T>(1)) - y0));
			return (ChebyDistProcImpl<order>::template Process<eWaveShaper, T>(x) - y0)*ynorm;
		}
	};

	template<size_t order> struct ChebyDistOversampling;
	template<> struct ChebyDistOversampling<2>
	{
		template<typename sample_t> using upsampler_t = oversampling::upsampler441_x2<sample_t>;
		template<typename sample_t> using downsampler_t = oversampling::downsampler441_x2<sample_t>;
	};
	template<> struct ChebyDistOversampling<3>
	{
		template<typename sample_t> using upsampler_t = oversampling::upsampler441_x4<sample_t>;
		template<typename sample_t> using downsampler_t = oversampling::downsampler441_x4<sample_t>;
	};
	template<> struct ChebyDistOversampling<4>
	{
		template<typename sample_t> using upsampler_t = oversampling::upsampler441_x8<sample_t>;
		template<typename sample_t> using downsampler_t = oversampling::downsampler441_x8<sample_t>;
	};
	template<> struct ChebyDistOversampling<5>
	{
		template<typename sample_t> using upsampler_t = oversampling::upsampler441_x16<sample_t>;
		template<typename sample_t> using downsampler_t = oversampling::downsampler441_x16<sample_t>;
	};
	template<> struct ChebyDistOversampling<6>
	{
		template<typename sample_t> using upsampler_t = oversampling::upsampler441_x32<sample_t>;
		template<typename sample_t> using downsampler_t = oversampling::downsampler441_x32<sample_t>;
	};
	template<> struct ChebyDistOversampling<7>
	{
		template<typename sample_t> using upsampler_t = oversampling::upsampler441_x64<sample_t>;
		template<typename sample_t> using downsampler_t = oversampling::downsampler441_x64<sample_t>;
	};
	template<> struct ChebyDistOversampling<8>
	{
		template<typename sample_t> using upsampler_t = oversampling::upsampler441_x128<sample_t>;
		template<typename sample_t> using downsampler_t = oversampling::downsampler441_x128<sample_t>;
	};

	template<typename sample_t, size_t order, size_t buf_n>
	struct ChebyDistBuf
	{
		static constexpr const size_t n_buf_mult = ChebyDistNums<order>::n_buf_mult;
		static constexpr const size_t bufupn = n_buf_mult*buf_n;
		static constexpr const size_t bufdnn = buf_n;
		sample_t bufup[bufupn];
		sample_t bufdn[buf_n];
		typename ChebyDistOversampling<order>::template upsampler_t<sample_t> upsampler;
		typename ChebyDistOversampling<order>::template downsampler_t<sample_t> downsampler;
		ChebyDistBuf() noexcept : bufup{ 0 }, bufdn{ 0 } {}
	};

	template<size_t order> struct ChebyDistSampleDelay;
	template<> struct ChebyDistSampleDelay<6> { static constexpr const size_t value = 147; };
	template<> struct ChebyDistSampleDelay<5> { static constexpr const size_t value = 146; };
	template<> struct ChebyDistSampleDelay<4> { static constexpr const size_t value = 144; };
	template<> struct ChebyDistSampleDelay<3> { static constexpr const size_t value = 140; };
	template<> struct ChebyDistSampleDelay<2> { static constexpr const size_t value = 128; };

	template<typename sample_t, size_t order, size_t buf_n, EChebyDistWaveShaper eWaveShaper = EChebyDistWaveShaper::InverseSquare, bool bOwner = false>
	class ChebyDist : public AudioSum<bOwner>
	{
		static_assert(std::is_floating_point_v<sample_t>, "ChebyDist sample_t must be a floating point type");
		static_assert(2 <= order && order <= 6, "ChebyDist order must be between 2 and 6, inclusive");
		static_assert((buf_n & (buf_n - 1)) == 0, "ChebyDist buf_n must be a power of 2");

	public:
		ChebyDist()
		{
			osbufs.emplace_back();
			osbufs.emplace_back();
		}

		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t numSamples,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			if (this->GetInputSamples(bufs, numChannels, numSamples, sampleRate)
				!= AudioSum<bOwner>::EGetInputSamplesResult::SamplesWritten)
			{
				return;
			}

			if (osbufs.size() != numChannels)
				osbufs.resize(numChannels);

			const size_t bufmod = numSamples & (buf_n - 1);
			for (size_t ch = 0; ch < numChannels; ++ch)
			{
				auto& chbuf(bufs[ch]);
				auto& osbuf(osbufs[ch]);
				size_t bufpos = 0;
				for (size_t bufend = buf_n; bufend <= numSamples; bufend += buf_n)
				{
					for (size_t i = 0; i < buf_n; ++i)
						osbuf.bufdn[i] = chbuf[bufpos + i];
					osbuf.upsampler.process(osbuf.bufdn, osbuf.bufup);
					for (size_t i = 0; i < osbuf_t::bufupn; ++i)
						osbuf.bufup[i] = ChebyDistProc<order>::template Process<eWaveShaper, sample_t>(osbuf.bufup[i]);
					osbuf.downsampler.process(osbuf.bufup, osbuf.bufdn);
					for (size_t i = 0; i < buf_n; ++i)
						chbuf[bufpos + i] = osbuf.bufdn[i];
					bufpos = bufend;
				}
				if (bufmod > 0)
				{
					for (size_t i = 0; i < bufmod; ++i)
						osbuf.bufdn[i] = chbuf[bufpos + i];
					osbuf.upsampler.process(osbuf.bufdn, osbuf.bufup, bufmod);
					for (size_t i = 0, end = osbuf_t::n_buf_mult*bufmod; i < end; ++i)
						osbuf.bufup[i] = ChebyDistProc<order>::template Process<eWaveShaper, sample_t>(osbuf.bufup[i]);
					osbuf.downsampler.process(osbuf.bufup, osbuf.bufdn, bufmod);
					for (size_t i = 0; i < bufmod; ++i)
						chbuf[bufpos + i] = osbuf.bufdn[i];
				}
			}
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return osbufs.size();
		}

		virtual size_t GetSampleDelay() const noexcept override
		{
			return AudioSum<bOwner>::GetSampleDelay() + ChebyDistSampleDelay<order>::value;
		}

	private:
		typedef ChebyDistBuf<sample_t, order, buf_n> osbuf_t;
		Vector<osbuf_t> osbufs;
	};
}

