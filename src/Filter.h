// Copyright Dan Price 2026.

#pragma once

#include "IAudioObject.h"
#include "IControlObject.h"
#include "Ramp.h"
#include "FastSin.h"
#include "FastTan.h"
#include "Binomial.h"
#include "BesselPoly.h"
#include <utility>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <cmath>

#include <iostream>

#define ALBUMBOT_FILTER_FORM_BIT 1
#define ALBUMBOT_FILTER_TRANSPOSE_BIT 2
#define ALBUMBOT_FILTER_FORM_1 0
#define ALBUMBOT_FILTER_FORM_2 ALBUMBOT_FILTER_FORM_BIT
#define ALBUMBOT_FILTER_TRANSPOSED ALBUMBOT_FILTER_TRANSPOSE_BIT
#define ALBUMBOT_FILTER_TOPO_MASK (ALBUMBOT_FILTER_FORM_BIT | ALBUMBOT_FILTER_TRANSPOSE_BIT)

//#define ALBUMBOT_FILTER_TOPO ALBUMBOT_FILTER_FORM_1
//#define ALBUMBOT_FILTER_TOPO (ALBUMBOT_FILTER_TRANSPOSED | ALBUMBOT_FILTER_FORM_1)
//#define ALBUMBOT_FILTER_TOPO ALBUMBOT_FILTER_FORM_2
//#define ALBUMBOT_FILTER_TOPO (ALBUMBOT_FILTER_TRANSPOSED | ALBUMBOT_FILTER_FORM_2)

#define ALBUMBOT_DELAY_TDF2 0

namespace json2wav
{
	enum class EFilterParam
	{
		None, Frequency, Resonance, Gain
	};
}

namespace json2wav::Filter
{
	using std::sin;

	enum class ETopo
	{
		/*DF1, DirectForm1 = DF1, TDF1, TransposedDirectForm1 = TDF1,*/ DF2, DirectForm2 = DF2, TDF2, TransposedDirectForm2 = TDF2
	};

	template<ETopo eTopo> struct Topo;

	/*template<> struct Topo<ETopo::DF1>
	{
		template<typename FloatType, size_t order>
		static void DoFilter(Sample& smp, FloatType (&z)[order], const FloatType (&a)[order], const FloatType(&b)[order + 1], FloatType* const zo)
		{
			const FloatType smpin = smp.AsFloat32();
			FloatType smpout = b[0] * smpin;
			for (uint_fast8_t j = 0; j < order; ++j)
			{
				smpout += b[j + 1] * z[j] - a[j] * zo[j];
			}
			for (uint_fast8_t j = 1; j < order; ++j)
			{
				z[order - j] = z[order - j - 1];
				zo[order - j] = zo[order - j - 1];
			}
			z[0] = smpin;
			z[0] = smpout;
			smp = static_cast<float>(smpout);
		}
	};

	template<> struct Topo<ETopo::TDF1>
	{
		template<typename FloatType, size_t order>
		static void DoFilter(Sample& smp, FloatType (&z)[order], const FloatType (&a)[order], const FloatType(&b)[order + 1], FloatType* const zo)
		{
			const FloatType smpin = smp.AsFloat32();
			const FloatType mid = smpin + z[0];
			const FloatType smpout = b[0] * mid + zo[0];
			for (uint_fast8_t j = 1; j < order; ++j)
			{
				z[j - 1] = z[j] - a[j - 1] * mid;
				zo[j - 1] = z[j] + b[j] * mid;
			}
			z[order - 1] = -a[order - 1] * mid;
			zo[order - 1] = b[order] * mid;
			smp = static_cast<float>(smpout);
		}
	};*/

