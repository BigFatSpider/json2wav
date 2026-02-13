// Copyright Dan Price. All rights reserved.

#pragma once

#include "IAudioObject.h"
#include "IControlObject.h"
#include "Memory.h"
#include <utility>

namespace json2wav
{

	struct CompSynthEventParams
	{
		float freq;
		float amp;
		float dur;
		unsigned long sampleRate;
	};

	struct CompSynthEventParams_SmpDur
	{
		float freq;
		float amp;
		unsigned long smpdur;
	};

	class CompSynthEvent : public IEvent
	{
	public:
		virtual void Activate(ControlObjectHolder& ctrl, const size_t samplenum) const override
		{
			// Do nothing; active events are on the contained synths and effects
		}
	};

	class IComposable
	{
	public:
		virtual ~IComposable() noexcept {}
		virtual void AddCompSynthEvent(const size_t samplenum, const CompSynthEventParams& params) = 0;
		virtual void AddCompSynthEvent(const size_t samplenum, const CompSynthEventParams_SmpDur& params) = 0;
		virtual float GetRelease() const = 0;
	};

	template<bool bOwner = false>
	class CompositeSynth_SpecifyIsOwner : public IAudioObject, public ControlObject<CompSynthEvent, CompositeSynth_SpecifyIsOwner<bOwner>>
	{
		friend class ControlObject<CompSynthEvent, CompositeSynth_SpecifyIsOwner<bOwner>>;

	public:

		template<typename SynthType, typename... CtorParamTypes>
		std::pair<SharedPtr<SynthType>, size_t> AddSynthNoRouting(CtorParamTypes&&... params)
		{
			std::pair<SharedPtr<SynthType>, size_t> ctrlpair(ctrls.CreatePair<SynthType>(std::forward<CtorParamTypes>(params)...));
			procs.push_back(ctrlpair.first);
			synths.push_back(ctrlpair.first);
			return ctrlpair;
		}

		template<typename SynthType, typename... CtorParamTypes>
		SharedPtr<SynthType> AddSynthPtrNoRouting(CtorParamTypes&&... params)
		{
			SharedPtr<SynthType> synth(ctrls.CreatePtr<SynthType>(std::forward<CtorParamTypes>(params)...));
			procs.push_back(synth);
			synths.push_back(synth);
			return synth;
		}

		template<typename SynthType, typename... CtorParamTypes>
		std::pair<SharedPtr<SynthType>, size_t> AddSynth(CtorParamTypes&&... params)
		{
			std::pair<SharedPtr<SynthType>, size_t> ctrlpair(AddSynthNoRouting<SynthType>(std::forward<CtorParamTypes>(params)...));
			if (effects.size() > 0)
				effects[0]->AddInput(ctrlpair.first);
			return ctrlpair;
		}

		template<typename SynthType, typename... CtorParamTypes>
		SharedPtr<SynthType> AddSynthPtr(CtorParamTypes&&... params)
		{
			SharedPtr<SynthType> synth(AddSynthPtrNoRouting<SynthType>(std::forward<CtorParamTypes>(params)...));
			if (effects.size() > 0)
				effects[0]->AddInput(synth);
			return synth;
		}

		template<typename EffectType, typename... CtorParamTypes>
		SharedPtr<EffectType> AddEffectNoRouting(CtorParamTypes&&... params)
		{
			static_assert(bOwner == EffectType::bIsOwner, "All effects added to CompositeSynth<bOwner> must match bOwner");
			SharedPtr<EffectType> effect(MakeShared<EffectType>(std::forward<CtorParamTypes>(params)...));
			effects.push_back(effect);
			return effect;
		}

		template<typename EffectType, typename... CtorParamTypes>
		SharedPtr<EffectType> AddEffect(CtorParamTypes&&... params)
		{
			SharedPtr<AudioSum<bOwner>> lastEffect;
			if (effects.size() > 0)
				lastEffect = effects.back();
			SharedPtr<EffectType> effect(AddEffectNoRouting<EffectType>(std::forward<CtorParamTypes>(params)...));
			if (lastEffect)
			{
				effect->AddInput(lastEffect);
			}
			else
			{
				for (const SharedPtr<IAudioObject>& synth : synths)
				{
					effect->AddInput(synth);
				}
			}
			return effect;
		}

		template<typename EffectType, typename... CtorParamTypes>
		std::pair<SharedPtr<EffectType>, size_t> AddCtrlEffectNoRouting(CtorParamTypes&&... params)
		{
			static_assert(bOwner == EffectType::bIsOwner, "All effects added to CompositeSynth<bOwner> must match bOwner");
			std::pair<SharedPtr<EffectType>, size_t> ctrlpair(ctrls.CreatePair<EffectType>(std::forward<CtorParamTypes>(params)...));
			effects.push_back(ctrlpair.first);
			return ctrlpair;
		}

		template<typename EffectType, typename... CtorParamTypes>
		SharedPtr<EffectType> AddCtrlEffectPtrNoRouting(CtorParamTypes&&... params)
		{
			static_assert(bOwner == EffectType::bIsOwner, "All effects added to CompositeSynth<bOwner> must match bOwner");
			SharedPtr<EffectType> effect(ctrls.CreatePtr<EffectType>(std::forward<CtorParamTypes>(params)...));
			effects.push_back(effect);
			return effect;
		}

		template<typename EffectType, typename... CtorParamTypes>
		std::pair<SharedPtr<EffectType>, size_t> AddCtrlEffect(CtorParamTypes&&... params)
		{
			SharedPtr<AudioSum<bOwner>> lastEffect;
			if (effects.size() > 0)
				lastEffect = effects.back();
			std::pair<SharedPtr<EffectType>, size_t> ctrlpair(AddCtrlEffectNoRouting<EffectType>(std::forward<CtorParamTypes>(params)...));
			if (lastEffect)
			{
				ctrlpair.first->AddInput(lastEffect);
			}
			else
			{
				for (const SharedPtr<IAudioObject>& synth : synths)
				{
					ctrlpair.first->AddInput(synth);
				}
			}
			return ctrlpair;
		}

