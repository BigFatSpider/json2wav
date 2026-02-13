// Copyright Dan Price 2026.

#pragma once

#include "SineSynth.h"
#include "Synth.h"
#include "Ramp.h"
#include "FastSin.h"
#include "Random.h"
#include "Filter.h"
#include "Envelope.h"
#include "Utility.h"
#include "Memory.h"
#include <utility>

namespace json2wav
{
	struct AdditiveHitSynthEvent : public SynthEvent<AdditiveHitSynthEvent>
	{
	public:
		template<typename... RampArgTypes>
		AdditiveHitSynthEvent(const ESynthParam param_init, RampArgTypes&&... rampArgs)
			: SynthEvent<AdditiveHitSynthEvent>(param_init, std::forward<RampArgTypes>(rampArgs)...)
			, hitStrength(0.0f)
		{
		}

		AdditiveHitSynthEvent(const float hitStrengthInit, const float durDummy = 0.0f)
			: SynthEvent<AdditiveHitSynthEvent>(static_cast<ESynthParam>(-1))
			, hitStrength(hitStrengthInit)
		{
		}

		virtual void Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const override;

		const float hitStrength;
	};

	class AdditiveHitSynth : public SynthWithCustomEvent<AdditiveHitSynthEvent>
	{
	public:
		using FiltType = Filter::BiquadPeak<false, 1, Filter::ETopo::DF2>;

		static constexpr const size_t NUM_FILTS = 4;

	public:
		AdditiveHitSynth(const float frequency_init = 100.0f
			, const bool bActivateFilters = true)
			: SynthWithCustomEvent<AdditiveHitSynthEvent>(frequency_init, 0.0f, 0.0f)
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
			, bModesUnlocked(true)
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
			bModesUnlocked = false;
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
							for (size_t idx = 0; idx < amps.size(); ++idx)
								smp += amp * amps[idx] * fast::cos(phases[idx] * vTau<double>::value);
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

		void AddMode(const float freq, const float amp)
		{
			if (bModesUnlocked)
			{
				amps.push_back(amp);
				freqs.push_back(freq);
				phases.push_back(0.0);
				dphases.push_back(0.0);
			}
		}

		void PopMode()
		{
			if (bModesUnlocked && amps.size() > 0)
			{
				amps.pop_back();
				freqs.pop_back();
				phases.pop_back();
				dphases.pop_back();
			}
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
			static_assert(filtidx < NUM_FILTS, "AdditiveHitSynth::SetFilt<filtidx>(): filtidx must be less than 4.");

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
			static_assert(envidx < NUM_FILTS, "AdditiveHitSynth::SetEnvelope<envidx>(): envidx must be less than 4.");
			envs[envidx] = Envelope(std::forward<ParamTypes>(params)...);
		}

		template<size_t envidx>
		void SetEnvAttack(const float value)
		{
			static_assert(envidx < NUM_FILTS, "AdditiveHitSynth::SetEnvAttack<envidx>(): envidx must be less than 4.");
			envs[envidx].attack = value;
		}

		template<size_t envidx>
		void SetEnvDecay(const float value)
		{
			static_assert(envidx < NUM_FILTS, "AdditiveHitSynth::SetEnvDecay<envidx>(): envidx must be less than 4.");
			envs[envidx].decay = value;
		}

		template<size_t envidx>
		void SetEnvRelease(const float value)
		{
			static_assert(envidx < NUM_FILTS, "AdditiveHitSynth::SetEnvRelease<envidx>(): envidx must be less than 4.");
			envs[envidx].release = value;
		}

		template<size_t envidx>
		void SetEnvAttLevel(const float value)
		{
			static_assert(envidx < NUM_FILTS, "AdditiveHitSynth::SetEnvAttLevel<envidx>(): envidx must be less than 4.");
			envs[envidx].attlevel = value;
		}

		template<size_t envidx>
		void SetEnvSusLevel(const float value)
		{
			static_assert(envidx < NUM_FILTS, "AdditiveHitSynth::SetEnvSusLevel<envidx>(): envidx must be less than 4.");
			envs[envidx].suslevel = value;
		}