	template<> struct Topo<ETopo::DF2>
	{
		template<typename FloatType, size_t order, typename z_t, typename a_t, typename b_t, typename b1_t>
		static void DoFilterGeneric(Sample& inoutsmp, z_t& z, a_t& a, b_t& b, b1_t& b1)
		{
			FloatType smp = inoutsmp.AsFloat32();
			for (uint_fast8_t j = 0; j < order; ++j) // a0 is 1, so a[0] is actually a1
				smp -= a[j] * z[j];
			const FloatType mid = smp;
			smp = b[0] * smp;
			for (uint_fast8_t j = 0; j < order; ++j)
				smp += b[j + 1] * z[j];
			for (uint_fast8_t j = 1; j < order; ++j)
				z[order - j] = z[order - j - 1];
			z[0] = mid;
			inoutsmp = static_cast<float>(smp);
		}

		template<typename FloatType, size_t order>
		static void DoFilter(
			Sample& inoutsmp,
			FloatType (&z)[order],
			const FloatType (&a)[order],
			const FloatType (&b)[order + 1],
			FloatType (&b1)[order + 1])
		{
			DoFilterGeneric<FloatType, order>(inoutsmp, z, a, b, b1);
		}
		template<typename FloatType, size_t order>
		static void DoFilterUnsafe(
			Sample& inoutsmp,
			FloatType* z,
			const FloatType* a,
			const FloatType* b,
			FloatType* b1)
		{
			DoFilterGeneric<FloatType, order>(inoutsmp, z, a, b, b1);
		}
	};

	template<> struct Topo<ETopo::TDF2>
	{
		template<typename FloatType, size_t order, typename z_t, typename a_t, typename b_t, typename b1_t>
		static void DoFilterGeneric(Sample& inoutsmp, z_t& z, a_t& a, b_t& b, b1_t& b1/*, FloatType* const zo*/)
		{
			const FloatType smpin = inoutsmp.AsFloat32();
			const FloatType smpout = smpin * b[0] + z[0];
			for (uint_fast8_t j = 0, end = order - 1; j < end; ++j)
			{
#if ALBUMBOT_DELAY_TDF2
				z[j] = smpin * b1[j + 1] - smpout * a[j] + z[j + 1];
				b1[j + 1] = b[j + 1];
#else
				z[j] = smpin * b[j + 1] - smpout * a[j] + z[j + 1];
#endif
			}
#if ALBUMBOT_DELAY_TDF2
			z[order - 1] = smpin * b1[order] - smpout * a[order - 1];
			b1[order] = b[order];
#else
			z[order - 1] = smpin * b[order] - smpout * a[order - 1];
#endif
			inoutsmp = static_cast<float>(smpout);
		}

		template<typename FloatType, size_t order>
		static void DoFilter(
			Sample& inoutsmp,
			FloatType (&z)[order],
			const FloatType (&a)[order],
			const FloatType (&b)[order + 1],
			FloatType (&b1)[order + 1])
		{
			DoFilterGeneric<FloatType, order>(inoutsmp, z, a, b, b1);
		}
		template<typename FloatType, size_t order>
		static void DoFilterUnsafe(
			Sample& inoutsmp,
			FloatType* z,
			const FloatType* a,
			const FloatType* b,
			FloatType* b1)
		{
			DoFilterGeneric<FloatType, order>(inoutsmp, z, a, b, b1);
		}
	};

	template<uint_fast8_t order, typename FloatType, typename FreqType, typename LaplaceType>
	inline void DoRecalc(
		const FloatType deltaTime,
		const FreqType freq,
		LaplaceType& laplace,
		FloatType (&b)[order + 1],
		FloatType (&a)[order])
	{
		// Fast bilinear transform
		const FloatType halfW = vHalfTau<FloatType>::value * static_cast<FloatType>(freq);
		//const FloatType ctau = std::tan(vQuarterTau<FloatType>::value - (halfW * deltaTime));
		//const FloatType ctau = static_cast<FloatType>(4.0) / static_cast<FloatType>(order) * FastCot<FloatType>(halfW * deltaTime);
		//const FloatType ctau = FastCot<FloatType>(halfW * deltaTime);
		// FastCot doesn't appear significantly faster than std::tan
		const FloatType ctau = std::tan(vQuarterTau<FloatType>::value - (halfW * deltaTime));
		const FloatType alpha = ctau;
		const FloatType beta = -ctau;
		//const FloatType delta = static_cast<FloatType>(1.0);
		//const FloatType gamma = static_cast<FloatType>(1.0);
		FloatType tab[order + 1][order + 1];
		laplace.Update();
		for (uint_fast8_t n = 0; n <= order; ++n)
		{
			const uint_fast8_t rowidx = order - n;
			const uint_fast8_t colidx = n;
			tab[rowidx][colidx] = laplace[n];
		}
		for (uint_fast8_t i = 0; i < order; ++i)
		{
			for (uint_fast8_t j = 0, end = order - i; j < end; ++j)
			{
				const uint_fast8_t targetRow = order - 1 - i - j;
				const uint_fast8_t targetCol = j;
				const uint_fast8_t aRow = targetRow + 1;
				const uint_fast8_t aCol = targetCol;
				const uint_fast8_t bRow = targetRow;
				const uint_fast8_t bCol = targetCol + 1;
				const FloatType denom = static_cast<FloatType>(order - targetRow - targetCol);
				tab[targetRow][targetCol] = (/* delta * */ aRow * tab[aRow][aCol] + alpha * bCol * tab[bRow][bCol]) / denom;
			}
		}
		FloatType denomPoly[order + 1];
		for (uint_fast8_t i = 0; i <= order; ++i)
			denomPoly[i] = static_cast<FloatType>(0);
		FloatType betaPower[order + 1];
		betaPower[0] = static_cast<FloatType>(1);
		for (uint_fast8_t j = 1; j <= order; ++j)
			betaPower[j] = betaPower[j - 1] * beta;
		for (uint_fast8_t k = 0; k <= order; ++k)
		{
			for (uint_fast8_t j = 0; j <= order - k; ++j)
			{
				const uint_fast8_t i = order - k - j;
				denomPoly[k] += tab[i][j] * /*(gammaPower[i]) * */ betaPower[j];
			}
		}
		const FloatType norm = static_cast<FloatType>(1) / denomPoly[order];
		b[0] = norm;
		for (uint_fast8_t n = 1; n <= order; ++n)
			b[n] = norm * Math::Binomial_t<FloatType, order>::data[n];
		for (uint_fast8_t n = 0, orderm1 = order - 1; n < order; ++n)
			a[n] = norm * denomPoly[orderm1 - n];
	}

