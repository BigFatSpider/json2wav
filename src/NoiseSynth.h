// Copyright Dan Price. All rights reserved.

#pragma once

#include "Synth.h"
#include "Random.h"
#include "Thread.h"

#define STANFORD_PINK 0

namespace json2wav
{

	class NoiseSynth : public BasicSynth
	{
	public:
		NoiseSynth(const float ampInit = 0.0f)
			: BasicSynth(1000.0f, ampInit), z1(0.0f), z2(0.0f), z3(0.0f)
		{
		}

		virtual void GetSamples(Sample* const* const bufs, const size_t numChannels, const size_t numSamples,
			const unsigned long sampleRate, IAudioObject* const requester) noexcept override
		{
			if (numChannels == 0)
			{
				return;
			}

			const double deltaTime = 1.0 / (double)sampleRate;
			Sample* const buf = bufs[0];
			GetSynthSamples(bufs, numChannels, numSamples, true, [this, buf, deltaTime](const size_t i)
				{

#if STANFORD_PINK
					static constexpr const float amp_norm = 4.0f;

					/** The coefficients for this -3 dB/8ve "pinking" filter were taken from
					 *  https://ccrma.stanford.edu/~jos/sasp/Example_Synthesis_1_F_Noise.html
					 *  Factoring the polynomials yields the following:
					 *  Poles:  0.555945, 0.943842, 0.995169
					 *  Zeroes: 0.107981, 0.832657, 0.982232
					 */
					//static constexpr const float a0 = 1.0f;
					static constexpr const float a1 = -2.494956002f;
					static constexpr const float a2 = 2.017265875f;
					static constexpr const float a3 = -0.522189400f;
					static constexpr const float b0 = 0.049922035f * amp_norm;
					static constexpr const float b1 = -0.095993537f * amp_norm;
					static constexpr const float b2 = 0.050612699f * amp_norm;
					static constexpr const float b3 = -0.004408786f * amp_norm;
#else
					static constexpr const float amp_norm = 6.0f;

					/** The following -3 dB/8ve "pinking" filter has a more pleasing sound due to a bass
					 *  boost of a few dB below 100 Hz and 1 dB cuts (relative to the Stanford "pinking"
					 *  filter) centered around 300Hz and 3500Hz, and it is more efficient due to a zero at
					 *  the complex origin making the b3 term 0:
					 *  Poles:  1-(1/6)^3, 1-(3/6)^3, 1-(5/6)^3
					 *  Zeroes: 1-(2/6)^3, 1-(4/6)^3, 1-(6/6)^3
					 */
					//static constexpr const float a0 = 1.0f;
					static constexpr const float a1 = -2.29166666667f;
					static constexpr const float a2 = 1.65892918381f;
					static constexpr const float a3 = -0.36692761917;
					static constexpr const float b0 = 0.030517578125f * amp_norm;
					static constexpr const float b1 = -0.0508626302083f * amp_norm;
					static constexpr const float b2 = 0.02067995006f * amp_norm;
					//static constexpr const float b3 = 0.0f * amp_norm;
#endif

					//static RNGNorm rng(0.0f, 0.18f);
					//static RNGNorm rng(0.0f, 0.5f);
					//static RNG rng(-1.0f, 1.0f);
					thread_local ThreadSafeStatic<RNG> rng(-1.0f, 1.0f);

					Increment(deltaTime);
					const float smpIn = GetAmplitude() * rng();
					const float mid = smpIn - a1*z1 - a2*z2 - a3*z3;
#if STANFORD_PINK
					buf[i] = b0*mid + b1*z1 + b2*z2 + b3*z3;
#else
					buf[i] = b0*mid + b1*z1 + b2*z2;
#endif
					z3 = z2;
					z2 = z1;
					z1 = mid;
					//buf[i] = smpIn;
				});
		}

	private:
		float z1, z2, z3;
	};
}

