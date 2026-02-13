// Copyright Dan Price 2026.

#pragma once

#include "InfiniSawComposable.h"
#include "NoiseSynthComposable.h"
#include "FilterComposable.h"
#include "Panner.h"
#include "CompositeSynth.h"
#include "Envelope.h"
#include "Random.h"
#include "Memory.h"
#include <utility>
#include <cmath>

namespace json2wav
{
	template<Filter::ETopo eTopo, bool bWithSaw = true, typename SawType = InfiniSawComposable>
	inline SharedPtr<CompositeSynth> CreateFilteredSaw(
		ControlSet& ctrls,
		const size_t unison,
		const float freqSpreadCents,
		const double phaseSpread,
		const float panSpread,
		const float noiseAmp,
		const Envelope& ampEnvHi,
		const Envelope& ampEnvLo,
		const Envelope& filtEnv,
		Vector<SharedPtr<SawType>>* const pOutSaws = nullptr,
		Vector<SharedPtr<NoiseSynthComposable>>* const pOutNoises = nullptr,
		Vector<SharedPtr<Panner<>>>* const pOutPans = nullptr,
		SharedPtr<LadderLPComposable<false, 2, eTopo>>* const pOutFilt = nullptr)
	{
		SharedPtr<CompositeSynth> compSynth;

		if (unison > 0)
		{
			Vector<SharedPtr<SawType>> outSaws;
			Vector<SharedPtr<NoiseSynthComposable>> outNoises;
			Vector<SharedPtr<Panner<>>> outPans;
			SharedPtr<LadderLPComposable<false, 2, eTopo>> outFilt;

			if constexpr (bWithSaw)
				outSaws.reserve(unison);
			outNoises.reserve(unison);
			outPans.reserve(unison);

			compSynth = ctrls.CreatePtr<CompositeSynth>();

			Vector<float> detunes;
			if constexpr (bWithSaw)
			{
				detunes.reserve(unison);
				if (unison > 1)
				{
					const float detuneLo(-0.5f*freqSpreadCents);
					const float detuneInt(freqSpreadCents/(static_cast<float>(unison - 1)));

					/*for (size_t i = 0; i < unison; ++i)
						detunes.push_back(detuneLo + i*detuneInt);

					// Swap every other with its mirror from the end to prevent pitch "sagging" to one side
					for (size_t i = 1, end = unison / 2; i < end; i += 2)
					{
						const float tmp(detunes[i]);
						detunes[i] = detunes[unison - 1 - i];
						detunes[unison - 1 - i] = tmp;
					}*/

					RNG r(detuneLo, (unison - 1)*detuneInt);
					for (size_t i = 0; i < unison; ++i)
						detunes.push_back(r());
				}
				else
				{
					detunes.push_back(0.0f);
				}
			}

			Vector<double> phases;
			if constexpr (bWithSaw)
			{
				phases.reserve(unison);
				if (unison > 1)
				{
					const double phaseLo(0.5 - 0.5*phaseSpread);
					const double phaseInt(phaseSpread/(static_cast<double>(unison - 1)));
					//for (size_t i = 0; i < unison; ++i)
					//	phases.push_back(phaseLo + i*phaseInt);

					RNG64 r(phaseLo, (unison - 1)*phaseInt);
					for (size_t i = 0; i < unison; ++i)
						phases.push_back(r());
				}
				else
				{
					phases.push_back(0.5);
				}
			}

			Vector<float> pans;
			pans.reserve(unison);
			if (unison > 1)
			{
				const float panLo(-panSpread);
				const float panInt(2.0f*panSpread/(static_cast<float>(unison - 1)));
				for (size_t i = 0; i < unison; ++i)
					pans.push_back(panLo + i*panInt);
			}
			else
			{
				pans.push_back(0.0f);
			}

			static constexpr const float centsTo8ve(1.0f/1200.0f);

			const Envelope noiseEnvLo(
				ampEnvLo.attack, ampEnvLo.decay, ampEnvLo.length, ampEnvLo.release,
				noiseAmp*ampEnvLo.attlevel, noiseAmp*ampEnvLo.suslevel,
				ampEnvLo.attramp, ampEnvLo.decramp, ampEnvLo.relramp, ampEnvLo.expression);
			const Envelope noiseEnvHi(
				ampEnvHi.attack, ampEnvHi.decay, ampEnvHi.length, ampEnvHi.release,
				noiseAmp*ampEnvHi.attlevel, noiseAmp*ampEnvHi.suslevel,
				ampEnvHi.attramp, ampEnvHi.decramp, ampEnvHi.relramp, ampEnvHi.expression);

			for (size_t i = 0; i < unison; ++i)
			{
				if constexpr (bWithSaw)
				{
					//Vector<InfiniSaw::Jump> jumps{ InfiniSaw::Jump(phases[i], 1.0f) };
					//outSaws.emplace_back(compSynth->AddSynthPtrNoRouting<SawType>(ampEnvLo, std::move(jumps), 0.0f, 0.0f));
					outSaws.emplace_back(compSynth->AddSynthPtrNoRouting<SawType>(ampEnvLo, 154.0f, 0.0f, phases[i]));
					outSaws[i]->SetDetuneFactor(std::pow(2.0f, detunes[i]*centsTo8ve));
				}
				outNoises.emplace_back(compSynth->AddSynthPtrNoRouting<NoiseSynthComposable>(noiseEnvLo, 0.0f));
				outPans.emplace_back(compSynth->AddCtrlEffectPtrNoRouting<Panner<>>(pans[i]));
				if constexpr (bWithSaw)
					outPans[i]->AddInput(outSaws[i]);
				outPans[i]->AddInput(outNoises[i]);
			}

			const size_t halfUnison(unison >> 1);
			if constexpr (bWithSaw)
				outSaws[halfUnison]->SetEnvelope(ampEnvHi);
			outNoises[halfUnison]->SetEnvelope(noiseEnvHi);
			if ((unison & 1) == 0)
			{
				if constexpr (bWithSaw)
					outSaws[halfUnison - 1]->SetEnvelope(ampEnvHi);
				outNoises[halfUnison - 1]->SetEnvelope(noiseEnvHi);
			}

			outFilt = compSynth->AddEnvEffectPtrNoRouting<LadderLPComposable<false, 2, eTopo>>(filtEnv, 1.0f, 0.5f);
			for (size_t i = 0; i < unison; ++i)
				outFilt->AddInput(outPans[i]);

			if constexpr (bWithSaw)
				if (pOutSaws)
					*pOutSaws = std::move(outSaws);
			if (pOutNoises)
				*pOutNoises = std::move(outNoises);
			if (pOutPans)
				*pOutPans = std::move(outPans);
			if (pOutFilt)
				*pOutFilt = std::move(outFilt);
		}

		return compSynth;
	}

