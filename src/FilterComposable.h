// Copyright Dan Price 2026.

#pragma once

#include "Filter.h"
#include "EnveloperComposable.h"
#include "Ramp.h"

namespace json2wav
{
	template<typename FilterType, uint32_t constResetVal>
	class FilterComposable : public EnveloperComposable<FilterType, EFilterParam, EFilterParam::Frequency, EFilterParam::None, true>
	{
	public:
		template<typename... ParamTypes>
		FilterComposable(ParamTypes&&... params)
			: EnveloperComposable<FilterType, EFilterParam, EFilterParam::Frequency, EFilterParam::None, true>(std::forward<ParamTypes>(params)...), resetVal(static_cast<float>(constResetVal))
		{
		}

		void SetResetVal(const float val)
		{
			resetVal = val;
		}

	private:
		virtual float AmpMap(const float amp) const override { return 1.0f; }

		virtual float GetResetVal() const override
		{
			return resetVal;
		}

		float resetVal;
	};

	template<bool bOwner = false, uint_fast8_t numch = 2, Filter::ETopo eTopo = Filter::ETopo::TDF2, uint32_t resetVal = 40>
	using LadderLPComposable = FilterComposable<Filter::LadderLP_Custom<bOwner, numch, eTopo, 4, double, double>, resetVal>;
	template<bool bOwner = false, uint_fast8_t numch = 2, Filter::ETopo eTopo = Filter::ETopo::TDF2, uint32_t resetVal = 40>
	using BiquadLPComposable = FilterComposable<Filter::BiquadLP_Custom<bOwner, numch, eTopo, double>, resetVal>;
	template<bool bOwner = false, uint_fast8_t numch = 2, Filter::ETopo eTopo = Filter::ETopo::TDF2, uint32_t resetVal = 20000>
	using BiquadHPComposable = FilterComposable<Filter::BiquadHP_Custom<bOwner, numch, eTopo, double>, resetVal>;
	template<bool bOwner = false, uint_fast8_t numch = 2, Filter::ETopo eTopo = Filter::ETopo::TDF2, uint32_t resetVal = 1000>
	using BiquadAPComposable = FilterComposable<Filter::BiquadAP_Custom<bOwner, numch, eTopo, double>, resetVal>;
	template<bool bOwner = false, uint_fast8_t numch = 2, Filter::ETopo eTopo = Filter::ETopo::TDF2, uint32_t resetVal = 1000>
	using BiquadNotchComposable = FilterComposable<Filter::BiquadNotch_Custom<bOwner, numch, eTopo, double>, resetVal>;
	template<bool bOwner = false, uint_fast8_t numch = 2, Filter::ETopo eTopo = Filter::ETopo::TDF2, uint32_t resetVal = 1000>
	using BiquadPeakComposable = FilterComposable<Filter::BiquadPeak_Custom<bOwner, numch, eTopo, double>, resetVal>;
	template<bool bOwner = false, uint_fast8_t numch = 2, Filter::ETopo eTopo = Filter::ETopo::TDF2, uint32_t resetVal = 100>
	using BiquadLoShelfComposable = FilterComposable<Filter::BiquadLoShelf_Custom<bOwner, numch, eTopo, double>, resetVal>;
	template<bool bOwner = false, uint_fast8_t numch = 2, Filter::ETopo eTopo = Filter::ETopo::TDF2, uint32_t resetVal = 20000>
	using BiquadHiShelfComposable = FilterComposable<Filter::BiquadHiShelf_Custom<bOwner, numch, eTopo, double>, resetVal>;
}

