// Copyright Dan Price 2026.

#pragma once

#include "Instrument.h"
#include "Ramp.h"
#include <utility>
#include <cmath>

namespace json2wav
{
	enum class ESynthParam
	{
		Frequency, Amplitude, Phase
	};

	template<typename SynthEventType>
	struct SynthEvent : public IEvent
	{
		template<typename... RampArgTypes>
		static Ramp ConstructRamp(const ESynthParam param, RampArgTypes&&... rampargs)
		{
			if (param == ESynthParam::Phase)
			{
				return Ramp();
			}
			return Ramp(std::forward<RampArgTypes>(rampargs)...);
		}
		template<typename... RampArgTypes>
		static PreciseRamp ConstructPhaseRamp(const ESynthParam param, RampArgTypes&&... rampargs)
		{
			if (param == ESynthParam::Phase)
			{
				return PreciseRamp(std::forward<RampArgTypes>(rampargs)...);
			}
			return PreciseRamp();
		}
		template<typename... RampArgTypes>
		SynthEvent(const ESynthParam param_init, RampArgTypes&&... rampargs)
			: param(param_init)
			, ramp(ConstructRamp(param_init, std::forward<RampArgTypes>(rampargs)...))
			, phase_ramp(ConstructPhaseRamp(param_init, std::forward<RampArgTypes>(rampargs)...))
		{
		}

		virtual void Activate(ControlObjectHolder& ctrl, const size_t samplenum) const override;

		const ESynthParam param;
		const Ramp ramp;
		const PreciseRamp phase_ramp;
	};

	struct BasicSynthEvent final : SynthEvent<BasicSynthEvent>
	{
		template<typename... ParamTypes>
		BasicSynthEvent(ParamTypes&&... params)
			: SynthEvent<BasicSynthEvent>(std::forward<ParamTypes>(params)...)
		{
		}
	};

	template<typename EventType>
	class SynthWithCustomEvent : public Instrument<EventType>
	{
	public:
		SynthWithCustomEvent(const float frequency_init = 1000.0f
			, const float amplitude_init = 0.5f
			, const double phase_init = 0.0)
			: frequency(frequency_init)
			, amplitude(amplitude_init)
			, phase(phase_init)
			, phaseoffset(0.0)
			, deltaphase_cached(0.0)
			//, deltatime_cached(0.0)
			, frequency_ramp(frequency_init, 0.0)
			, amplitude_ramp(amplitude_init, 0.0)
			, phase_ramp(phase_init, 0.0)
		{
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return 1;
		}

		void SetFrequency(const Ramp& ramp)
		{
			frequency_ramp = ramp;
		}

		void SetFrequency(const float freq)
		{
			frequency = freq;
		}

		void SetAmplitude(const Ramp& ramp)
		{
			amplitude_ramp = ramp;
		}

		void SetAmplitude(const float amp)
		{
			amplitude = amp;
		}

		void SetPhase(const PreciseRamp& ramp)
		{
			phase_ramp = ramp;
		}

		float GetFrequency() const noexcept
		{
			return frequency;
		}

		float GetAmplitude() const noexcept
		{
			return amplitude;
		}

		double GetPhase() const noexcept
		{
			return phaseoffset;
		}

		double GetInstantaneousPhase() const noexcept
		{
			const double instphase(phaseoffset + phase);
			return (phase - std::floor(instphase)) + phaseoffset;
		}

	protected:
		void Increment(const double deltaTime)
		{
#if 0
			static constexpr const double MaxPhase = 1.0;
			static constexpr const double TwoMaxPhase = 2.0 * MaxPhase;
#endif

			//if (deltatime_cached != deltaTime)
			//{
			//	deltatime_cached = deltaTime;
			//}

			//if (frequency_ramp.Increment(frequency, deltatime_cached))
			if (frequency_ramp.Increment(frequency, deltaTime))
			{
				//deltaphase_cached = frequency * deltatime_cached;
				deltaphase_cached = frequency * deltaTime;
				OnFrequencyChange(frequency, deltaTime);
			}
			const auto nextphase(phase + deltaphase_cached);
			phase = (phase - std::floor(nextphase)) + deltaphase_cached;

			//amplitude_ramp.Increment(amplitude, deltatime_cached);
			if (amplitude_ramp.Increment(amplitude, deltaTime))
			{
				OnAmplitudeChange(amplitude, deltaTime);
			}

			//if (phase_ramp.Increment(phaseoffset, deltatime_cached))
			if (phase_ramp.Increment(phaseoffset, deltaTime))
			{
#if 0
				if (phaseoffset > MaxPhase)
					phaseoffset -= TwoMaxPhase;
				else if (phaseoffset < MaxPhase)
					phaseoffset += TwoMaxPhase;
#else
				phaseoffset -= std::floor(phaseoffset);
#endif
				OnPhaseOffsetChange(phaseoffset, deltaTime);
			}

#if 0
			if (phase > MaxPhase)
				phase -= TwoMaxPhase;
#endif
		}

		template<typename ProcSampFunc>
		void GetSynthSamples(Sample* const* const bufs, const size_t numChannels, const size_t numSamples,
			const bool bCopyFirstChannel, ProcSampFunc&& ProcessSample) noexcept
		{
			if (numChannels == 0)
			{
				return;
			}

			this->ProcessEvents(numSamples, std::forward<ProcSampFunc>(ProcessSample));

			if (bCopyFirstChannel)
			{
				Sample* const buf = bufs[0];
				for (size_t ch = 1; ch < numChannels; ++ch)
				{
					Sample* const chbuf = bufs[ch];
					for (size_t i = 0; i < numSamples; ++i)
					{
						chbuf[i] = buf[i];
					}
				}
			}
		}

	private:
		virtual void OnFrequencyChange(const float freq, const double deltaTime) {}
		virtual void OnAmplitudeChange(const float amp, const double deltaTime) {}
		virtual void OnPhaseOffsetChange(const double phoffset, const double deltaTime) {}

	private:
		float frequency;
		float amplitude;
		double phase;
		double phaseoffset;
		double deltaphase_cached;
		//double deltatime_cached;
		Ramp frequency_ramp;
		Ramp amplitude_ramp;
		PreciseRamp phase_ramp;
	};

	template<typename SynthEventType>
	inline void SynthEvent<SynthEventType>::Activate(ControlObjectHolder& ctrl, const size_t samplenum) const
	{
		SynthWithCustomEvent<SynthEventType>& synth = ctrl.Get<SynthWithCustomEvent<SynthEventType>>();
		switch (param)
		{
		case ESynthParam::Frequency:
			synth.SetFrequency(ramp);
			break;
		case ESynthParam::Amplitude:
			synth.SetAmplitude(ramp);
			break;
		case ESynthParam::Phase:
			synth.SetPhase(phase_ramp);
			break;
		default:
			break;
		}
	}

	using BasicSynth = SynthWithCustomEvent<BasicSynthEvent>;
}