	template<typename FloatType, typename FreqType>
	struct IFilterState
	{
		virtual ~IFilterState() noexcept {}
		virtual void Recalc(const FloatType deltaTime, const FreqType freq) = 0;
		//virtual uint_fast8_t GetOrder() const = 0;
		//virtual uint_fast8_t GetNumChannels() const = 0;
		//virtual const FloatType* GetA() const = 0;
		//virtual size_t GetALen() const = 0;
		//virtual const FloatType* GetB() const = 0;
		//virtual size_t GetBLen() const = 0;
		//virtual FloatType* const* GetZ() = 0;
		//virtual size_t GetZLen() const = 0;
		//virtual size_t GetZLenLen() const = 0;
		virtual void DoFilter(const uint_fast8_t ch, Sample& smp, const ETopo eTopo) = 0;
	};

	template<typename FloatType, typename FreqType, typename LaplaceType,
		uint_fast8_t order, uint_fast8_t numch>
	struct FilterState : IFilterState<FloatType, FreqType>
	{
		FilterState()
		{
			for (uint_fast8_t ch = 0; ch < numch; ++ch)
				for (uint_fast8_t i = 0; i < order + 1; ++i)
					b1[ch][i] = 0.0f;
		}
		FilterState(const FilterState&) = default;
		FilterState& operator=(const FilterState&) = default;
		~FilterState() noexcept = default;
		virtual void Recalc(const FloatType deltaTime, const FreqType freq) override
		{ DoRecalc(deltaTime, freq, laplace, b, a); }
		//virtual uint_fast8_t GetOrder() const override { return order; }
		//virtual uint_fast8_t GetNumChannels() const override { return numch; }
		//virtual const FloatType* GetA() const override { return a; }
		//virtual size_t GetALen() const override { return order; }
		//virtual const FloatType* GetB() const override { return b; }
		//virtual size_t GetBLen() const override { return order + 1; }
		//virtual FloatType* const* GetZ() override { return z; }
		//virtual size_t GetZLen() const override { return numch; }
		//virtual size_t GetZLenLen() const override { return order; }
		virtual void DoFilter(const uint_fast8_t ch, Sample& smp, const ETopo eTopo) override
		{
			switch (eTopo)
			{
			case ETopo::DF2: Topo<ETopo::DF2>::DoFilter<FloatType, order>(smp, z[ch], a, b, b1[ch]); break;
			default:
			case ETopo::TDF2: Topo<ETopo::TDF2>::DoFilter<FloatType, order>(smp, z[ch], a, b, b1[ch]); break;
			}
		}
		LaplaceType laplace;
		FloatType a[order];
		FloatType b[order + 1];
		FloatType b1[numch][order + 1];
		FloatType z[numch][order];
	};

