// Copyright Dan Price. All rights reserved.

#pragma once

#include "FastSin.h"

namespace json2wav
{
	namespace detail
	{
		template<typename FloatType> inline FloatType CotHyperbDiff(const FloatType x)
		{
			// Minimal error
			// Evaluated cotx - 1/x at x=0, x=tau/12, x=tau/6+0.12, and x=tau/4
			static constexpr const FloatType a = -0.0454610155381;
			static constexpr const FloatType b = 0.0324787507522;
			static constexpr const FloatType c = -0.344131677192;

			/*
			// Scale to second derivative
			// Evaluated cotx - 1/x at x=0, x=1.04231530824334676231127517, x=1.36703376278722832718638040, and x=tau/4
			static constexpr const FloatType a = -0.0600964575567;
			static constexpr const FloatType b = 0.0718779974694;
			static constexpr const FloatType c = -0.369908363474;
			*/

			return x*(x*(a*x + b) + c);
		}
	}

	template<typename FloatType>
	inline FloatType FastCot(const FloatType x)
	{
		const FloatType x_cyc = x * vHalfTauInv<FloatType>::value;
		const FloatType x_mod = x_cyc - std::floor(x_cyc);
		const FloatType flip = (x_mod < 0.5) ? 1.0 : -1.0;
		const FloatType x_norm = (static_cast<FloatType>(0.5) - flip * static_cast<FloatType>(0.5)) + flip * x_mod * vHalfTau<FloatType>::value;
		return flip * (static_cast<FloatType>(1.0) / x_norm + detail::CotHyperbDiff<FloatType>(x_norm));
	}

	template<typename FloatType>
	inline FloatType FastTan(const FloatType x)
	{
		return FastCot<FloatType>(vQuarterTau<FloatType>::value - x);
	}
}

