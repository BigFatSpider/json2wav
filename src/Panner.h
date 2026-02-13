// Copyright Dan Price 2026.

#pragma once

#include "IAudioObject.h"
#include "IControlObject.h"
#include "Ramp.h"
#include "FastSin.h"
#include <utility>
#include <cstdint>

namespace json2wav
{
	enum class EPannerParam
	{
		Pan
	};

	template<bool bOwner>
	class PannerEvent : public IEvent
	{
	public:
		template<typename... RampArgTypes>
		PannerEvent(const EPannerParam param_init, RampArgTypes&&... rampargs)
			: param(param_init), ramp(std::forward<RampArgTypes>(rampargs)...)
		{
		}

		virtual void Activate(ControlObjectHolder& ctrl, const size_t samplenum) const override;

	private:
		const EPannerParam param;
		const Ramp ramp;
	};

	enum class EPanLaw : uint8_t
	{
		Linear3dB, Linear6dB//, Linear4_5dB, Circular3dB, Circular4_5dB, Circular6dB
	};

	inline float GetPanVolume(const EPanLaw panlaw, const float pan)
	{
		const float panNormalized = (pan + 1.0f) * 0.5f;
		switch (panlaw)
		{
		case EPanLaw::Linear6dB: return panNormalized;
		default:
		case EPanLaw::Linear3dB: return FastSin<6>(QuarterTau<float>() * panNormalized);
		}
	}

	template<bool bOwner = false>
	class Panner : public AudioSum<bOwner>, public ControlObject<PannerEvent<bOwner>>
	{
	public:
		Panner(const float panInit = 0.0f, const EPanLaw panLawInit = EPanLaw::Linear3dB)
			: panlaw(panLawInit), pan(panInit)
		{
		}

		float GetLeftPanVolume() const noexcept
		{
			return GetPanVolume(panlaw, -pan);
		}

		float GetRightPanVolume() const noexcept
		{
			return GetPanVolume(panlaw, pan);
		}

		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t numSamples,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			this->GetInputSamples(bufs, numChannels, numSamples, sampleRate);
			if (numChannels != 2)
			{
				this->IncrementSampleNum(numSamples);
				return;
			}

			const double deltaTime = 1.0 / static_cast<double>(sampleRate);
			this->ProcessEvents(numSamples, [this, bufs, deltaTime](const size_t i)
				{
					pan_ramp.Increment(pan, deltaTime);
					bufs[0][i] *= GetLeftPanVolume();
					bufs[1][i] *= GetRightPanVolume();
				});
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return 2;
		}

		EPanLaw GetPanLaw() const noexcept
		{
			return panlaw;
		}

		void SetPanLaw(const EPanLaw newpanlaw) noexcept
		{
			panlaw = newpanlaw;
		}

		float GetPan() const noexcept
		{
			return pan;
		}

		void SetPan(const Ramp& ramp)
		{
			pan_ramp = ramp;
		}

	private:
		EPanLaw panlaw;
		float pan;
		Ramp pan_ramp;
	};

	template<bool bOwner>
	void PannerEvent<bOwner>::Activate(ControlObjectHolder& ctrl, const size_t samplenum) const
	{
		Panner<bOwner>& panner = ctrl.Get<Panner<bOwner>>();
		switch (param)
		{
		default:
		case EPannerParam::Pan:
			panner.SetPan(ramp);
			break;
		}
	}
}