	template<bool bOwner>
	class FilterEvent : public IEvent
	{
	public:
		template<typename... RampArgTypes>
		FilterEvent(const EFilterParam paramInit, RampArgTypes&&... rampArgs)
			: param(paramInit), ramp(std::forward<RampArgTypes>(rampArgs)...)
		{
		}

		virtual void Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const override;

	private:
		const EFilterParam param;
		const Ramp ramp;
	};

	template<bool bOwner = false>
	class FilterBase : public AudioSum<bOwner>, public ControlObject<FilterEvent<bOwner>>
	{
	protected:
		explicit FilterBase(const float freq_init, const float res_init, const float gainDB_init)
			: freq(freq_init)
			, res(res_init)
			, gainDB(gainDB_init)
		{
		}

	public:
		float GetFrequency() const noexcept
		{
			return freq;
		}

		float GetResonance() const noexcept
		{
			return res;
		}

		float GetGainDB() const noexcept
		{
			return gainDB;
		}

		float GetGainFactor() const noexcept
		{
			return std::pow(10.0f, gainDB / 20.0f);
		}

		float GetGainFactorSqrt() const noexcept
		{
			return std::pow(10.0f, gainDB / 40.0f);
		}

		float GetGainFactorSqrtSqrt() const noexcept
		{
			return std::pow(10.0f, gainDB / 80.0f);
		}

		void SetFrequency(const Ramp& ramp)
		{
			freqRamp = ramp;
		}

		void SetResonance(const Ramp& ramp)
		{
			resRamp = ramp;
		}

		void SetGainDB(const Ramp& ramp)
		{
			gainDBRamp = ramp;
		}

	protected:
		bool IncrementRamps(const double deltaTime)
		{
			const bool freqIncred = freqRamp.Increment(freq, deltaTime);
			const bool resIncred = resRamp.Increment(res, deltaTime);
			const bool gainDBIncred = gainDBRamp.Increment(gainDB, deltaTime);
			return freqIncred || resIncred || gainDBIncred;
		}

	private:
		float freq;
		float res;
		float gainDB;
		Ramp freqRamp;
		Ramp resRamp;
		Ramp gainDBRamp;
	};

	template<uint_fast8_t order, bool bOwner = false, uint_fast8_t numch = 2, typename FloatType = double, ETopo eTopo = ETopo::TDF2> 
	class Filter : public FilterBase<bOwner>
	{
		static_assert(order < 255, "255-order filter causes integer overflow and is an absurdly high order anyway");

	public:
		explicit Filter(const float freq_init = 1000.0f
			, const float res_init = 1.0f
			, const float gainDB_init = 0.0f
			, const uint_fast16_t controlUpdate_init = 1)
			: FilterBase<bOwner>(freq_init, res_init, gainDB_init)
			, controlUpdateInterval(controlUpdate_init)
			, controlUpdateCounter(1)
			, lastSampleRate(0)
		{
			for (uint_fast8_t ch = 0; ch < numch; ++ch)
			{
				for (uint_fast8_t n = 0; n < order; ++n)
				{
					z[ch][n] = 0.0f;
					b1[ch][n] = 0.0f;
				}
				b1[ch][order] = 0.0f;
			}
		}

	private:
		virtual void recalc(
			const FloatType deltaTime,
			FloatType (&b)[order + 1],
			FloatType (&a)[order]) = 0;

	public:
		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t numSamples,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			if (numChannels != numch)
			{
				//throw std::logic_error("Tried to call GetSamples with " + std::to_string(numChannels) +
				//	" channels on a " + std::to_string(numch) + "-channel filter");
				this->IncrementSampleNum(numSamples);
				return;
			}

			const double deltaTime = 1.0 / static_cast<double>(sampleRate);
			if (sampleRate != lastSampleRate)
			{
				lastSampleRate = sampleRate;
				recalc(deltaTime, b, a);
			}

			if (this->GetInputSamples(bufs, numChannels, numSamples, sampleRate) != AudioSum<bOwner>::EGetInputSamplesResult::SamplesWritten)
			{
				this->IncrementSampleNum(numSamples);
				return;
			}

			this->ProcessEvents(numSamples, [this, bufs, deltaTime](const size_t i)
				{
					if (controlUpdateCounter >= controlUpdateInterval)
					{
						if (this->IncrementRamps(deltaTime * controlUpdateInterval))
						{
							recalc(deltaTime, b, a);
						}
						controlUpdateCounter = 1;
					}
					else
					{
						++controlUpdateCounter;
					}

					for (uint_fast8_t ch = 0; ch < numch; ++ch)
					{
						Topo<eTopo>::template DoFilter<FloatType, order>(bufs[ch][i], z[ch], a, b, b1[ch]);
					}
				});
		}

