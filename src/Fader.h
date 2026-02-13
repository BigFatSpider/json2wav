// Copyright Dan Price 2026.

#pragma once

#include "IAudioObject.h"
#include "Ramp.h"
#include <cmath>

namespace json2wav
{
	template<bool bOwner>
	class FaderEvent : public IEvent
	{
	public:
		template<typename... RampArgTypes>
		FaderEvent(RampArgTypes&&... rampArgs)
			: ramp(std::forward<RampArgTypes>(rampArgs)...)
		{
		}

		virtual void Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const override;

	private:
		const Ramp ramp;
	};

	template<bool bOwner = false>
	class Fader : public AudioSum<bOwner>, public ControlObject<FaderEvent<bOwner>>
	{
	public:
		Fader(const float gainDBInit = 0.0f) : lastNumChannels(2), gainDB(gainDBInit) {}

		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t numSamples,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			lastNumChannels = numChannels;

			const double deltaTime = 1.0 / static_cast<double>(sampleRate);

			if (this->GetInputSamples(bufs, numChannels, numSamples, sampleRate)
				!= AudioSum<bOwner>::EGetInputSamplesResult::SamplesWritten)
			{
				this->IncrementSampleNum(numSamples);
				return;
			}

			this->ProcessEvents(numSamples, [this, bufs, numChannels, deltaTime](const size_t i)
				{
					gainDBRamp.Increment(gainDB, deltaTime);
					const float gainfactor = this->GetGainFactor();
					for (uint_fast8_t ch = 0; ch < numChannels; ++ch)
						bufs[ch][i] *= gainfactor;
				});
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return lastNumChannels;
		}

		void SetGainDB(const float db)
		{
			gainDB = db;
		}

		void SetGainDB(const Ramp& ramp)
		{
			gainDBRamp = ramp;
		}

		float GetGainFactor() const noexcept
		{
			constexpr const float over20 = 1.0f / 20.0f;
			return std::pow(10.0f, gainDB * over20);
		}

	private:
		size_t lastNumChannels;
		float gainDB;
		Ramp gainDBRamp;
	};

	template<bool bOwner>
	inline void FaderEvent<bOwner>::Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const
	{
		Fader<bOwner>& fader = *ctrl.GetPtr<Fader<bOwner>>();
		fader.SetGainDB(ramp);
	}
}

