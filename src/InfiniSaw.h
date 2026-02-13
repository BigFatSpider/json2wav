// Copyright Dan Price. All rights reserved.

#pragma once

#include "Synth.h"
#include "CircleQueue.h"
#include "Memory.h"
#include "FastSin.h"
#include "InfiniSaw.gen.h"
#include <utility>
#include <algorithm>
#include <stdexcept>
#include <atomic>
//#include <fstream>
#include <cmath>

#define INFINISAW_ANTIALIAS 1
#define INFINISAW_LOG_ANTIALIAS 0
#define INFINISAW_LOG_BLEPSPIKE 0
#define INFINISAW_LOG_BLEPSPIKE_NEW 0

#if (defined(INFINISAW_LOG_ANTIALIAS) && INFINISAW_LOG_ANTIALIAS) || INFINISAW_LOG_BLEPSPIKE
#include <iostream>
#include <string>
#endif

#if defined(INFINISAW_LOG_ANTIALIAS) && INFINISAW_LOG_ANTIALIAS
#define AA_LOG(msg) if (bAaLog) { std::cout << (msg) << '\n'; char c = '\0'; std::cin.get(c); if (c == 'c' || c == 'C') { bAaLog = false; } }
#else
#define AA_LOG(msg)
#endif

#if INFINISAW_LOG_BLEPSPIKE
#define BS_LOG(cond, msg) if (bBsLog && (cond)) { std::cout << (msg) << '\n'; char c = '\0'; std::cin.get(c); if (c == 'c' || c == 'C') { bBsLog = false; } }
#else
#define BS_LOG(cond, msg)
#endif

#if defined(INFINISAW_LOG_BLEPSPIKE_NEW) && INFINISAW_LOG_BLEPSPIKE_NEW
#include <iostream>
#define SPIKE_SAMPLE 620790
#define NEAR_SPIKE(x) ((x) >= SPIKE_SAMPLE - 10 && (x) < SPIKE_SAMPLE + 11)
#define CHECK_NEAR_SPIKE(x) bLogSpike = NEAR_SPIKE(x)
#define LOG_SPIKE(n, v) LogSpike(n, v)
#define LOG_SPIKE_COND(c, n, v) if (c) LogSpike(n, v)
#else
#define NEAR_SPIKE(x)
#define CHECK_NEAR_SPIKE(x)
#define LOG_SPIKE(n, v)
#define LOG_SPIKE_COND(c, n, v)
#endif

namespace json2wav
{
	inline bool nearly_equal(const double f1, const double f2)
	{
		return ((f1 < f2) ? f2 - f1 : f1 - f2) < 0.0001;
	}

	enum class EInfiniSawParam
	{
		SynthParam, HardSync
	};

	struct InfiniSawEvent : public SynthEvent<InfiniSawEvent>
	{
	private:
		static ESynthParam VerifyInfiniSawParam(const EInfiniSawParam param)
		{
			if (param == EInfiniSawParam::SynthParam)
			{
				throw std::runtime_error("Don't initialize InfiniSawEvents with EInfiniSawParam::SynthParam! Use ESynthParam values instead.");
			}
			return static_cast<ESynthParam>(-1);
		}

	public:
		template<typename... RampArgTypes>
		InfiniSawEvent(const ESynthParam param_init, RampArgTypes&&... rampArgs)
			: SynthEvent<InfiniSawEvent>(param_init, std::forward<RampArgTypes>(rampArgs)...), jumpParam(EInfiniSawParam::SynthParam)
		{
		}

		template<typename... RampArgTypes>
		InfiniSawEvent(const EInfiniSawParam param_init, RampArgTypes&&... rampArgs)
			: SynthEvent<InfiniSawEvent>(VerifyInfiniSawParam(param_init), std::forward<RampArgTypes>(rampArgs)...), jumpParam(param_init)
		{
		}

		virtual void Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const override;

		const EInfiniSawParam jumpParam;
	};

	/** No prefix = mathematically precise waveforms; brightest sound
	 *  M = monotonic blep, which puts cutoff at -6 dB relative to precise math; darkest sound
	 *  R = ripple such that blep cutoff is -3 dB relative to precise math; medium-bright sound
	 *  H = half ripple, no overshoot; slightly brighter than R
	 */
	enum class EInfiniSawPrecision
	{
		Precise,
		Fast,
		ExtraFast,
		MPrecise,
		MFast,
		MExtraFast,
		RPrecise,
		RFast,
		RExtraFast,
		HPrecise,
		HFast,
		HExtraFast,
		Num
	};

	class InfiniSaw : public SynthWithCustomEvent<InfiniSawEvent>
	{

#if defined(INFINISAW_LOG_BLEPSPIKE_NEW) && INFINISAW_LOG_BLEPSPIKE_NEW
		template<typename T>
		void LogSpike(std::string name, const T& value)
		{
			if (bLogSpike)
			{
				std::cout << '"' << name << "\" has value \"" << value << "\"\n";
			}
		}
#endif

	public:
		struct Jump
		{
			double pos;
			float amp;
			Jump(const double pos_init, const float amp_init) noexcept : pos(pos_init), amp(amp_init) {}
		};

		struct SampleMetadata
		{
			double deltaTime;
			double nextWaveformSample;
			double normalizedPhase;
			float amp;
			float freq;
			SampleMetadata(const double dt_init, const double nws_init, const float np_init, const float amp_init, const float freq_init)
				: deltaTime(dt_init), nextWaveformSample(nws_init), normalizedPhase(np_init), amp(amp_init), freq(freq_init)
			{
			}
		};

		struct JumpMetadata
		{
			size_t idx;
			double pos;
			float amp;
			JumpMetadata(const size_t idx_init, const double pos_init, const float amp_init)
				: idx(idx_init), pos(pos_init), amp(amp_init)
			{
			}
		};