		virtual size_t GetNumChannels() const noexcept override { return numch; }

	private:
		uint_fast16_t controlUpdateInterval;
		uint_fast16_t controlUpdateCounter;
		unsigned long lastSampleRate;
		FloatType b[order + 1];
		FloatType b1[numch][order + 1];
		FloatType a[order];
		FloatType z[numch][order];
	};

	template<bool bOwner>
	inline void FilterEvent<bOwner>::Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const
	{
		FilterBase<bOwner>& filter = *ctrl.GetPtr<FilterBase<bOwner>>();
		switch (param)
		{
		default:
		case EFilterParam::None: break;
		case EFilterParam::Frequency:
			filter.SetFrequency(ramp);
			break;
		case EFilterParam::Resonance:
			filter.SetResonance(ramp);
			break;
		case EFilterParam::Gain:
			filter.SetGainDB(ramp);
			break;
		}
	}

	template<uint_fast8_t order, typename LaplaceType, bool bOwner = false, uint_fast8_t numch = 2, typename FloatType = double, ETopo eTopo = ETopo::TDF2>
	class LaplaceFilter : public Filter<order, bOwner, numch, FloatType, eTopo>
	{
		using Super = Filter<order, bOwner, numch, FloatType, eTopo>;

	public:
		explicit LaplaceFilter(const float freq_init = 1000.0f
			, const float res_init = 1.0f
			, const float gainDB_init = 0.0f
			, const uint_fast16_t controlUpdate_init = 1)
			: Super(freq_init, res_init, gainDB_init, controlUpdate_init)
			, laplace(*this)
		{
		}

	private:
		virtual void recalc(
			const FloatType deltaTime,
			FloatType (&b)[order + 1],
			FloatType (&a)[order]) override
		{
			DoRecalc<order>(deltaTime, this->GetFrequency(), laplace, b, a);
		}
		/*{
			// Fast bilinear transform
			const FloatType halfW = vHalfTau<FloatType>::value * static_cast<FloatType>(this->GetFrequency());
			//const FloatType ctau = std::tan(vQuarterTau<FloatType>::value - (halfW * deltaTime));
			//const FloatType ctau = static_cast<FloatType>(4.0) / static_cast<FloatType>(order) * FastCot<FloatType>(halfW * deltaTime);
			//const FloatType ctau = FastCot<FloatType>(halfW * deltaTime);
			// FastCot doesn't appear significantly faster than std::tan
			const FloatType ctau = std::tan(vQuarterTau<FloatType>::value - (halfW * deltaTime));
			const FloatType alpha = ctau;
			const FloatType beta = -ctau;
			//const FloatType delta = static_cast<FloatType>(1.0);
			//const FloatType gamma = static_cast<FloatType>(1.0);
			FloatType tab[order + 1][order + 1];
			laplace.Update();
			for (uint_fast8_t n = 0; n <= order; ++n)
			{
				const uint_fast8_t rowidx = order - n;
				const uint_fast8_t colidx = n;
				tab[rowidx][colidx] = laplace[n];
			}
			for (uint_fast8_t i = 0; i < order; ++i)
			{
				for (uint_fast8_t j = 0, end = order - i; j < end; ++j)
				{
					const uint_fast8_t targetRow = order - 1 - i - j;
					const uint_fast8_t targetCol = j;
					const uint_fast8_t aRow = targetRow + 1;
					const uint_fast8_t aCol = targetCol;
					const uint_fast8_t bRow = targetRow;
					const uint_fast8_t bCol = targetCol + 1;
					const FloatType denom = static_cast<FloatType>(order - targetRow - targetCol);
					tab[targetRow][targetCol] = (// delta *
						aRow * tab[aRow][aCol] + alpha * bCol * tab[bRow][bCol]) / denom;
				}
			}
			FloatType denomPoly[order + 1];
			for (uint_fast8_t i = 0; i <= order; ++i)
			{
				denomPoly[i] = static_cast<FloatType>(0);
			}
			FloatType betaPower[order + 1];
			betaPower[0] = static_cast<FloatType>(1);
			for (uint_fast8_t j = 1; j <= order; ++j)
			{
				betaPower[j] = betaPower[j - 1] * beta;
			}
			for (uint_fast8_t k = 0; k <= order; ++k)
			{
				for (uint_fast8_t j = 0; j <= order - k; ++j)
				{
					const uint_fast8_t i = order - k - j;
					denomPoly[k] += tab[i][j] * //(gammaPower[i]) *
						betaPower[j];
				}
			}
			const FloatType norm = static_cast<FloatType>(1) / denomPoly[order];
			b[0] = norm;
			for (uint_fast8_t n = 1; n <= order; ++n)
			{
				b[n] = norm * Math::Binomial_t<FloatType, order>::data[n];
			}
			for (uint_fast8_t n = 0, orderm1 = order - 1; n < order; ++n)
			{
				a[n] = norm * denomPoly[orderm1 - n];
			}
		}*/

	private:
		LaplaceType laplace;
	};

	template<typename LaplaceType>
	class const_laplace_iterator
	{
	public:
		using type = const_laplace_iterator<LaplaceType>;
		explicit const_laplace_iterator(const LaplaceType& laplaceInit, const uint_fast8_t idxInit = 0)
			: laplace(laplaceInit), idx(idxInit)
		{
		}
		typename LaplaceType::return_type operator*() const { return laplace[idx]; }
		type& operator++() { if (idx <= LaplaceType::order) { ++idx; } return *this; }
		type operator++(int) { type it(laplace, idx); ++*this; return it; }
		friend bool operator==(
			const const_laplace_iterator<LaplaceType>& lhs,
			const const_laplace_iterator<LaplaceType>& rhs)
		{ return &lhs.laplace == &rhs.laplace && lhs.idx == rhs.idx; }
		friend bool operator!=(
			const const_laplace_iterator<LaplaceType>& lhs,
			const const_laplace_iterator<LaplaceType>& rhs)
		{ return !(lhs == rhs); }
		const LaplaceType& laplace;
		uint_fast8_t idx;
	};

	template<typename T, uint_fast8_t order_n, bool bOwner> struct LadderLPLaplace
	{
		static constexpr const uint_fast8_t order = order_n;
		using type = LadderLPLaplace<T, order, bOwner>;
		using return_type = T;
		using const_iterator = const_laplace_iterator<type>;
		LadderLPLaplace(const FilterBase<bOwner>& filterInit) : filter(filterInit)
		{
			for (uint_fast8_t i = 0; i <= order; ++i)
			{
				offset[i] = static_cast<T>(0);
			}
		}
		T GetRaw(const uint_fast8_t i)
		{
			return Math::Binomial_t<T, order>::data[i];
		}
		void Update()
		{
			offset[0] = static_cast<T>(filter.GetResonance());
		}
		T operator[](const uint_fast8_t i)
		{
			return GetRaw(i) + offset[i];
		}
		const_iterator begin() const { return const_iterator(*this); }
		const_iterator end() const { return const_iterator(*this, order + 1); }
		const_iterator cbegin() const { return const_iterator(*this); }
		const_iterator cend() const { return const_iterator(*this, order + 1); }
		const FilterBase<bOwner>& filter;
		T offset[order + 1];
	};

	template<typename T, uint_fast8_t order_n>
	struct BesselLPLaplace
	{
		static constexpr const uint_fast8_t order = order_n;
		using type = BesselLPLaplace<T, order>;
		using return_type = T;
		using const_iterator = const_laplace_iterator<type>;
		BesselLPLaplace() = default;
		BesselLPLaplace(const BesselLPLaplace&) = default;
		BesselLPLaplace& operator=(const BesselLPLaplace&) = default;
		~BesselLPLaplace() noexcept = default;
		template<bool bOwner> BesselLPLaplace(const FilterBase<bOwner>&) {}
		void Update() noexcept {}
		T operator[](const uint_fast8_t i) noexcept
		{
			return Math::BesselPolyReverse_t<T, order>::data[i];
		}
		const_iterator begin() const { return const_iterator(*this); }
		const_iterator end() const { return const_iterator(*this, order + 1); }
		const_iterator cbegin() const { return const_iterator(*this); }
		const_iterator cend() const { return const_iterator(*this, order + 1); }
	};

	template<bool bOwner = false, uint_fast8_t numch = 2, ETopo eTopo = ETopo::TDF2, uint_fast8_t order = 4, typename LaplaceFloatType = double, typename FilterFloatType = double>
	class LadderLP_Custom : public LaplaceFilter<order, LadderLPLaplace<LaplaceFloatType, order, bOwner>, bOwner, numch, FilterFloatType, eTopo>
	{
		using Super = LaplaceFilter<order, LadderLPLaplace<LaplaceFloatType, order, bOwner>, bOwner, numch, FilterFloatType, eTopo>;

	public:
		explicit LadderLP_Custom(const float freq_init = 1000.0f
			, const float res_init = 1.0f
			, const float gainDB_init = 0.0f
			, const uint_fast16_t controlUpdate_init = 1)
			: Super(freq_init, res_init, gainDB_init, controlUpdate_init)
		{
		}

	};
	template<bool bOwner = false, uint_fast8_t numch = 2, ETopo eTopo = ETopo::TDF2>
	using LadderLP = LadderLP_Custom<bOwner, numch, eTopo>;

	template<uint_fast8_t order, bool bOwner = false, uint_fast8_t numch = 2, ETopo eTopo = ETopo::TDF2, typename LaplaceFloatType = double, typename FilterFloatType = double>
	class BesselLP_Custom : public LaplaceFilter<order, BesselLPLaplace<LaplaceFloatType, order>, bOwner, numch, FilterFloatType, eTopo>
	{
		using Super = LaplaceFilter<order, BesselLPLaplace<LaplaceFloatType, order>, bOwner, numch, FilterFloatType, eTopo>;

	public:
		explicit BesselLP_Custom(const float freq_init = 1000.0f
			, const float res_init = 1.0f
			, const float gainDB_init = 0.0f
			, const uint_fast16_t controlUpdate_init = 1)
			: Super(freq_init, res_init, gainDB_init, controlUpdate_init)
		{
		}

	};
	template<uint_fast8_t order, bool bOwner = false, uint_fast8_t numch = 2, ETopo eTopo = ETopo::TDF2>
	using BesselLP = BesselLP_Custom<order, bOwner, numch, eTopo>;

