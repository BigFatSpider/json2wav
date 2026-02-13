// Copyright Dan Price. All rights reserved.

#pragma once

#include "IAudioObject.h"
#include "Sample.h"
#include "Filter.h"
#include "Utility.h"
#include "Memory.h"
#include <limits>
#include <functional>

namespace json2wav
{
	enum class EFeedbackType
	{
		Gain, DB, DBNeg
	};

	inline float CalcFeedback(const float value, const EFeedbackType efbt) noexcept
	{
		switch (efbt)
		{
		default:
		case EFeedbackType::Gain: return value;
		case EFeedbackType::DB: return Utility::DBToGain(value);
		case EFeedbackType::DBNeg: return -Utility::DBToGain(value);
		}
	}

	template<bool bOwner = false>
	class Delay : public AudioSum<bOwner>
	{
	public:
		Delay(const float timeInit, const float feedbackInit = 0.0f, const EFeedbackType efbt = EFeedbackType::Gain
			, const size_t sampleRateInit = 0)
			: time(timeInit), feedback(CalcFeedback(feedbackInit, efbt)), timeSamples(0)
			, filtnumch(std::numeric_limits<size_t>::max())
			, lastNumChannels(0), lastSampleRate(sampleRateInit)
			, queueLength(256), bQueueInitialized(false)
			, filter([](const uint_fast8_t ch, const float val) -> Sample { Sample smp(val); return smp; })
		{
		}

		void SetTime(const float t) noexcept
		{
			time = t;
			if (lastSampleRate > 0)
				timeSamples = static_cast<size_t>(time * static_cast<float>(lastSampleRate));
		}

		float GetTime() const noexcept
		{
			return time;
		}

		void SetFeedback(const float f, const EFeedbackType efbt = EFeedbackType::Gain) noexcept
		{
			feedback = CalcFeedback(f, efbt);
		}

		void SetFeedbackGain(const float f) noexcept
		{
			feedback = f;
		}

		void SetFeedbackDB(const float f, const bool bSignBit = false) noexcept
		{
			static constexpr const int arrsign[2] = { 1, -1 };
			const int sign(arrsign[bSignBit]);
			feedback = sign*Utility::DBToGain(sign*f);
		}

		float GetFeedback() const noexcept
		{
			return feedback;
		}

		float GetFeedbackDB() const noexcept
		{
			static constexpr const int arrsign[2] = { 1, -1 };
			const int sign(arrsign[feedback < 0.0f]);
			return (feedback == 0.0f) ? -std::numeric_limits<float>::infinity() : Utility::GainToDB(sign*feedback);
		}

	private:
		template<uint_fast8_t order, uint_fast8_t numch>
		using BesselFilterState = Filter::FilterState<float, float, Filter::BesselLPLaplace<float, order>, order, numch>;

		template<uint_fast8_t order>
		UniquePtr<Filter::IFilterState<float, float>> CreateBesselFilterState(const uint_fast8_t numch)
		{
			UniquePtr<Filter::IFilterState<float, float>> ptr;
			switch (numch)
			{
			case 1: ptr = MakeUnique<BesselFilterState<order, 1>>(); break;
			case 2: ptr = MakeUnique<BesselFilterState<order, 2>>(); break;
			default: break;
			}
			return ptr;
		}

	public:
		void SetBesselFilter(
			const float deltaTime,
			const float freq,
			const uint_fast8_t order,
			const uint_fast8_t numch,
			const Filter::ETopo eTopo = Filter::ETopo::TDF2)
		{
			filtnumch = numch;
			auto CreateFilterState = [this, order, numch]()
			{
				switch (order)
				{
				case 0:
				case 1: return CreateBesselFilterState<1>(numch);
				case 2: return CreateBesselFilterState<2>(numch);
				case 3: return CreateBesselFilterState<3>(numch);
				case 4: return CreateBesselFilterState<4>(numch);
				case 5: return CreateBesselFilterState<5>(numch);
				case 6: return CreateBesselFilterState<6>(numch);
				case 7: return CreateBesselFilterState<7>(numch);
				default:
				case 8: return CreateBesselFilterState<8>(numch);
				}
			};
			filterState = CreateFilterState();
			filterState->Recalc(deltaTime, freq);
			filter = [this, eTopo](const uint_fast8_t ch, const float val) -> Sample
			{
				Sample smp(val);
				filterState->DoFilter(ch, smp, eTopo);
				return smp;
			};
		}

