// Copyright Dan Price. All rights reserved.

#pragma once

#include "DrumHit.h"
#include "Bessel.h"
#include "SineSynth.h"
#include "Synth.h"
#include "Ramp.h"
#include "FastSin.h"
#include "InfiniSaw.h"
#include "Random.h"
#include "Filter.h"
#include "Envelope.h"
#include "Utility.h"
#include "Memory.h"
#include <utility>
#include <cmath>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace json2wav
{
	enum class EDrumHitSynthParam
	{
		SynthParam, HitRadius, HitAngle, MicRadius, ResetPhase, Blep, BlepPrecision, Hit
	};

	struct DrumHitSynthEvent : public SynthEvent<DrumHitSynthEvent>
	{
	private:
		static ESynthParam VerifyDrumHitSynthParam(const EDrumHitSynthParam param)
		{
			if (param == EDrumHitSynthParam::SynthParam)
			{
				throw std::runtime_error("Don't initialize DrumHitSynthEvents with EDrumHitSynthParam::SynthParam! Use ESynthParam values instead.");
			}
			return static_cast<ESynthParam>(-1);
		}

	public:
		template<typename... RampArgTypes>
		DrumHitSynthEvent(const ESynthParam param_init, RampArgTypes&&... rampArgs)
			: SynthEvent<DrumHitSynthEvent>(param_init, std::forward<RampArgTypes>(rampArgs)...)
			, drumHitParam(EDrumHitSynthParam::SynthParam)
			, jump(0.0, 0.0f)
			, ePrecision(EInfiniSawPrecision::RFast)
			, hitStrength(0.0f)
		{
		}

		template<typename... RampArgTypes>
		DrumHitSynthEvent(const EDrumHitSynthParam param_init, RampArgTypes&&... rampArgs)
			: SynthEvent<DrumHitSynthEvent>(VerifyDrumHitSynthParam(param_init))
			, drumHitParam(param_init)
			, ramp(std::forward<RampArgTypes>(rampArgs)...)
			, jump(0.0, 0.0f)
			, ePrecision(EInfiniSawPrecision::RFast)
			, hitStrength(0.0f)
		{
		}

		DrumHitSynthEvent(const InfiniSaw::Jump& jump_init)
			: SynthEvent<DrumHitSynthEvent>(static_cast<ESynthParam>(-1))
			, drumHitParam(EDrumHitSynthParam::Blep)
			, jump(jump_init)
			, ePrecision(EInfiniSawPrecision::RFast)
			, hitStrength(0.0f)
		{
		}

		DrumHitSynthEvent(const EInfiniSawPrecision ePrecision_init)
			: SynthEvent<DrumHitSynthEvent>(static_cast<ESynthParam>(-1))
			, drumHitParam(EDrumHitSynthParam::BlepPrecision)
			, jump(0.0, 0.0f)
			, ePrecision(ePrecision_init)
			, hitStrength(0.0f)
		{
		}

		DrumHitSynthEvent(const float hitStrengthInit, const float durDummy = 0.0f)
			: SynthEvent<DrumHitSynthEvent>(static_cast<ESynthParam>(-1))
			, drumHitParam(EDrumHitSynthParam::Hit)
			, jump(0.0, 0.0f)
			, ePrecision(EInfiniSawPrecision::RFast)
			, hitStrength(hitStrengthInit)
		{
		}

		virtual void Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const override;

		const EDrumHitSynthParam drumHitParam;
		const Ramp ramp;
		const InfiniSaw::Jump jump;
		const EInfiniSawPrecision ePrecision;
		const float hitStrength;
	};

	class alignas(32) DrumHitSynth : public SynthWithCustomEvent<DrumHitSynthEvent>
	{
	public:
		using Jump = InfiniSaw::Jump;
		using JumpMetadata = InfiniSaw::JumpMetadata;
		using FiltType = Filter::BiquadPeak<false, 1, Filter::ETopo::DF2>;

		static constexpr const size_t NUM_FILTS = 4;

	public:
		DrumHitSynth(const float frequency_init = 100.0f
			, const float mic_init = 0.0f
			, const float hit_range = 0.2f
			, const float th_init = 0.0f
			, const bool bActivateFilters = true)
			: SynthWithCustomEvent<DrumHitSynthEvent>(frequency_init, 0.0f, 0.0f)
			, hit(0.0f), th(th_init), mic(mic_init)
			, rdist(0.0f, hit_range)
			, strenToAmp(0.25f)
			, transientTime(0.00025)
			, transientShape(ERampShape::SCurve)
			, decayDelay(0.1)
			, decayAmount(0.001f)
			, decayTime(2.0)
			, decayShape(ERampShape::LogScaleLinear)
			, fundFreq(frequency_init)
			, detuneDelay(0.00075)
			, detuneAmount(0.9f)
			, detuneTime(1.0)
			, detuneShape(ERampShape::LogScaleLinear)
			, lastSampleRate(0)
			, bReentering(false)
			, bFiltersActive(false)
			, filts{ ctrls.CreatePtr<FiltType>(8000.0f, 0.5f),
				ctrls.CreatePtr<FiltType>(2500.0f, 0.5f),
				ctrls.CreatePtr<FiltType>(800.0f, 0.7f),
				ctrls.CreatePtr<FiltType>(fundFreq, 0.7f) }
			, envs{ Envelope(0.00125f, 0.0125f, 0.0625f, 48.0f, 36.0f, ERampShape::SCurve, ERampShape::Linear, ERampShape::Linear),
				Envelope(0.001875f, 0.01875f, 0.09375f, 24.0f, 18.0f, ERampShape::SCurve, ERampShape::Linear, ERampShape::Linear),
				Envelope(0.00375f, 0.0375f, 0.1875f, 9.0f, 6.0f, ERampShape::SCurve, ERampShape::Linear, ERampShape::Linear),
				Envelope(0.005f, 0.05f, 0.25f, 9.0f, 6.0f, ERampShape::SCurve, ERampShape::Linear, ERampShape::Linear) }
			, filtdels{ 0.0f, 0.0f, 0.0f, 0.005f }
			, dumb(MakeShared<BasicAudioSum<false, false>>())
		{
#ifdef __AVX2__
			alignas(32) static double arrzero[4] = { 0.0 };
			alignas(32) static float arrone[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			__m256d mmzero = _mm256_load_pd(arrzero);
			__m128 mmone = _mm_load_ps(arrone);
#endif
			for (size_t order = 0; order < DrumHit::NumOrders; ++order)
			{
#ifdef __AVX2__
				for (size_t zero = 0; zero < DrumHit::NumZeroes; zero += 4)
#else
				for (size_t zero = 0; zero < DrumHit::NumZeroes; ++zero)
#endif
				{
					//freqs[order][zero] = 0.0f;
					amps[order][zero] = 0.0f;
#ifdef __AVX2__
					amps[order][zero + 1] = 0.0f;
					amps[order][zero + 2] = 0.0f;
					amps[order][zero + 3] = 0.0f;
					_mm_store_ps(modecay[order] + zero, mmone);
					_mm256_store_pd(phases[order] + zero, mmzero);
					_mm256_store_pd(dphases[order] + zero, mmzero);
#else
					modecay[order][zero] = 1.0f;
					phases[order][zero] = 0.0;
					dphases[order][zero] = 0.0;
#endif
				}
			}

			dumb->AddInput(this);
			if (bActivateFilters)
				ActivateFilters();
		}

		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t numSamples,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			if (bReentering)
			{
				Sample* const buf = bufs[0];
				const double deltaTime = 1.0 / static_cast<double>(sampleRate);
				OnFrequencyChange(GetFrequency(), deltaTime);
				//OnAmplitudeChange(GetAmplitude(), deltaTime);
				OnHitChange();
				GetSynthSamples(bufs, 1, numSamples, false, [this, buf, deltaTime](const size_t i)
					{
						const bool bIncAmp = IncrementHit(deltaTime);
						Increment(deltaTime);
						const float amp = GetAmplitude();
						if (bIncAmp)
							OnHitChange();
						float smp = 0.0f;
						if (Utility::FloatAbsGreaterEqual(amp, 0.0001f))
						{
							IncrementPhases(deltaTime);
#ifdef __AVX2__
							__m128 amp4 = _mm_set1_ps(amp);
							__m128 acc0 = _mm_set1_ps(0.0f);
							__m128 acc1 = _mm_set1_ps(0.0f);
							__m256d tau4 = _mm256_set1_pd(vTau<double>::value);
#endif
							for (size_t order = 0; order < DrumHit::NumOrders; ++order)
							{
#ifdef __AVX2__
								for (size_t zero = 0; zero < DrumHit::NumZeroes; zero += 8)
#else
								for (size_t zero = 0; zero < DrumHit::NumZeroes; ++zero)
#endif
								{
#ifdef __AVX2__
									__m256d phase0 = _mm256_load_pd(phases[order] + zero);
									__m256d phase1 = _mm256_load_pd(phases[order] + zero + 4);
									__m128 cos0 = fast::cos(_mm256_mul_pd(phase0, tau4));
									__m128 cos1 = fast::cos(_mm256_mul_pd(phase1, tau4));
									cos0 = _mm_mul_ps(cos0, _mm_load_ps(amps[order] + zero));
									cos1 = _mm_mul_ps(cos1, _mm_load_ps(amps[order] + zero + 4));
									acc0 = _mm_fmadd_ps(cos0, amp4, acc0);
									acc1 = _mm_fmadd_ps(cos1, amp4, acc1);
#else
									smp += amp * amps[order][zero] * fast::cos(phases[order][zero] * vTau<double>::value);
#endif
								}
#ifdef __AVX2__
								float acc[8];
								_mm_store_ps(acc, acc0);
								_mm_store_ps(acc + 4, acc1);
								//for (size_t j = 1; j < 8; ++j)
									//acc[0] += acc[j];
								//smp = acc[0];
								acc[0] += acc[1];
								acc[2] += acc[3];
								acc[4] += acc[5];
								acc[6] += acc[7];
								acc[0] += acc[2];
								acc[4] += acc[6];
								smp = acc[0] + acc[4];

								/*acc0 = _mm_hadd_ps(acc0, acc1);
								acc0 = _mm_hadd_ps(acc0, acc0);
								float acc[4];
								_mm_store_ps(acc, acc0);
								smp = acc[0] + acc[1];*/
#endif
							}

							IncrementAmps(deltaTime);
						}
						buf[i] = smp;
					});

				InfiniSaw::BlepBuf(buf, numSamples, jumps, ePrecision);
				jumps.clear();
			}
			else
			{
				if (numChannels == 0)
					return;

				if (lastSampleRate == 0)
					lastSampleRate = sampleRate;
				else if (lastSampleRate != sampleRate)
					return;

				bReentering = true;
				filts[0]->GetSamples(bufs, 1, numSamples, sampleRate, requester);
				bReentering = false;
				Sample* const buf = bufs[0];
				for (size_t ch = 1; ch < numChannels; ++ch)
					for (size_t idx = 0; idx < numSamples; ++idx)
						bufs[ch][idx] = buf[idx];
			}
		}

		float GetRelease() const
		{
			return static_cast<float>(transientTime + decayDelay + decayTime + 0.001);
		}

		void SetHitRadius(const Ramp& ramp)
		{
			hitRamp = ramp;
		}

		void SetHitRadius(const float hitr)
		{
			hit = hitr;
		}

		void SetHitAngle(const Ramp& ramp)
		{
			angRamp = ramp;
		}

		void SetHitAngle(const float hith)
		{
			th = hith;
		}

		void SetMicRadius(const Ramp& ramp)
		{
			micRamp = ramp;
		}

		void SetStrengthToAmp(const float strenToAmpSet)
		{
			strenToAmp = strenToAmpSet;
		}

		void SetTransientTime(const double transientTimeSet)
		{
			transientTime = transientTimeSet;
		}

		void SetTransientShape(const ERampShape transientShapeSet)
		{
			transientShape = transientShapeSet;
		}

		void SetDecayDelay(const double decayDelaySet)
		{
			decayDelay = decayDelaySet;
		}

		void SetDecayAmount(const float decayAmountSet)
		{
			decayAmount = decayAmountSet;
		}

		void SetDecayTime(const double decayTimeSet)
		{
			decayTime = decayTimeSet;
		}

		void SetDecayShape(const ERampShape decayShapeSet)
		{
			decayShape = decayShapeSet;
		}

		void SetFundamental(const float fundFreqSet)
		{
			fundFreq = fundFreqSet;
		}

		void SetDetuneDelay(const double detuneDelaySet)
		{
			detuneDelay = detuneDelaySet;
		}

		void SetDetuneAmount(const float detuneAmountSet)
		{
			detuneAmount = detuneAmountSet;
		}

		void SetDetuneTime(const double detuneTimeSet)
		{
			detuneTime = detuneTimeSet;
		}

		void SetDetuneShape(const ERampShape detuneShapeSet)
		{
			detuneShape = detuneShapeSet;
		}

		template<size_t filtidx, typename... ParamTypes>
		void SetFilt(ParamTypes&&... params)
		{
			static_assert(filtidx < NUM_FILTS, "DrumHitSynth::SetFilt<filtidx>(): filtidx must be less than 4.");

			if (bFiltersActive)
			{
				if constexpr (filtidx > 0)
					filts[filtidx - 1]->RemoveInput(filts[filtidx]);
				if constexpr (filtidx + 1 == NUM_FILTS)
					filts[filtidx]->RemoveInput(dumb);
				else
					filts[filtidx]->RemoveInput(filts[filtidx + 1]);
			}
			ctrls.Remove(filts[filtidx]);
			filts[filtidx] = ctrls.CreatePtr<FiltType>(std::forward<ParamTypes>(params)...);
			if (bFiltersActive)
			{
				if constexpr (filtidx + 1 == NUM_FILTS)
					filts[filtidx]->AddInput(dumb);
				else
					filts[filtidx]->AddInput(filts[filtidx + 1]);
				if constexpr (filtidx > 0)
					filts[filtidx - 1]->AddInput(filts[filtidx]);
			}
		}

		template<size_t envidx, typename... ParamTypes>
		void SetEnvelope(ParamTypes&&... params)
		{
			static_assert(envidx < NUM_FILTS, "DrumHitSynth::SetEnvelope<envidx>(): envidx must be less than 4.");
			envs[envidx] = Envelope(std::forward<ParamTypes>(params)...);
		}

		template<size_t envidx>
		void SetEnvAttack(const float value)
		{
			static_assert(envidx < NUM_FILTS, "DrumHitSynth::SetEnvAttack<envidx>(): envidx must be less than 4.");
			envs[envidx].attack = value;
		}

		template<size_t envidx>
		void SetEnvDecay(const float value)
		{
			static_assert(envidx < NUM_FILTS, "DrumHitSynth::SetEnvDecay<envidx>(): envidx must be less than 4.");
			envs[envidx].decay = value;
		}

		template<size_t envidx>
		void SetEnvRelease(const float value)
		{
			static_assert(envidx < NUM_FILTS, "DrumHitSynth::SetEnvRelease<envidx>(): envidx must be less than 4.");
			envs[envidx].release = value;
		}

		template<size_t envidx>
		void SetEnvAttLevel(const float value)
		{
			static_assert(envidx < NUM_FILTS, "DrumHitSynth::SetEnvAttLevel<envidx>(): envidx must be less than 4.");
			envs[envidx].attlevel = value;
		}

		template<size_t envidx>
		void SetEnvSusLevel(const float value)
		{
			static_assert(envidx < NUM_FILTS, "DrumHitSynth::SetEnvSusLevel<envidx>(): envidx must be less than 4.");
			envs[envidx].suslevel = value;
		}

		template<size_t envidx>
		void SetEnvAttShape(const ERampShape value)
		{
			static_assert(envidx < NUM_FILTS, "DrumHitSynth::SetEnvAttShape<envidx>(): envidx must be less than 4.");
			envs[envidx].attramp = value;
		}

		template<size_t envidx>
		void SetEnvDecShape(const ERampShape value)
		{
			static_assert(envidx < NUM_FILTS, "DrumHitSynth::SetEnvDecShape<envidx>(): envidx must be less than 4.");
			envs[envidx].decramp = value;
		}

		template<size_t envidx>
		void SetEnvRelShape(const ERampShape value)
		{
			static_assert(envidx < NUM_FILTS, "DrumHitSynth::SetEnvRelShape<envidx>(): envidx must be less than 4.");
			envs[envidx].relramp = value;
		}

		template<size_t filtidx>
		void SetFiltDelay(const float delay)
		{
			static_assert(filtidx < NUM_FILTS, "DrumHitSynth::SetFiltDelay<filtidx>(): filtidx must be less than 4.");
			filtdels[filtidx] = delay;
		}

		void SetModeDecayAmp(const size_t order, const size_t zero, const float ampPerSample)
		{
			if (order < DrumHit::NumOrders && zero < DrumHit::NumZeroes)
			{
				if (ampPerSample > 1.0f)
					modecay[order][zero] = 1.0f;
				else if (ampPerSample < 0.001f)
					modecay[order][zero] = 0.001f;
				else
					modecay[order][zero] = ampPerSample;
			}
		}

		template<int sr> void SetModeDecayRT60(const size_t order, const size_t zero, const float RT60)
		{
			// secondsPer60DB = RT60
			// secondsPerDB = secondsPer60DB/60
			// dBPerSecond = 1/secondsPerDB = 60/secondsPer60DB = 60/RT60
			// dBPerSample = dBPerSecond/sr = (60/RT60)/sr
			// dBPerSampleOver20 = dBPerSample/20 = ((60/RT60)/sr)/20 = ((60/20)/sr)/RT60 = (3/sr)/RT60
			static constexpr const float threeOverSR = 3.0f/static_cast<float>(sr);
			const float dBPerSampleOver20 = threeOverSR/RT60;
			// dB = 10*log10(P/Pref)
			// P = V^2
			// dB = 10*log10(V^2/Vref^2) = 10*log10((V/Vref)^2) = 20*log10(V/Vref)
			// amp = V/Vref
			// dB = 20*log10(amp)
			// log10(amp) = dB/20
			// amp = 10^(dB/20)
			const float ampPerSample = std::powf(10.0f, dBPerSampleOver20);
			SetModeDecayAmp(order, zero, ampPerSample);
		}
		template<typename... Ts> void SetModeDecay441(Ts&&... params) { SetModeDecayRT60<44100>(std::forward<Ts>(params)...); }
		template<typename... Ts> void SetModeDecay48k(Ts&&... params) { SetModeDecayRT60<48000>(std::forward<Ts>(params)...); }
		template<typename... Ts> void SetModeDecay882(Ts&&... params) { SetModeDecayRT60<88200>(std::forward<Ts>(params)...); }
		template<typename... Ts> void SetModeDecay96k(Ts&&... params) { SetModeDecayRT60<96000>(std::forward<Ts>(params)...); }

		void ActivateFilters()
		{
			if (!bFiltersActive)
			{
				filts[3]->AddInput(dumb);
				filts[2]->AddInput(filts[3]);
				filts[1]->AddInput(filts[2]);
				filts[0]->AddInput(filts[1]);
				bFiltersActive = true;
			}
		}

		void DeactivateFilters()
		{
			if (bFiltersActive)
			{
				filts[0]->RemoveInput(filts[1]);
				filts[1]->RemoveInput(filts[2]);
				filts[2]->RemoveInput(filts[3]);
				filts[3]->RemoveInput(dumb);
				bFiltersActive = false;
			}
		}

		bool FiltersActive() const noexcept { return bFiltersActive; }

		void ResetPhase()
		{
#ifdef __AVX2__
			__m256d vzero = _mm256_set1_pd(0.0);
			for (size_t i = 0; i < DrumHit::NumOrders; ++i)
			{
				for (size_t j = 0; j < DrumHit::NumZeroes; j += 8)
				{
					_mm256_store_pd(phases[i] + j, vzero);
					_mm256_store_pd(phases[i] + j + 4, vzero);
				}
			}
#else
			for (auto& order : phases)
				for (auto& phase : order)
					phase = 0.0;
#endif
		}

		void Blep(InfiniSaw::JumpMetadata&& jump)
		{
			jumps.push_back(std::move(jump));
		}

		void BlepPrecision(const EInfiniSawPrecision ePrecisionSet)
		{
			ePrecision = ePrecisionSet;
		}

		void Hit(const float hitStrength, const unsigned long sampleNum)
		{
			static std::uniform_real_distribution<float> thdist(0.0f, vTau<float>::value);

			const float amp = GetAmplitude();
			float oldamp = 0.0f;
			for (size_t order = 0; order < DrumHit::NumOrders; ++order)
				for (size_t zero = 0; zero < DrumHit::NumZeroes; ++zero)
					oldamp += amp * amps[order][zero] * fast::cos(phases[order][zero] * vTau<double>::value);

			SetHitRadius(grng(rdist));
			SetHitAngle(grng(thdist));
			ResetPhase();

			OnHitChange();

			float newamp = 0.0f;
			for (size_t order = 0; order < DrumHit::NumOrders; ++order)
				for (size_t zero = 0; zero < DrumHit::NumZeroes; ++zero)
					newamp += amps[order][zero] * fast::cos(phases[order][zero] * vTau<double>::value);

			const float hitAmp = strenToAmp * hitStrength;
			const unsigned long decayDelaySamps = static_cast<unsigned long>(decayDelay * static_cast<double>(lastSampleRate));
			const unsigned long decayTimeSamps = static_cast<unsigned long>(decayTime * static_cast<double>(lastSampleRate));
			const unsigned long detuneDelaySamps = static_cast<unsigned long>(detuneDelay * static_cast<double>(lastSampleRate));
			const unsigned long smpstart = sampleNum + 1;
			const unsigned long smpend = sampleNum + decayDelaySamps + decayTimeSamps + 1;

			{
				Vector<size_t> eventsToErase = GetEventKeysInRange(smpstart, smpend);
				Vector<std::pair<size_t, size_t>> erasepairs;
				for (const size_t smpnum : eventsToErase)
				{
					const Vector<SharedPtr<IEvent>>& smpevts = static_cast<const DrumHitSynth*>(this)->GetEvents(smpnum);
					for (size_t i = 0, num = smpevts.size(); i < num; ++i)
					{
						const DrumHitSynthEvent& evt = static_cast<const DrumHitSynthEvent&>(*smpevts[i]);
						if (evt.param == ESynthParam::Amplitude || evt.param == ESynthParam::Frequency)
							erasepairs.emplace_back(std::make_pair(smpnum, i));
					}
				}
				for (auto it = erasepairs.rbegin(), end = erasepairs.rend(); it != end; ++it)
					RemoveEvent(it->first, it->second);
			}

			for (SharedPtr<FiltType> filt : filts)
			{
				Vector<size_t> filtevents = filt->GetEventKeysInRange(smpstart, smpend);
				Vector<std::pair<size_t, size_t>> erasepairs;
				for (const size_t smpnum : filtevents)
				{
					const Vector<SharedPtr<IEvent>>& smpevts = static_cast<const FiltType&>(*filt).GetEvents(smpnum);
					for (size_t i = 0, num = smpevts.size(); i < num; ++i)
						erasepairs.emplace_back(std::make_pair(smpnum, i));
				}
				for (auto it = erasepairs.rbegin(), end = erasepairs.rend(); it != end; ++it)
					filt->RemoveEvent(it->first, it->second);
			}

			// a*newamp = oldamp
			// a = oldamp/newamp
			SetAmplitude(oldamp / newamp);
			SetAmplitude(Ramp(hitAmp, transientTime, transientShape));
			SetFrequency(Ramp(fundFreq, 0.0));

			AddEvent(sampleNum + detuneDelaySamps, ESynthParam::Frequency, detuneAmount * fundFreq, detuneTime, detuneShape);
			AddEvent(sampleNum + decayDelaySamps, ESynthParam::Amplitude, decayAmount * hitAmp, decayTime, decayShape);
			AddEvent(sampleNum + decayDelaySamps + decayTimeSamps, ESynthParam::Amplitude, 0.0f, 0.001, ERampShape::SCurve);

			const unsigned long filt0attsamps = static_cast<unsigned long>(envs[0].attack * static_cast<float>(lastSampleRate));
			const unsigned long filt1attsamps = static_cast<unsigned long>(envs[1].attack * static_cast<float>(lastSampleRate));
			const unsigned long filt2attsamps = static_cast<unsigned long>(envs[2].attack * static_cast<float>(lastSampleRate));
			const unsigned long filt3attsamps = static_cast<unsigned long>(envs[3].attack * static_cast<float>(lastSampleRate));
			const unsigned long filt0decsamps = static_cast<unsigned long>(envs[0].decay * static_cast<float>(lastSampleRate));
			const unsigned long filt1decsamps = static_cast<unsigned long>(envs[1].decay * static_cast<float>(lastSampleRate));
			const unsigned long filt2decsamps = static_cast<unsigned long>(envs[2].decay * static_cast<float>(lastSampleRate));
			const unsigned long filt3decsamps = static_cast<unsigned long>(envs[3].decay * static_cast<float>(lastSampleRate));
			const unsigned long filt0delsamps = static_cast<unsigned long>(filtdels[0] * static_cast<float>(lastSampleRate));
			const unsigned long filt1delsamps = static_cast<unsigned long>(filtdels[1] * static_cast<float>(lastSampleRate));
			const unsigned long filt2delsamps = static_cast<unsigned long>(filtdels[2] * static_cast<float>(lastSampleRate));
			const unsigned long filt3delsamps = static_cast<unsigned long>(filtdels[3] * static_cast<float>(lastSampleRate));

			filts[0]->AddEvent(sampleNum + filt0delsamps + 1, EFilterParam::Gain, envs[0].attlevel, envs[0].attack, envs[0].attramp);
			filts[0]->AddEvent(sampleNum + filt0delsamps + filt0attsamps, EFilterParam::Gain, envs[0].suslevel, envs[0].decay, envs[0].decramp);
			filts[0]->AddEvent(sampleNum + filt0delsamps + filt0attsamps + filt0decsamps, EFilterParam::Gain, 0.0f, envs[0].release, envs[0].relramp);
			filts[1]->AddEvent(sampleNum + filt1delsamps + 1, EFilterParam::Gain, envs[1].attlevel, envs[1].attack, envs[1].attramp);
			filts[1]->AddEvent(sampleNum + filt1delsamps + filt1attsamps, EFilterParam::Gain, envs[1].suslevel, envs[1].decay, envs[1].decramp);
			filts[1]->AddEvent(sampleNum + filt1delsamps + filt1attsamps + filt1decsamps, EFilterParam::Gain, 0.0f, envs[1].release, envs[1].relramp);
			filts[2]->AddEvent(sampleNum + filt2delsamps + 1, EFilterParam::Gain, envs[2].attlevel, envs[2].attack, envs[2].attramp);
			filts[2]->AddEvent(sampleNum + filt2delsamps + filt2attsamps, EFilterParam::Gain, envs[2].suslevel, envs[2].decay, envs[2].decramp);
			filts[2]->AddEvent(sampleNum + filt2delsamps + filt2attsamps + filt2decsamps, EFilterParam::Gain, 0.0f, envs[2].release, envs[2].relramp);
			filts[3]->AddEvent(sampleNum + filt3delsamps + 1, EFilterParam::Gain, envs[3].attlevel, envs[3].attack, envs[3].attramp);
			filts[3]->AddEvent(sampleNum + filt3delsamps + filt3attsamps, EFilterParam::Gain, envs[3].suslevel, envs[3].decay, envs[3].decramp);
			filts[3]->AddEvent(sampleNum + filt3delsamps + filt3attsamps + filt3decsamps, EFilterParam::Gain, 0.0f, envs[3].release, envs[3].relramp);

			RefreshEvents();
			filts[0]->RefreshEvents();
			filts[1]->RefreshEvents();
			filts[2]->RefreshEvents();
			filts[3]->RefreshEvents();
		}

	private:
		bool IncrementHit(const double dt)
		{
			bool bIncAmp = false;
			if (hitRamp.Increment(hit, dt))
				bIncAmp = true;
			if (angRamp.Increment(th, dt))
				bIncAmp = true;
			if (micRamp.Increment(mic, dt))
				bIncAmp = true;
			return bIncAmp;
		}

		void IncrementPhases(const double dt)
		{
#ifndef __AVX2__
			static constexpr const double MaxPhase = 1.0;
			static constexpr const double TwoMaxPhase = 2.0 * MaxPhase;
#endif

			for (size_t order = 0; order < DrumHit::NumOrders; ++order)
			{
#ifdef __AVX2__
				for (size_t zero = 0; zero < DrumHit::NumZeroes; zero += 4)
#else
				for (size_t zero = 0; zero < DrumHit::NumZeroes; ++zero)
#endif
				{
#ifdef __AVX2__
					__m256d phase = _mm256_add_pd(
						_mm256_load_pd(phases[order] + zero),
						_mm256_load_pd(dphases[order] + zero));
					phase = _mm256_sub_pd(phase, _mm256_floor_pd(phase));
					_mm256_store_pd(phases[order] + zero, phase);
#else
					double& phase = phases[order][zero];
					phase += dphases[order][zero];
					if (phase > MaxPhase)
						phase -= TwoMaxPhase;
#endif
				}
			}
		}

		void IncrementAmps(const double dt)
		{
			for (size_t order = 0; order < DrumHit::NumOrders; ++order)
			{
#ifdef __AVX2__
				for (size_t zero = 0; zero < DrumHit::NumZeroes; zero += 4)
					_mm_store_ps(amps[order] + zero, _mm_mul_ps(_mm_load_ps(amps[order] + zero), _mm_load_ps(modecay[order] + zero)));
#else
				for (size_t zero = 0; zero < DrumHit::NumZeroes; ++zero)
					amps[order][zero] *= modecay[order][zero];
#endif
			}
		}

	private:
		virtual void OnFrequencyChange(const float basefreq, const double deltaTime) override
		{
			for (size_t order = 0; order < DrumHit::NumOrders; ++order)
			{
				for (size_t zero = 0; zero < DrumHit::NumZeroes; ++zero)
				{
					const float freq = basefreq * bessel_harmonics_by_order[order][zero];
					dphases[order][zero] = static_cast<double>(freq) * deltaTime;
				}
			}
		}

		void OnHitChange()
		{
			for (size_t order = 0; order < DrumHit::NumOrders; ++order)
				for (size_t zero = 0; zero < DrumHit::NumZeroes; ++zero)
					amps[order][zero] =
						DrumHit::ModeAmp(order, zero, hit) * jn_drum(order, zero, mic) * fast::cos(zero * th);
		}

	private:
#ifdef __AVX2__
		static constexpr const size_t NumZeroesAlign = DrumHit::NumZeroes + ((32 - (DrumHit::NumZeroes & 31)) & 31);
		alignas(32) float amps[DrumHit::NumOrders][NumZeroesAlign];
		alignas(32) float modecay[DrumHit::NumOrders][NumZeroesAlign];
		alignas(32) double phases[DrumHit::NumOrders][NumZeroesAlign];
		alignas(32) double dphases[DrumHit::NumOrders][NumZeroesAlign];
#else
		Array<Array<float, DrumHit::NumZeroes>, DrumHit::NumOrders> amps;
		Array<Array<float, DrumHit::NumZeroes>, DrumHit::NumOrders> modecay;
		Array<Array<double, DrumHit::NumZeroes>, DrumHit::NumOrders> phases;
		Array<Array<double, DrumHit::NumZeroes>, DrumHit::NumOrders> dphases;
#endif
		float hit;
		float th;
		float mic;
		Ramp hitRamp;
		Ramp angRamp;
		Ramp micRamp;
		Vector<InfiniSaw::JumpMetadata> jumps;
		EInfiniSawPrecision ePrecision;

		std::uniform_real_distribution<float> rdist;

		float strenToAmp;
		double transientTime;
		ERampShape transientShape;
		double decayDelay;
		float decayAmount;
		double decayTime;
		ERampShape decayShape;
		float fundFreq;
		double detuneDelay;
		float detuneAmount;
		double detuneTime;
		ERampShape detuneShape;

		unsigned long lastSampleRate;

		bool bReentering;
		bool bFiltersActive;
		ControlSet ctrls;
		Array<SharedPtr<FiltType>, NUM_FILTS> filts;
		Array<Envelope, NUM_FILTS> envs;
		Array<float, NUM_FILTS> filtdels;
		SharedPtr<BasicAudioSum<false, false>> dumb;
	};

	void DrumHitSynthEvent::Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const
	{
		if (drumHitParam == EDrumHitSynthParam::SynthParam)
		{
			SynthEvent<DrumHitSynthEvent>::Activate(ctrl, sampleNum);
			return;
		}

		DrumHitSynth& drum = ctrl.Get<DrumHitSynth>();
		switch (drumHitParam)
		{
		case EDrumHitSynthParam::SynthParam: break; // Solely to prevent warnings as this is impossible
		case EDrumHitSynthParam::HitRadius: drum.SetHitRadius(ramp); break;
		case EDrumHitSynthParam::HitAngle: drum.SetHitAngle(ramp); break;
		case EDrumHitSynthParam::MicRadius: drum.SetMicRadius(ramp); break;
		case EDrumHitSynthParam::ResetPhase: drum.ResetPhase(); break;
		case EDrumHitSynthParam::Blep: drum.Blep(InfiniSaw::JumpMetadata(sampleNum, jump.pos, jump.amp)); break;
		case EDrumHitSynthParam::BlepPrecision: drum.BlepPrecision(ePrecision); break;
		case EDrumHitSynthParam::Hit: drum.Hit(hitStrength, sampleNum); break;
		}
	}
}