#define ALBUMBOT_DEFINE_BIQUAD_CLASS(TypeSuffix) \
	template<bool bOwner = false, uint_fast8_t numch = 2, ETopo eTopo = ETopo::TDF2, typename FloatType = double> \
	class Biquad##TypeSuffix##_Custom : public Filter<2, bOwner, numch, FloatType, eTopo> \
	{ \
		using Super = Filter<2, bOwner, numch, FloatType, eTopo>; \
		static constexpr const uint_fast8_t order = 2; \
	public: \
		explicit Biquad##TypeSuffix##_Custom(const float freq_init = 1000.0f \
			, const float res_init = 1.0f \
			, const float gainDB_init = 0.0f \
			, const uint_fast16_t controlUpdate_init = 1) \
			: Super(freq_init, res_init, gainDB_init, controlUpdate_init) \
		{ \
		} \
	private: \
		virtual void recalc( \
			const FloatType deltaTime, \
			FloatType (&b)[order + 1], \
			FloatType (&a)[order]) override; \
	}

#define ALBUMBOT_DEFINE_BIQUAD_FUNC(TypeSuffix) \
	template<bool bOwner, uint_fast8_t numch, ETopo eTopo, typename FloatType> \
	void Biquad##TypeSuffix##_Custom<bOwner, numch, eTopo, FloatType>::recalc( \
		const FloatType deltaTime, \
		FloatType (&b)[order + 1], \
		FloatType (&a)[order])