		void UnsetFilter()
		{
			filter = [](const uint_fast8_t ch, const float val) { Sample smp(val); return smp; };
			filterState = nullptr;
			filtnumch = std::numeric_limits<size_t>::max();
		}

	public:
		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			static const float fbthresh = Utility::DBToGain(-96.3f);
			static constexpr const int arrsign[2] = { 1, -1 };
			if (numChannels > filtnumch)
				return;

			lastNumChannels = numChannels;
			lastSampleRate = sampleRate;
			InitializeQueue(numChannels, sampleRate);
			Vector<Sample*> bufOffsets(numChannels, nullptr);
			size_t fillSize = timeSamples;
			size_t movenum = 0;
			const bool bFeedback = arrsign[feedback < 0.0f] * feedback >= fbthresh;

			if (timeSamples < bufSize)
			{
				for (size_t ch = 0; ch < numChannels; ++ch)
				{
					for (size_t i = 0; i < timeSamples; ++i)
						bufs[ch][i] = filter(ch, queue[ch][i]);
					bufOffsets[ch] = bufs[ch] + timeSamples;
				}
				this->GetInputSamples(bufOffsets.data(), numChannels, bufSize - fillSize, sampleRate);
				if (bFeedback)
					for (size_t ch = 0; ch < numChannels; ++ch)
						for (size_t i = timeSamples; i < bufSize; ++i)
							bufs[ch][i] = filter(ch, bufs[ch][i] + feedback * bufs[ch][i - timeSamples]);
				else
					for (size_t ch = 0; ch < numChannels; ++ch)
						for (size_t i = timeSamples; i < bufSize; ++i)
							bufs[ch][i] = filter(ch, bufs[ch][i]);
				for (size_t ch = 0; ch < numChannels; ++ch)
					bufOffsets[ch] = queue[ch];
			}
			else
			{
				movenum = timeSamples - bufSize;
				for (size_t ch = 0; ch < numChannels; ++ch)
				{
					for (size_t i = 0; i < bufSize; ++i)
						bufs[ch][i] = filter(ch, queue[ch][i]);
					for (size_t i = 0; i < movenum; ++i)
						queue[ch][i] = queue[ch][timeSamples - movenum + i];
					bufOffsets[ch] = queue[ch] + movenum;
				}
				fillSize = bufSize;
			}

			this->GetInputSamples(bufOffsets.data(), numChannels, fillSize, sampleRate);
			if (bFeedback)
				for (size_t ch = 0; ch < numChannels; ++ch)
					for (size_t i = 0; i < fillSize; ++i)
						bufOffsets[ch][i] += feedback * bufs[ch][bufSize + movenum - timeSamples + i];
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return lastNumChannels;
		}

	private:
		void InitializeQueue(const size_t numChannels, const size_t sampleRate)
		{
			if (bQueueInitialized)
				return;

			if (!queue.Initialized())
			{
				timeSamples = static_cast<size_t>(time * static_cast<float>(sampleRate));
				size_t newQueueLength = queueLength;
				while (newQueueLength <= timeSamples)
					newQueueLength <<= 1;
				queueLength = newQueueLength;
				queue.Initialize(numChannels, queueLength);
			}
			bQueueInitialized = true;
		}

	private:
		float time;
		float feedback;
		size_t timeSamples;
		size_t filtnumch;
		size_t lastNumChannels;
		size_t lastSampleRate;
		size_t queueLength;
		bool bQueueInitialized;
		SampleBuf queue;
		std::function<Sample(const uint_fast8_t, const float)> filter;
		UniquePtr<Filter::IFilterState<float, float>> filterState;
	};
}