	public:
		static void BlepBuf(double* const buf, const size_t numSamples, const Vector<JumpMetadata>& jumps,
			const EInfiniSawPrecision ePrecision = EInfiniSawPrecision::RFast) noexcept
		{
			const auto blep_peek = GetBlepPeek(ePrecision);
			const auto GetBlepRes = GetGetBlepRes(ePrecision);
			const auto GetBlepSize = GetGetBlepSize(ePrecision);
			const size_t blep_size = GetBlepSize();
			for (const JumpMetadata& jump : jumps)
			{
				const size_t jmp_idx = jump.idx;
				const double jmp_smp_pos = jump.pos;
				const double jmp_amp = (double)jump.amp;
				size_t blep_idx = (jmp_idx >= blep_peek) ? 0 : blep_peek - jmp_idx;
				for (size_t buf_idx = jmp_idx + blep_idx - blep_peek;
						blep_idx < blep_size && buf_idx < numSamples;
						++blep_idx, ++buf_idx)
					buf[buf_idx] += jmp_amp * GetBlepRes(blep_idx, jmp_smp_pos);
			}
		}

		static void BlepBuf(Sample* const buf, const size_t numSamples, const Vector<JumpMetadata>& jumps,
			const EInfiniSawPrecision ePrecision = EInfiniSawPrecision::RFast) noexcept
		{
			if (jumps.empty())
				return;
			Vector<double> buf64;
			buf64.reserve(numSamples);
			for (size_t i = 0; i < numSamples; ++i)
				buf64.push_back(static_cast<double>(buf[i].AsFloat32()));
			BlepBuf(buf64.data(), buf64.size(), jumps, ePrecision);
			for (size_t i = 0; i < numSamples; ++i)
				buf[i] = static_cast<float>(buf64[i]);
		}

	public:
		InfiniSaw(const Vector<Jump>& jumps_init
			, const float frequency_init = 1000.0f
			, const float amplitude_init = 0.5f
			, const double phase_init = 0.0
			, const EInfiniSawPrecision ePrecision = EInfiniSawPrecision::RFast)
			: SynthWithCustomEvent(frequency_init, amplitude_init, phase_init)
			, jumps(jumps_init)
			, blep_peek(GetBlepPeek(ePrecision))
			, GetBlepRes(GetGetBlepRes(ePrecision))
			, GetBlepSize(GetGetBlepSize(ePrecision))
#if defined(INFINISAW_LOG_ANTIALIAS) && INFINISAW_LOG_ANTIALIAS
			, bAaLog(true)
#endif
#if INFINISAW_LOG_BLEPSPIKE
			, bBsLog(true)
#endif
			//, fileidx(NextFileIdx())
		{
		}

		InfiniSaw(Vector<Jump>&& jumps_init
			, const float frequency_init = 1000.0f
			, const float amplitude_init = 0.5f
			, const double phase_init = 0.0
			, const EInfiniSawPrecision ePrecision = EInfiniSawPrecision::RFast)
			: SynthWithCustomEvent(frequency_init, amplitude_init, phase_init)
			, jumps(std::move(jumps_init))
			, blep_peek(GetBlepPeek(ePrecision))
			, GetBlepRes(GetGetBlepRes(ePrecision))
			, GetBlepSize(GetGetBlepSize(ePrecision))
#if defined(INFINISAW_LOG_ANTIALIAS) && INFINISAW_LOG_ANTIALIAS
			, bAaLog(true)
#endif
#if INFINISAW_LOG_BLEPSPIKE
			, bBsLog(true)
#endif
			//, fileidx(NextFileIdx())
		{
		}

		InfiniSaw(const float frequency_init = 1000.0f
			, const float amplitude_init = 0.5f
			, const double phase_init = 0.0
			, const EInfiniSawPrecision ePrecision = EInfiniSawPrecision::RFast)
			: SynthWithCustomEvent(frequency_init, amplitude_init, 0.0)
			, jumps{ Jump(phase_init, 1.0f) }
			, blep_peek(GetBlepPeek(ePrecision))
			, GetBlepRes(GetGetBlepRes(ePrecision))
			, GetBlepSize(GetGetBlepSize(ePrecision))
#if defined(INFINISAW_LOG_ANTIALIAS) && INFINISAW_LOG_ANTIALIAS
			, bAaLog(true)
#endif
#if INFINISAW_LOG_BLEPSPIKE
			, bBsLog(true)
#endif
			//, fileidx(NextFileIdx())
		{
		}

		const Vector<Jump>& GetJumps() const noexcept
		{
			return jumps;
		}

		Vector<Jump>& GetJumps() noexcept
		{
			return jumps;
		}

		void HardSync(const size_t sampleNum)
		{
			hardSyncs.push_back(sampleNum);
		}