#define ALBUMBOT_DEFINE_BIQUAD_CUSTOM(TypeSuffix, al, n0, n1, a0, a1, a2, b0, b1, b2, ...) \
	ALBUMBOT_DEFINE_BIQUAD_CLASS(TypeSuffix); \
	ALBUMBOT_DEFINE_BIQUAD_FUNC(TypeSuffix) \
	{ \
		static constexpr const FloatType one = static_cast<FloatType>(1); \
		static constexpr const FloatType half = static_cast<FloatType>(0.5); \
		static constexpr const FloatType two = one / half; \
		const FloatType w = vTau<FloatType>::value * this->GetFrequency() * deltaTime; \
		const FloatType q = this->GetResonance(); \
		__VA_ARGS__; \
		const FloatType alpha = al; \
		const FloatType cosw = sin(w + vQuarterTau<FloatType>::value); \
		const FloatType a0orig = a0; \
		const FloatType a0recip = one / a0orig; \
		const FloatType coswa0recip = cosw * a0recip; \
		const FloatType num0 = n0; \
		const FloatType num1 = n1; \
		a[0] = a1; \
		a[1] = a2; \
		b[0] = b0; \
		b[1] = b1; \
		b[2] = b2; \
	} \
	template<bool bOwner = false, uint_fast8_t numch = 2, ETopo eTopo = ETopo::TDF2> \
	using Biquad##TypeSuffix = Biquad##TypeSuffix##_Custom<bOwner, numch, eTopo>;