		template<typename EffectType, typename... CtorParamTypes>
		SharedPtr<EffectType> AddCtrlEffectPtr(CtorParamTypes&&... params)
		{
			SharedPtr<AudioSum<bOwner>> lastEffect;
			if (effects.size() > 0)
				lastEffect = effects.back();
			SharedPtr<EffectType> effect(AddCtrlEffectPtrNoRouting<EffectType>(std::forward<CtorParamTypes>(params)...));
			if (lastEffect)
			{
				effect->AddInput(lastEffect);
			}
			else
			{
				for (const SharedPtr<IAudioObject>& synth : synths)
				{
					effect->AddInput(synth);
				}
			}
			return effect;
		}

		template<typename EffectType, typename... CtorParamTypes>
		std::pair<SharedPtr<EffectType>, size_t> AddEnvEffectNoRouting(CtorParamTypes&&... params)
		{
			static_assert(bOwner == EffectType::bIsOwner, "All effects added to CompositeSynth<bOwner> must match bOwner");
			std::pair<SharedPtr<EffectType>, size_t> ctrlpair(ctrls.CreatePair<EffectType>(std::forward<CtorParamTypes>(params)...));
			procs.push_back(ctrlpair.first);
			effects.push_back(ctrlpair.first);
			return ctrlpair;
		}

		template<typename EffectType, typename... CtorParamTypes>
		SharedPtr<EffectType> AddEnvEffectPtrNoRouting(CtorParamTypes&&... params)
		{
			static_assert(bOwner == EffectType::bIsOwner, "All effects added to CompositeSynth<bOwner> must match bOwner");
			SharedPtr<EffectType> effect(ctrls.CreatePtr<EffectType>(std::forward<CtorParamTypes>(params)...));
			procs.push_back(effect);
			effects.push_back(effect);
			return effect;
		}

		template<typename EffectType, typename... CtorParamTypes>
		std::pair<SharedPtr<EffectType>, size_t> AddEnvEffect(CtorParamTypes&&... params)
		{
			SharedPtr<AudioSum<bOwner>> lastEffect;
			if (effects.size() > 0)
				lastEffect = effects.back();
			std::pair<SharedPtr<EffectType>, size_t> ctrlpair(AddEnvEffectNoRouting<EffectType>(std::forward<CtorParamTypes>(params)...));
			if (lastEffect)
			{
				ctrlpair.first->AddInput(lastEffect);
			}
			else
			{
				for (const SharedPtr<IAudioObject>& synth : synths)
				{
					ctrlpair.first->AddInput(synth);
				}
			}
			return ctrlpair;
		}

		template<typename EffectType, typename... CtorParamTypes>
		SharedPtr<EffectType> AddEnvEffectPtr(CtorParamTypes&&... params)
		{
			SharedPtr<AudioSum<bOwner>> lastEffect;
			if (effects.size() > 0)
				lastEffect = effects.back();
			SharedPtr<EffectType> effect(AddEnvEffectPtrNoRouting<EffectType>(std::forward<CtorParamTypes>(params)...));
			if (lastEffect)
			{
				effect->AddInput(lastEffect);
			}
			else
			{
				for (const SharedPtr<IAudioObject>& synth : synths)
				{
					effect->AddInput(synth);
				}
			}
			return effect;
		}

		float GetRelease() const
		{
			float release(0.0f);
			for (const SharedPtr<IComposable>& proc : procs)
				if (const float procrelease = proc->GetRelease(); procrelease > release)
					release = procrelease;
			return release;
		}

		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			if (effects.empty())
			{
				switch (synths.size())
				{
				default: AddEffect<BasicAudioSum<bOwner>>(); break;
				case 1: synths[0]->GetSamples(bufs, numChannels, bufSize, sampleRate, requester);
				case 0: return;
				}
			}
			effects.back()->GetSamples(bufs, numChannels, bufSize, sampleRate, requester);
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			if (effects.size() > 0)
				return effects.back()->GetNumChannels();
			size_t maxnum(0);
			for (const SharedPtr<IAudioObject>& synth : synths)
			{
				const size_t num(synth->GetNumChannels());
				if (num > maxnum)
				{
					maxnum = num;
				}
			}
			return maxnum;
		}

		ControlSet& GetControls() noexcept { return ctrls; }
		const ControlSet& GetControls() const noexcept { return ctrls; }

	private:
		template<typename T>
		bool AddEventInternal(const size_t samplenum, const CompSynthEventParams& params)
		{
			bool bAddedAny(false);
			for (const SharedPtr<IComposable>& proc : procs)
			{
				proc->AddCompSynthEvent(samplenum, params);
				bAddedAny = true;
			}
			return bAddedAny;
		}

		template<typename T>
		bool AddEventInternal(const size_t samplenum, const CompSynthEventParams_SmpDur& params)
		{
			bool bAddedAny(false);
			for (const SharedPtr<IComposable>& proc : procs)
			{
				proc->AddCompSynthEvent(samplenum, params);
				bAddedAny = true;
			}
			return bAddedAny;
		}

	private:
		ControlSet ctrls;
		Vector<SharedPtr<IComposable>> procs;
		Vector<SharedPtr<IAudioObject>> synths;
		Vector<SharedPtr<AudioSum<bOwner>>> effects;
	};

	typedef CompositeSynth_SpecifyIsOwner<false> CompositeSynth;
	typedef CompositeSynth_SpecifyIsOwner<true> CompositeSynthOwner;
}