		void SetFast(const bool bFast)
		{
			typedef size_t (*gbs_t)();
			static const size_t etofSize = static_cast<size_t>(EInfiniSawPrecision::Num);
			static const gbs_t etof[etofSize] = {
				&InfiniSaw::GetBlepSizePrecise,
				&InfiniSaw::GetBlepSizeFast,
				&InfiniSaw::GetBlepSizeXfast,
				&InfiniSaw::GetMBlepSizePrecise,
				&InfiniSaw::GetMBlepSizeFast,
				&InfiniSaw::GetMBlepSizeXfast,
				&InfiniSaw::GetRBlepSizePrecise,
				&InfiniSaw::GetRBlepSizeFast,
				&InfiniSaw::GetRBlepSizeXfast,
				&InfiniSaw::GetHBlepSizePrecise,
				&InfiniSaw::GetHBlepSizeFast,
				&InfiniSaw::GetHBlepSizeXfast
			};

			size_t ePrecision(0);
			while (ePrecision < etofSize && etof[ePrecision] != GetBlepSize) { ++ePrecision; }

			switch (static_cast<EInfiniSawPrecision>(ePrecision))
			{
			case EInfiniSawPrecision::Precise: if (bFast) { SetPrecision(EInfiniSawPrecision::Fast); } break;
			case EInfiniSawPrecision::Fast: if (!bFast) { SetPrecision(EInfiniSawPrecision::Precise); } break;
			case EInfiniSawPrecision::ExtraFast: if (!bFast) { SetPrecision(EInfiniSawPrecision::Precise); } break;
			case EInfiniSawPrecision::MPrecise: if (bFast) { SetPrecision(EInfiniSawPrecision::MFast); } break;
			case EInfiniSawPrecision::MFast: if (!bFast) { SetPrecision(EInfiniSawPrecision::MPrecise); } break;
			case EInfiniSawPrecision::MExtraFast: if (!bFast) { SetPrecision(EInfiniSawPrecision::MPrecise); } break;
			case EInfiniSawPrecision::RPrecise: if (bFast) { SetPrecision(EInfiniSawPrecision::RFast); } break;
			case EInfiniSawPrecision::RFast: if (!bFast) { SetPrecision(EInfiniSawPrecision::RPrecise); } break;
			case EInfiniSawPrecision::RExtraFast: if (!bFast) { SetPrecision(EInfiniSawPrecision::RPrecise); } break;
			case EInfiniSawPrecision::HPrecise: if (bFast) { SetPrecision(EInfiniSawPrecision::HFast); } break;
			case EInfiniSawPrecision::HFast: if (!bFast) { SetPrecision(EInfiniSawPrecision::HPrecise); } break;
			case EInfiniSawPrecision::HExtraFast: if (!bFast) { SetPrecision(EInfiniSawPrecision::HPrecise); } break;
			default:
			case EInfiniSawPrecision::Num:
				SetPrecision((bFast) ? EInfiniSawPrecision::RFast : EInfiniSawPrecision::RPrecise);
				break;
			}
		}

		void SetPrecision(const EInfiniSawPrecision ePrecision)
		{
			blep_peek = GetBlepPeek(ePrecision);
			GetBlepRes = GetGetBlepRes(ePrecision);
			GetBlepSize = GetGetBlepSize(ePrecision);
		}