#define ALBUMBOT_DEFINE_BIQUAD(TypeSuffix, al, n0, n1, a0, a1, a2, b0, b1, b2) \
	ALBUMBOT_DEFINE_BIQUAD_CUSTOM(TypeSuffix, al, n0, n1, a0, a1, a2, b0, b1, b2, (void)0)

#define ALBUMBOT_DEFINE_BIQUAD_GAINSQRT(TypeSuffix, al, n0, n1, a0, a1, a2, b0, b1, b2) \
	ALBUMBOT_DEFINE_BIQUAD_CUSTOM(TypeSuffix, al, n0, n1, a0, a1, a2, b0, b1, b2, const FloatType A = this->GetGainFactorSqrt())

#define ALBUMBOT_DEFINE_BIQUAD_GAINSQRTSQRT(TypeSuffix, al, n0, n1, a0, a1, a2, b0, b1, b2) \
	ALBUMBOT_DEFINE_BIQUAD_CUSTOM(TypeSuffix, al, n0, n1, a0, a1, a2, b0, b1, b2, const FloatType Asqrt = this->GetGainFactorSqrtSqrt(); const FloatType A = Asqrt * Asqrt)

	/** These are adapted from
	 *  https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
	 */
	ALBUMBOT_DEFINE_BIQUAD(LP,
		sin(w)/(two*q),
		a0recip - coswa0recip, half * num0,
		one + alpha, -two * coswa0recip, (one - alpha) * a0recip,
		num1, num0, num1);

	ALBUMBOT_DEFINE_BIQUAD(HP,
		sin(w)/(two*q),
		a0recip + coswa0recip, half * num0,
		one + alpha, -two * coswa0recip, (one - alpha) * a0recip,
		num1, -num0, num1);

	ALBUMBOT_DEFINE_BIQUAD(AP,
		sin(w)/(two*q),
		(one - alpha) * a0recip, -two * coswa0recip,
		one + alpha, num1, num0,
		num0, num1, one);

	ALBUMBOT_DEFINE_BIQUAD(Notch,
		sin(w)/(two*q),
		(one - alpha) * a0recip, -two * coswa0recip,
		one + alpha, num1, num0,
		a0recip, num1, a0recip);

	ALBUMBOT_DEFINE_BIQUAD(Peak,
		sin(w)/(two*q),
		alpha * alpha / (a0orig - one), -two * coswa0recip,
		one + alpha / this->GetGainFactorSqrt(), num1, a0recip + a0recip - one,
		(one + num0) * a0recip, num1, (one - num0) * a0recip);

	ALBUMBOT_DEFINE_BIQUAD_GAINSQRTSQRT(LoShelf,
		sin(w)/two*sqrt((A+one/A)*(one/q-one)+two),
		a0orig - two * Asqrt * alpha, a0orig - num0,
		A+one+(A-one)*cosw+two*Asqrt*alpha, -two*((A-one)*a0recip+(A+one)*coswa0recip), (num0-num1)*a0recip,
		A*(two*(A+one)-num0+num1)*a0recip, two*A*((A-one)*a0recip-(A+one)*coswa0recip), A*two*(A+one)*a0recip-A);

	ALBUMBOT_DEFINE_BIQUAD_GAINSQRTSQRT(HiShelf,
		sin(w)/two*sqrt((A+one/A)*(one/q-one)+two),
		a0orig - two * Asqrt * alpha, a0orig - num0,
		A+one-(A-one)*cosw+two*Asqrt*alpha, two*((A-one)*a0recip-(A+one)*coswa0recip), (num0-num1)*a0recip,
		A*(two*(A+one)-num0+num1)*a0recip, -two*A*((A-one)*a0recip+(A+one)*coswa0recip), A*two*(A+one)*a0recip-A);

}