	inline SharedPtr<CompositeSynth> CreateFatSaw0(ControlSet& ctrls)
	{
		SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::DF2>> filt;
		SharedPtr<CompositeSynth> synth(CreateFilteredSaw<Filter::ETopo::DF2, true, InfiniSawComposable>(ctrls, 7, 10.0f, 1.0, 1.0f, 0.05f, Envelope(0.005f, 0.01f, 0.25f, 0.7f, 0.5f, ERampShape::SCurve), Envelope(0.005f, 0.01f, 0.25f, 0.7f, 0.5f, ERampShape::SCurve), Envelope(0.1f, 0.01f, 0.05f, 5000.0f, 5000.0f, ERampShape::LogScaleSCurve), nullptr, nullptr, nullptr, &filt));
		if (synth)
			filt->SetResetVal(50.0f);
		return synth;
	}

	inline SharedPtr<CompositeSynth> CreateFatSaw1(ControlSet& ctrls)
	{
		SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::TDF2>> filt;
		SharedPtr<CompositeSynth> synth(CreateFilteredSaw<Filter::ETopo::TDF2, true, InfiniSawComposable>(ctrls, 7, 10.0f, 1.0, 1.0f, 0.05f, Envelope(0.005f, 0.01f, 0.25f, 0.7f, 0.5f, ERampShape::SCurve), Envelope(0.005f, 0.01f, 0.25f, 0.7f, 0.5f, ERampShape::SCurve), Envelope(0.7f, 0.01f, 0.05f, 1200.0f, 1200.0f, ERampShape::LogScaleSCurve), nullptr, nullptr, nullptr, &filt));
		if (synth)
			filt->SetResetVal(5000.0f);
		return synth;
	}

	inline SharedPtr<CompositeSynth> CreateSolidSaw0(ControlSet& ctrls)
	{
		SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::TDF2>> filt;
		SharedPtr<CompositeSynth> synth(CreateFilteredSaw<Filter::ETopo::TDF2, true, InfiniSawComposable>(ctrls, 3, 0.0f, 1.0, 1.0f, 0.05f, Envelope(0.002f, 0.7f, 0.01f, 0.85f, 0.5f, ERampShape::SCurve), Envelope(0.002f, 0.7f, 0.01f, 0.85f, 0.5f, ERampShape::SCurve), Envelope(0.7f, 0.01f, 0.05f, 2000.0f, 2000.0f, ERampShape::LogScaleSCurve), nullptr, nullptr, nullptr, &filt));
		if (synth)
			filt->SetResetVal(10000.0f);
		return synth;
	}

	inline SharedPtr<CompositeSynth> CreateSolidSaw1(ControlSet& ctrls)
	{
		SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::TDF2>> filt;
		SharedPtr<CompositeSynth> synth(CreateFilteredSaw<Filter::ETopo::TDF2, true, InfiniSawComposable>(ctrls, 3, 0.0f, 1.0, 1.0f, 0.05f, Envelope(0.002f, 0.7f, 0.01f, 0.85f, 0.85f, ERampShape::SCurve), Envelope(0.002f, 0.7f, 0.01f, 0.85f, 0.85f, ERampShape::SCurve), Envelope(0.7f, 0.01f, 0.05f, 2000.0f, 2000.0f, ERampShape::LogScaleSCurve), nullptr, nullptr, nullptr, &filt));
		if (synth)
			filt->SetResetVal(10000.0f);
		return synth;
	}

	inline SharedPtr<CompositeSynth> CreateFilteredNoise(ControlSet& ctrls)
	{
		SharedPtr<BiquadLPComposable<false, 2, Filter::ETopo::TDF2>> lp0;
		SharedPtr<BiquadLPComposable<false, 2, Filter::ETopo::TDF2>> lp1;
		SharedPtr<BiquadHPComposable<false, 2, Filter::ETopo::TDF2>> hp0;
		SharedPtr<BiquadHPComposable<false, 2, Filter::ETopo::TDF2>> hp1;
		SharedPtr<CompositeSynth> synth;
		// TODO: Do it
		return synth;
	}

	constexpr const unsigned long cdsr = 44100;

	template<template<typename> typename T, typename U>
	inline void AddEvent(T<U>& synth, const float startTime, const float freq, const float amp, const float dur)
	{
		synth->AddEvent(static_cast<unsigned long>(startTime * cdsr), CompSynthEventParams{ freq, amp, dur, cdsr });
	}

	template<template<typename> typename T, typename U>
	inline void AddEvent(T<U>& synth, const unsigned long samplenum, const float freq, const float amp, const float dur)
	{
		synth->AddEvent(samplenum, CompSynthEventParams{ freq, amp, dur, cdsr });
	}

}