		virtual void GetSamples(Sample* const* const bufs, const size_t numChannels, const size_t numSamples,
			const unsigned long sampleRate, IAudioObject* const requester) noexcept override
		{
			if (numChannels == 0)
			{
				return;
			}

			Sample* const buf = bufs[0];
			Vector<double> buf64;
			buf64.resize(numSamples, 0.0);
			buf_amp_cache.clear();
			buf_amp_cache.reserve(numSamples);
			const double deltaTime = 1.0 / static_cast<double>(sampleRate);
			Vector<std::pair<size_t, std::pair<double, float>>> sampleStreamJumps;
			GetSynthSamples(bufs, numChannels, numSamples, false, [this, &buf64, /*sampleRate,*/ deltaTime, &sampleStreamJumps](const size_t i)
				{
					CHECK_NEAR_SPIKE(i);

					LOG_SPIKE("Sample number", i);
					const auto foundit = std::find(hardSyncs.begin(), hardSyncs.end(), i);
					double currentPhase;
					double nextPhase;
					float currentFreq;
					float currentAmp;
					buf64[i] += [this, deltaTime](double& outCurrentPhase, float& outCurrentFreq, float& outCurrentAmp) -> double
						{
							double nextSample;
							GetNextWaveformSample(&nextSample, deltaTime, outCurrentPhase, outCurrentAmp, outCurrentFreq);
							buf_amp_cache.push_back(outCurrentAmp);
							LOG_SPIKE("Next sample", nextSample);
							LOG_SPIKE("Current phase", outCurrentPhase);
							LOG_SPIKE("Current frequency", outCurrentFreq);
							LOG_SPIKE("Current amplitude", outCurrentAmp);
							return nextSample;
						}(currentPhase, currentFreq, currentAmp);

					if (foundit != hardSyncs.end())
					{
						LOG_SPIKE("Found-hard-sync", true);
						SetPhase(PreciseRamp(0.0, 1.0f, ERampShape::Instant));
					}

					float amp;
					float freq;
					PeekNextWaveformSample(nullptr, deltaTime, nextPhase, amp, freq);
					GetJumpsInPhaseRange(currentPhase, nextPhase, i, buf64[i], foundit != hardSyncs.end(), sampleStreamJumps);
					//if (GetJumpsInPhaseRange(currentPhase, nextPhase, i, buf[i].AsFloat32(), foundit != hardSyncs.end(), sampleStreamJumps) > 1)
					//{
						//const float maxFreq = sampleRate * 0.5f;
						//buf[i] = currentAmp * AdditiveWaveform(currentPhase, currentFreq, maxFreq);
					//}

					if (foundit != hardSyncs.end())
					{
						hardSyncs.erase(foundit);
					}
				});

#if defined(INFINISAW_ANTIALIAS) && INFINISAW_ANTIALIAS
			// Look ahead
			double nextSample;
			double postEndPhase;
			float amp;
			float freq;
			PeekNextWaveformSample(nullptr, deltaTime, postEndPhase, amp, freq, blep_peek);
			for (size_t look = 0; look < blep_peek - 1; ++look)
			{
				double currentPhase;
				double nextPhase;
				PeekNextWaveformSample(&nextSample, deltaTime, currentPhase, amp, freq, look);
				PeekNextWaveformSample(nullptr, deltaTime, nextPhase, amp, freq, look + 1);
				GetJumpsInPhaseRange(currentPhase, nextPhase, numSamples + look, nextSample, false, sampleStreamJumps);
			}
			double endPhase;
			PeekNextWaveformSample(&nextSample, deltaTime, endPhase, amp, freq, blep_peek - 1);
			GetJumpsInPhaseRange(endPhase, postEndPhase, numSamples + blep_peek - 1, nextSample, false, sampleStreamJumps);

#if 0
			// TODO: Pairwise summation
			Vector<JumpMetadata> aaQueue;
			aaQueue.reserve(antiAliasQueue.size());
			while (!antiAliasQueue.empty())
				aaQueue.push_back(antiAliasQueue.pop());
			Vector<const JumpMetadata*> jumps;
			auto jumpsOntoIdx = [this, &sampleStreamJumps, &jumps](const size_t idx)
			{
				jumps.clear();
				if (idx < blep_peek)
				{
					for (const JumpData& jump : aaQueue)
					{
						if (std::max(jump.idx, idx) - std::min(jump.idx, idx) < blep_peek)
						{
							jumps.push_back(&jump);
						}
					}
				}
			};

			for (size_t buf_idx = 0, stmjmp_idx = 0; buf_idx < numSamples; ++buf_idx)
			{
				{
					buf64[buf_idx] += (double)GetAmpAtBufIdx(buf_idx) * (double)jump.amp * GetBlepRes(blep_idx, jump.pos);
				}
			}
#else
			while (!antiAliasQueue.empty())
			{
				const JumpMetadata& jump = antiAliasQueue.peek();
				AA_LOG("Next jump in anti-alias queue: idx=" + std::to_string(jump.idx) + ", pos=" + std::to_string(jump.pos) + ", amp=" + std::to_string(jump.amp));
				for (size_t blep_idx = jump.idx, buf_idx = 0, blep_size = GetBlepSize(); blep_idx < blep_size; ++blep_idx, ++buf_idx)
				{
					//const float blep_res = GetBlepRes(blep_idx, jump.pos);
					//const float newsample = buf[buf_idx].AsFloat32() + GetAmpAtBufIdx(buf_idx) * jump.amp * blep_res;
					//BS_LOG(((newsample < 0.0f) == (buf[buf_idx].AsFloat32() < 0.0f)) && ((newsample * newsample) > (0.25f * 0.25f)) && ((newsample * newsample) > (4.0f * [](const float x) { return x * x; }(buf[buf_idx].AsFloat32()))), "Spike at sample " + std::to_string(buf_idx) + ". Was " + std::to_string(buf[buf_idx].AsFloat32()) + " is " + std::to_string(newsample));
					//AA_LOG("\tSample at buf idx " + std::to_string(buf_idx) + ": before=" + std::to_string(buf[buf_idx].AsFloat32()) + ", after=" + std::to_string(newsample) + ", blep res=" + std::to_string(blep_res));
					//buf[buf_idx] = newsample;

					// DISABLE TO TEST POPS
					const double blepres = static_cast<double>(GetAmpAtBufIdx(buf_idx)) * static_cast<double>(jump.amp) * GetBlepRes(blep_idx, jump.pos);
					buf64[buf_idx] += blepres;
				}
				antiAliasQueue.pop_idx();
			}

			for (size_t j = 0; j < sampleStreamJumps.size(); ++j)
			{
				const size_t jmp_idx = sampleStreamJumps[j].first;
				const double jmp_smp_pos = sampleStreamJumps[j].second.first;
				const double jmp_amp = (double)sampleStreamJumps[j].second.second;

				CHECK_NEAR_SPIKE(jmp_idx);
				LOG_SPIKE("Jump index", jmp_idx);
				LOG_SPIKE("Jump sample position", jmp_smp_pos);
				LOG_SPIKE("Jump amplitude", jmp_amp);

				AA_LOG("Next jump in sample stream: idx=" + std::to_string(jmp_idx) + ", pos=" + std::to_string(jmp_smp_pos) + ", amp=" + std::to_string(jmp_amp));

				const size_t blep_size = GetBlepSize();
				size_t blep_idx = (jmp_idx >= blep_peek) ? 0 : blep_peek - jmp_idx;
				for (size_t buf_idx = jmp_idx + blep_idx - blep_peek;
					blep_idx < blep_size && buf_idx < numSamples;
					++blep_idx, ++buf_idx)
				{
					LOG_SPIKE("Blep index", blep_idx);
					LOG_SPIKE("Buffer index", buf_idx);

					//const float blep_res = GetBlepRes(blep_idx, jmp_smp_pos);
					//LOG_SPIKE("Blep residue", blep_res);
					//const float newsample = buf[buf_idx].AsFloat32() + GetAmpAtBufIdx(buf_idx) * jmp_amp * blep_res;
					//LOG_SPIKE("New sample", newsample);
					//BS_LOG(((newsample < 0.0f) == (buf[buf_idx].AsFloat32() < 0.0f)) && ((newsample * newsample) > (0.25f * 0.25f)) && ((newsample * newsample) > (4.0f * [](const float x) { return x * x; }(buf[buf_idx].AsFloat32()))), "Spike at sample " + std::to_string(buf_idx) + ". Was " + std::to_string(buf[buf_idx].AsFloat32()) + " is " + std::to_string(newsample));
					//AA_LOG("\tSample at buf idx " + std::to_string(buf_idx) + ": before=" + std::to_string(buf[buf_idx].AsFloat32()) + ", after=" + std::to_string(newsample) + ", blep res=" + std::to_string(blep_res));
					//buf[buf_idx] = newsample;

					// DISABLE TO TEST POPS
					const double blepres = static_cast<double>(GetAmpAtBufIdx(buf_idx)) * jmp_amp * GetBlepRes(blep_idx, jmp_smp_pos);
					buf64[buf_idx] += blepres;
				}

				if (blep_idx < blep_size && jmp_idx < numSamples)
					antiAliasQueue.push(blep_idx, jmp_smp_pos, jmp_amp);
			}
#endif

			for (size_t smp = 0; smp < numSamples; ++smp)
			{
				buf[smp] = static_cast<float>(buf64[smp]);
			}
#endif

			for (size_t ch = 1; ch < numChannels; ++ch)
			{
				// Debugging spike
				//bufs[ch][0] = 0.5f;
				for (size_t smp = 0; smp < numSamples; ++smp)
				{
					bufs[ch][smp] = buf[smp];
				}
			}
		}

