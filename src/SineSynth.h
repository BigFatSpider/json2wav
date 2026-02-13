// Copyright Dan Price. All rights reserved.

#pragma once

#include "Synth.h"
#include "FastSin.h"

namespace json2wav
{
	template<bool bSine>
	class SinusoidSynth : public BasicSynth
	{
	public:
		SinusoidSynth(const float frequency_init = 1000.0f
			, const float amplitude_init = 0.5f
			, const float phase_init = 0.0f)
			: BasicSynth(frequency_init, amplitude_init, phase_init)
		{
		}

		virtual void GetSamples(Sample* const* const bufs, const size_t numChannels, const size_t numSamples,
			const unsigned long samplerate, IAudioObject* const requester) noexcept override
		{
			if (numChannels == 0)
			{
				return;
			}

			Sample* const buf = bufs[0];
			const double deltatime = 1.0 / static_cast<double>(samplerate);
			GetSynthSamples(bufs, numChannels, numSamples, true, [this, buf, deltatime](const size_t i)
				{
					Increment(deltatime);
					buf[i] = GetAmplitude() * FastSinusoid<bSine>::template call<5>(GetInstantaneousPhase() * vTau<double>::value);
				});
		}
	};

	using CosineSynth = SinusoidSynth<false>;
	using SineSynth = SinusoidSynth<true>;
}