		template<size_t envidx>
		void SetEnvAttShape(const ERampShape value)
		{
			static_assert(envidx < NUM_FILTS, "AdditiveHitSynth::SetEnvAttShape<envidx>(): envidx must be less than 4.");
			envs[envidx].attramp = value;
		}

		template<size_t envidx>
		void SetEnvDecShape(const ERampShape value)
		{
			static_assert(envidx < NUM_FILTS, "AdditiveHitSynth::SetEnvDecShape<envidx>(): envidx must be less than 4.");
			envs[envidx].decramp = value;
		}

		template<size_t envidx>
		void SetEnvRelShape(const ERampShape value)
		{
			static_assert(envidx < NUM_FILTS, "AdditiveHitSynth::SetEnvRelShape<envidx>(): envidx must be less than 4.");
			envs[envidx].relramp = value;
		}

		template<size_t filtidx>
		void SetFiltDelay(const float delay)
		{
			static_assert(filtidx < NUM_FILTS, "AdditiveHitSynth::SetFiltDelay<filtidx>(): filtidx must be less than 4.");
			filtdels[filtidx] = delay;
		}

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
			for (auto& phase : phases)
				phase = 0.0;
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
			const float amp = GetAmplitude();
			float oldamp = 0.0f;
			for (size_t idx = 0; idx < amps.size(); ++idx)
				oldamp += amp * amps[idx] * fast::cos(phases[idx] * vTau<double>::value);

			ResetPhase();

			OnHitChange();

			float newamp = 0.0f;
			for (size_t idx = 0; idx < amps.size(); ++idx)
				newamp += amps[idx] * fast::cos(phases[idx] * vTau<double>::value);

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
					const Vector<SharedPtr<IEvent>>& smpevts = static_cast<const AdditiveHitSynth*>(this)->GetEvents(smpnum);
					for (size_t i = 0, num = smpevts.size(); i < num; ++i)
					{
						const AdditiveHitSynthEvent& evt = static_cast<const AdditiveHitSynthEvent&>(*smpevts[i]);
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
			return bIncAmp;
		}

		void IncrementPhases(const double dt)
		{
			static constexpr const double MaxPhase = 1.0;
			static constexpr const double TwoMaxPhase = 2.0 * MaxPhase;

			for (size_t idx = 0; idx < amps.size(); ++idx)
			{
				double& phase = phases[idx];
				phase += dphases[idx];
				if (phase > MaxPhase)
					phase -= TwoMaxPhase;
			}
		}

	private:
		virtual void OnFrequencyChange(const float basefreq, const double deltaTime) override
		{
			for (size_t idx = 0; idx < amps.size(); ++idx)
			{
				const float freq = basefreq * freqs[idx];
				dphases[idx] = static_cast<double>(freq) * deltaTime;
			}
		}

		void OnHitChange()
		{
			//for (size_t order = 0; order < DrumHit::NumOrders; ++order)
			//	for (size_t zero = 0; zero < DrumHit::NumZeroes; ++zero)
			//		amps[order][zero] =
			//			DrumHit::ModeAmp(order, zero, hit) * jn_drum(order, zero, mic) * fast::cos(zero * th);
		}

	private:
		Vector<float> amps;
		Vector<float> freqs;
		Vector<double> phases;
		Vector<double> dphases;
		Vector<InfiniSaw::JumpMetadata> jumps;
		EInfiniSawPrecision ePrecision;

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
		bool bModesUnlocked;
		ControlSet ctrls;
		Array<SharedPtr<FiltType>, NUM_FILTS> filts;
		Array<Envelope, NUM_FILTS> envs;
		Array<float, NUM_FILTS> filtdels;
		SharedPtr<BasicAudioSum<false, false>> dumb;
	};

	void AdditiveHitSynthEvent::Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const
	{
		if (hitStrength == 0.0f)
		{
			SynthEvent<AdditiveHitSynthEvent>::Activate(ctrl, sampleNum);
			return;
		}

		AdditiveHitSynth& synth = ctrl.Get<AdditiveHitSynth>();
		synth.Hit(hitStrength, sampleNum);
	}
}