	private:
		float GetAmpAtBufIdx(const size_t buf_idx) const
		{
			return buf_amp_cache[buf_idx];
		}

		void GetNextWaveformSample(double* const outWaveformSample, const double deltaTime, double& outNormalizedPhase, float& outAmplitude, float& outFrequency)
		{
			if (waveformSampleQueue.empty())
			{
				const double waveformSample = CalculateNextWaveformSample(deltaTime, outNormalizedPhase, outAmplitude, outFrequency);
				if (outWaveformSample)
				{
					*outWaveformSample = waveformSample;
				}
				return;
			}

			const SampleMetadata& nextSample = waveformSampleQueue.peek();
			if (deltaTime != nextSample.deltaTime)
			{
				throw std::runtime_error("deltaTime changed between peeking and getting next sample");
			}

			outNormalizedPhase = nextSample.normalizedPhase;
			outAmplitude = nextSample.amp;
			outFrequency = nextSample.freq;
			const double waveformSample = nextSample.nextWaveformSample;
			waveformSampleQueue.pop_idx();
			if (outWaveformSample)
			{
				*outWaveformSample = waveformSample;
			}
			return;
		}

		void PeekNextWaveformSample(double* const outWaveformSample, const double deltaTime, double& outNormalizedPhase, float& outAmplitude, float& outFrequency, const size_t numToSkip = 0)
		{
			const size_t numToPush = (numToSkip < waveformSampleQueue.size()) ? 0 : numToSkip + 1 - waveformSampleQueue.size();

			if (numToPush > 0)
			{
				double normalizedPhase;
				float amp;
				float freq;
				double nextWaveformSample;
				for (size_t i = 0; i < numToPush; ++i)
				{
					nextWaveformSample = CalculateNextWaveformSample(deltaTime, normalizedPhase, amp, freq);
					waveformSampleQueue.push(deltaTime, nextWaveformSample, normalizedPhase, amp, freq);
				}
				outNormalizedPhase = normalizedPhase;
				outAmplitude = amp;
				outFrequency = freq;
				if (outWaveformSample)
				{
					*outWaveformSample = nextWaveformSample;
				}
				return;
			}

			const SampleMetadata& nextSample = waveformSampleQueue.peek(numToSkip);
			if (deltaTime != nextSample.deltaTime)
			{
				throw std::runtime_error("deltaTime changed between queue peekings");
			}

			outNormalizedPhase = nextSample.normalizedPhase;
			outAmplitude = nextSample.amp;
			outFrequency = nextSample.freq;
			if (outWaveformSample)
			{
				*outWaveformSample = nextSample.nextWaveformSample;
			}
			return;
		}

		double CalculateNextWaveformSample(const double deltaTime, double& outNormalizedPhase, float& outAmplitude, float& outFrequency)
		{
			Increment(deltaTime);
			const double normalizedPhase = GetInstantaneousPhase();
#if 0
			const double phase = GetInstantaneousPhase();
			const double normalizedPhase = phase - std::floor(phase);
#endif
			const float amp = GetAmplitude();
			const double output = amp * Waveform2(normalizedPhase);
			outNormalizedPhase = normalizedPhase;
			outAmplitude = amp;
			outFrequency = GetFrequency();
			return output;
		}

		double Waveform(const double phase) const
		{
			double wavePos = 0.0;
			for (const Jump& jump : jumps)
			{
				const double sawPhase = phase - jump.pos;
				const double sawNormPhase = sawPhase + ((FloatToBits(sawPhase) & SignBit<double>::value) >> BitNum<SignBit<double>::value>::value);
				wavePos += static_cast<double>(jump.amp) * (0.5 - sawNormPhase);
			}
			return wavePos;
		}

		double Waveform2(const double phase) const
		{
			double wavePos = 0.0;
			for (const Jump& jump : jumps)
			{
				wavePos += static_cast<double>(jump.amp)*((static_cast<double>(int(phase >= jump.pos)) - 0.5) + (jump.pos - phase));
			}
			return wavePos;
		}

		double AdditiveWaveform(const double phase, const float freq, const float maxFreq) const
		{
			static constexpr const float normalizeAmp = 2.0f / HalfTau<float>();
			double wavePos = 0.0;
			for (const Jump& jump : jumps)
			{
				const double sawPhase = phase - jump.pos;
				const double sawNormPhase = sawPhase + ((FloatToBits(sawPhase) & SignBit<double>::value) >> BitNum<SignBit<double>::value>::value);
				size_t n = 1;
				float amp = jump.amp * normalizeAmp;
				for (double harm = freq; harm < maxFreq; harm += freq)
				{
					wavePos += amp * fast::sin(Tau<double>() * harm * sawNormPhase) / (float)n;
					++n;
					amp = -amp;
				}
			}
			return wavePos;
		}

		void GetJumpsInPhaseRange(const double phase1, const double phase2, const size_t smpnum, const double smpval, const bool bHardSync, Vector<std::pair<size_t, std::pair<double, float>>>& sampleStreamJumps)
		{
			GetJumpsInPhaseRange(phase1, phase2, smpnum, static_cast<float>(smpval), bHardSync, sampleStreamJumps);
		}

		void GetJumpsInPhaseRange(const double phase1, const double phase2, const size_t smpnum, const float smpval, const bool bHardSync, Vector<std::pair<size_t, std::pair<double, float>>>& sampleStreamJumps)
		{
			if (phase1 < 0.0)
			{
				return;
			}

			if (bHardSync)
			{
				sampleStreamJumps.push_back(std::make_pair(smpnum, std::make_pair(0.5, -smpval)));
				return;
			}

			if (phase1 < phase2)
			{
				size_t i = 0;
				for (; i < jumps.size(); ++i)
					if (jumps[i].pos >= phase1)
						break;

				/*
			for (jump in jumps)
			{
				const double sawPhase1 = phase1 - jump.pos;
				const double wrap1 = ((FloatToBits(sawPhase1) & SignBit<double>::value) >> BitNum<SignBit<double>::value>::value);
				const double sawNormPhase1 = sawPhase1 + wrap1;
				wavePos1 += static_cast<double>(jump.amp) * (0.5 - sawNormPhase1);
			}
				*/
				/*
			for (jump in jumps)
			{
				const double sawPhase2 = phase2 - jump.pos;
				const double wrap2 = ((FloatToBits(sawPhase2) & SignBit<double>::value) >> BitNum<SignBit<double>::value>::value);
				const double sawNormPhase2 = sawPhase2 + wrap2;
				wavePos += static_cast<double>(jump.amp) * (0.5 - sawNormPhase2);
			}
				*/

				//for (jump in jumps)
				//	wavePos += static_cast<double>(jump.amp)*((static_cast<double>(int(phase >= jump.pos)) - 0.5) + (jump.pos - phase));

				// Higher precision to prevent high frequencies rounding to the wrong side of the blep residue
				const double phaseStretch = 1.0 / (phase2 - phase1);
				for (; i < jumps.size(); ++i)
				{
					if (jumps[i].pos < phase2)
					{
						const double jmppos = phaseStretch * (jumps[i].pos - phase1);
						sampleStreamJumps.push_back(std::make_pair(smpnum, std::make_pair(jmppos, jumps[i].amp)));
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 < p2): jumps[i].pos", jumps[i].pos);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 < p2): phase1", phase1);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 < p2): phaseStretch", phaseStretch);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 < p2): jmppos", jmppos);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase2), "Phase2 equal (p1 < p2): phase2", phase2);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase2), "Phase2 equal (p1 < p2): phaseStretch", phaseStretch);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase2), "Phase2 equal (p1 < p2): jmppos", jmppos);
					}
					else
					{
						break;
					}
				}
			}
			else
			{
				// Higher precision to prevent high frequencies rounding to the wrong side of the blep residue
				const double phaseStretch = 1.0 / ((phase2 + 1.0) - phase1);
				size_t i = 0;
				for (; i < jumps.size(); ++i)
				{
					if (jumps[i].pos < phase2)
					{
						const double jmppos = phaseStretch * ((jumps[i].pos + 1.0) - phase1);
						sampleStreamJumps.push_back(std::make_pair(smpnum, std::make_pair(jmppos, jumps[i].amp)));
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 >= p2, j[i].pos < p2): phase1", phase1);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 >= p2, j[i].pos < p2): phaseStretch", phaseStretch);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 >= p2, j[i].pos < p2): jmppos", jmppos);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase2), "Phase2 equal (p1 >= p2, j[i].pos < p2): phase2", phase2);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase2), "Phase2 equal (p1 >= p2, j[i].pos < p2): phaseStretch", phaseStretch);
						LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase2), "Phase2 equal (p1 >= p2, j[i].pos < p2): jmppos", jmppos);
					}
					else
					{
						break;
					}
				}

				for (; i < jumps.size(); ++i)
					if (jumps[i].pos >= phase1)
						break;

				for (; i < jumps.size(); ++i)
				{
					const double jmppos = phaseStretch * (jumps[i].pos - phase1);
					sampleStreamJumps.push_back(std::make_pair(smpnum, std::make_pair(jmppos, jumps[i].amp)));
					LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 >= p2): phase1", phase1);
					LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 >= p2): phaseStretch", phaseStretch);
					LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase1), "Phase1 equal (p1 >= p2): jmppos", jmppos);
					LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase2), "Phase2 equal (p1 >= p2): phase2", phase2);
					LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase2), "Phase2 equal (p1 >= p2): phaseStretch", phaseStretch);
					LOG_SPIKE_COND(nearly_equal(jumps[i].pos, phase2), "Phase2 equal (p1 >= p2): jmppos", jmppos);
				}
			}
		}

		// Example with 8-sample sinc:
		//  Jump point:
		//         X
		//  Sinc (8 samples):
		//        *|*
		//  * * *  |  * * *
		//  0 1 2 3|4 5 6 7
		// Blep (running sum + starting value = 9 samples):
		//         | * * * *
		//         *
		// * * * * |
		// 0 1 2 3 4 5 6 7 8
		//   Assume end points are 0 and 1 (7 samples):
		//         | * * *
		//         *
		//   * * * |
		//   0 1 2 3 4 5 6
		// Jump position is "phase" between two samples
		// Jump position corresponds to blep idx 3 or n/2-1
		//
		//  Waveform:
		//                        ***********************
		//                        |
		//  **********************|
		//  Jump point:           ^
		//                      / | \
		//  Samples:              |
		//                        | *     *     *     *
		//                        | |
		//  *     *     *     *   | |
		//  Blep:             |   | |
		//                    |   | |   *     *     *
		//                    |   * |   |
		//      *     *     * |   | |   |
		//      0     1     2 |   3 |   4     5     6
		//  Blep + Samples:   |   | |   |
		//                    |   | *   + *   + *   + *
		//                    |   + |   |
		//  *   + *   + *   + *   | |   |
		//      0     1     2 |   3 |   4     5     6
		//                    |   | |   |
		//                    *   + *   |
		//                    |___| |   |
		//                      | | |   |
		//          Jump position | |   |
		//                        + *   +
		//                        |_|
		//                         |
		//                  Offset from blep (1 - jump position)
		//

		static double GetBlepResPrecise(const size_t blep_idx, const double jmp_pos)
		{
			// TODO: Ensure rounding error from multiply vs. divide is acceptable
			//const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) / static_cast<double>(BLEP_POLYS);
			constexpr const double scaling = 1.0 / static_cast<double>(BLEP_POLYS);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = blep_precise[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetBlepResFast(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(BLEP_POLYS_FAST);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = blep_fast[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetBlepResXfast(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(BLEP_POLYS_XFAST);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = blep_xfast[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetMBlepResPrecise(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(MBLEP_POLYS);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = mblep_precise[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetMBlepResFast(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(MBLEP_POLYS_FAST);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = mblep_fast[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetMBlepResXfast(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(MBLEP_POLYS_XFAST);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = mblep_xfast[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetRBlepResPrecise(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(RBLEP_POLYS);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = rblep_precise[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetRBlepResFast(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(RBLEP_POLYS_FAST);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = rblep_fast[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetRBlepResXfast(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(RBLEP_POLYS_XFAST);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = rblep_xfast[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetHBlepResPrecise(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(HBLEP_POLYS);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = hblep_precise[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetHBlepResFast(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(HBLEP_POLYS_FAST);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = hblep_fast[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static double GetHBlepResXfast(const size_t blep_idx, const double jmp_pos)
		{
			constexpr const double scaling = 1.0 / static_cast<double>(HBLEP_POLYS_XFAST);
			const double blep_pos = ((double)blep_idx + 0.5 - jmp_pos) * scaling;
			if (blep_pos < 0.0 || blep_pos >= 1.0)
			{
				return 0.0;
			}

			const double blep_val = hblep_xfast[blep_idx](blep_pos);
			return (blep_pos < 0.5) ? blep_val : blep_val - 1.0;
		}

		static size_t GetBlepSizePrecise()
		{
			return blep_precise.size();
		}

		static size_t GetBlepSizeFast()
		{
			return blep_fast.size();
		}

		static size_t GetBlepSizeXfast()
		{
			return blep_xfast.size();
		}

		static size_t GetMBlepSizePrecise()
		{
			return mblep_precise.size();
		}

		static size_t GetMBlepSizeFast()
		{
			return mblep_fast.size();
		}

		static size_t GetMBlepSizeXfast()
		{
			return mblep_xfast.size();
		}

		static size_t GetRBlepSizePrecise()
		{
			return rblep_precise.size();
		}

		static size_t GetRBlepSizeFast()
		{
			return rblep_fast.size();
		}

		static size_t GetRBlepSizeXfast()
		{
			return rblep_xfast.size();
		}

		static size_t GetHBlepSizePrecise()
		{
			return hblep_precise.size();
		}

		static size_t GetHBlepSizeFast()
		{
			return hblep_fast.size();
		}

		static size_t GetHBlepSizeXfast()
		{
			return hblep_xfast.size();
		}

		static size_t GetBlepPeek(const EInfiniSawPrecision ePrecision)
		{
			switch (ePrecision)
			{

			case EInfiniSawPrecision::Precise: return BLEP_PEEK;
			case EInfiniSawPrecision::Fast: return BLEP_PEEK_FAST;
			case EInfiniSawPrecision::ExtraFast: return BLEP_PEEK_XFAST;

			case EInfiniSawPrecision::MPrecise: return MBLEP_PEEK;
			case EInfiniSawPrecision::MFast: return MBLEP_PEEK_FAST;
			case EInfiniSawPrecision::MExtraFast: return MBLEP_PEEK_XFAST;

			case EInfiniSawPrecision::RPrecise: return RBLEP_PEEK;
			default:
			case EInfiniSawPrecision::RFast: return RBLEP_PEEK_FAST;
			case EInfiniSawPrecision::RExtraFast: return RBLEP_PEEK_XFAST;

			case EInfiniSawPrecision::HPrecise: return HBLEP_PEEK;
			case EInfiniSawPrecision::HFast: return HBLEP_PEEK_FAST;
			case EInfiniSawPrecision::HExtraFast: return HBLEP_PEEK_XFAST;

			}
		}

		static auto GetGetBlepRes(const EInfiniSawPrecision ePrecision) -> double(*)(const size_t, const double)
		{
			switch (ePrecision)
			{

			case EInfiniSawPrecision::Precise: return &InfiniSaw::GetBlepResPrecise;
			case EInfiniSawPrecision::Fast: return &InfiniSaw::GetBlepResFast;
			case EInfiniSawPrecision::ExtraFast: return &InfiniSaw::GetBlepResXfast;

			case EInfiniSawPrecision::MPrecise: return &InfiniSaw::GetMBlepResPrecise;
			case EInfiniSawPrecision::MFast: return &InfiniSaw::GetMBlepResFast;
			case EInfiniSawPrecision::MExtraFast: return &InfiniSaw::GetMBlepResXfast;

			case EInfiniSawPrecision::RPrecise: return &InfiniSaw::GetRBlepResPrecise;
			default:
			case EInfiniSawPrecision::RFast: return &InfiniSaw::GetRBlepResFast;
			case EInfiniSawPrecision::RExtraFast: return &InfiniSaw::GetRBlepResXfast;

			case EInfiniSawPrecision::HPrecise: return &InfiniSaw::GetHBlepResPrecise;
			case EInfiniSawPrecision::HFast: return &InfiniSaw::GetHBlepResFast;
			case EInfiniSawPrecision::HExtraFast: return &InfiniSaw::GetHBlepResXfast;

			}
		}

		static auto GetGetBlepSize(const EInfiniSawPrecision ePrecision) -> size_t(*)()
		{
			switch (ePrecision)
			{

			case EInfiniSawPrecision::Precise: return &InfiniSaw::GetBlepSizePrecise;
			case EInfiniSawPrecision::Fast: return &InfiniSaw::GetBlepSizeFast;
			case EInfiniSawPrecision::ExtraFast: return &InfiniSaw::GetBlepSizeXfast;

			case EInfiniSawPrecision::MPrecise: return &InfiniSaw::GetMBlepSizePrecise;
			case EInfiniSawPrecision::MFast: return &InfiniSaw::GetMBlepSizeFast;
			case EInfiniSawPrecision::MExtraFast: return &InfiniSaw::GetMBlepSizeXfast;

			case EInfiniSawPrecision::RPrecise: return &InfiniSaw::GetRBlepSizePrecise;
			default:
			case EInfiniSawPrecision::RFast: return &InfiniSaw::GetRBlepSizeFast;
			case EInfiniSawPrecision::RExtraFast: return &InfiniSaw::GetRBlepSizeXfast;

			case EInfiniSawPrecision::HPrecise: return &InfiniSaw::GetHBlepSizePrecise;
			case EInfiniSawPrecision::HFast: return &InfiniSaw::GetHBlepSizeFast;
			case EInfiniSawPrecision::HExtraFast: return &InfiniSaw::GetHBlepSizeXfast;

			}
		}

	private:
		Vector<Jump> jumps;
		Vector<float> buf_amp_cache;
		Vector<size_t> hardSyncs;
		StaticCircleQueue<SampleMetadata, 6> waveformSampleQueue;
		StaticCircleQueue<JumpMetadata, 16> antiAliasQueue;
		size_t blep_peek;
		double (*GetBlepRes)(const size_t, const double);
		size_t (*GetBlepSize)();

#if defined(INFINISAW_LOG_BLEPSPIKE_NEW) && INFINISAW_LOG_BLEPSPIKE_NEW
		bool bLogSpike;
#endif

#if defined(INFINISAW_LOG_ANTIALIAS) && INFINISAW_LOG_ANTIALIAS
		bool bAaLog;
#endif

#if INFINISAW_LOG_BLEPSPIKE
		bool bBsLog;
#endif

		/*
		static std::ofstream*& GetNaiveSawFilePtr(const size_t idx)
		{
			static std::ofstream* apfiles[1024] = { 0 };
			return apfiles[idx & 1023];
		}
		static std::ofstream& GetNaiveSawFile(const size_t idx)
		{
			std::ofstream*& pfile = GetNaiveSawFilePtr(idx);
			if (!pfile)
				pfile = new std::ofstream("naivesaw" + std::to_string(idx) + ".wav", std::ios::write | std::ios::binary);
			return *pfile;
		}
		static void CloseNaiveSawFile(const size_t idx)
		{
			std::ofstream*& pfile = GetNaiveSawFilePtr(idx);
			if (pfile)
			{
				delete pfile;
				pfile = nullptr;
			}
		}

		static std::ofstream*& GetBlepResFilePtr(const size_t idx)
		{
			static std::ofstream* apfiles[1024] = { 0 };
			return apfiles[idx & 1023];
		}
		static std::ofstream& GetBlepResFile(const size_t idx)
		{
			std::ofstream*& pfile = GetBlepResFilePtr(idx);
			if (!pfile)
				pfile = new std::ofstream("blepres" + std::to_string(idx) + ".wav", std::ios::write | std::ios::binary);
			return *pfile;
		}
		static void CloseBlepResFile(const size_t idx)
		{
			std::ofstream*& pfile = GetBlepResFilePtr(idx);
			if (pfile)
			{
				delete pfile;
				pfile = nullptr;
			}
		}

		static size_t NextFileIdx()
		{
			static std::atomic<size_t> nextfileidx = 0;
			return nextfileidx++;
		}
		size_t fileidx;
		*/

	private:
		static Array<BlepPolyType, BLEP_POLYS> blep_precise;
		static Array<BlepPolyType_Fast, BLEP_POLYS_FAST> blep_fast;
		static Array<BlepPolyType_Xfast, BLEP_POLYS_XFAST> blep_xfast;
		static Array<MBlepPolyType, MBLEP_POLYS> mblep_precise;
		static Array<MBlepPolyType_Fast, MBLEP_POLYS_FAST> mblep_fast;
		static Array<MBlepPolyType_Xfast, MBLEP_POLYS_XFAST> mblep_xfast;
		static Array<RBlepPolyType, RBLEP_POLYS> rblep_precise;
		static Array<RBlepPolyType_Fast, RBLEP_POLYS_FAST> rblep_fast;
		static Array<RBlepPolyType_Xfast, RBLEP_POLYS_XFAST> rblep_xfast;
		static Array<HBlepPolyType, HBLEP_POLYS> hblep_precise;
		static Array<HBlepPolyType_Fast, HBLEP_POLYS_FAST> hblep_fast;
		static Array<HBlepPolyType_Xfast, HBLEP_POLYS_XFAST> hblep_xfast;
	};

	inline void InfiniSawEvent::Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const
	{
		if (jumpParam == EInfiniSawParam::SynthParam)
		{
			SynthEvent<InfiniSawEvent>::Activate(ctrl, sampleNum);
			return;
		}

		InfiniSaw& infiniSaw = ctrl.Get<InfiniSaw>();
		switch (jumpParam)
		{
		case EInfiniSawParam::SynthParam: break; // Solely to prevent warnings as this is impossible
		case EInfiniSawParam::HardSync:
			infiniSaw.HardSync(sampleNum);
			break;
		}
	}
}

