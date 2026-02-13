// Copyright Dan Price. All rights reserved.

#pragma once

#include "CompositeSynth.h"
#include "Envelope.h"
#include "NoteData.h"
#include "Utility.h"
#include "Memory.h"
#include <utility>
#include <algorithm>
#include <cmath>

namespace json2wav
{
	//constexpr const float defaultSweepTime = 0.0075f;
	constexpr const float defaultSweepTime = 0.005f;

	template<typename ConcreteAudioObject, typename EParamType, EParamType eParamAmp, EParamType eParamFreq, bool bResetOnStart>
	class EnveloperComposable : public ConcreteAudioObject, public IComposable
	{
	public:
		template<typename... ParamTypes>
		EnveloperComposable(const Envelope& envInit, ParamTypes&&... params) : ConcreteAudioObject(std::forward<ParamTypes>(params)...), env(envInit), sweeptime(defaultSweepTime), detuneFactor(1.0f), bDirty(true)
		{
		}

		virtual void GetSamples(Sample* const* const bufs, const size_t numChannels, const size_t numSamples,
			const unsigned long sampleRate, IAudioObject* const requester) noexcept override
		{
			CommitEvents(sampleRate);
			ConcreteAudioObject::GetSamples(bufs, numChannels, numSamples, sampleRate, requester);
		}

		void SetEnvelope(const Envelope& envNew)
		{
			env = envNew;
			bDirty = true;
		}

		void SetEnvelope(Envelope&& envNew)
		{
			env = std::move(envNew);
			bDirty = true;
		}

		void SetAttack(const float att)
		{
			env.attack = att;
			bDirty = true;
		}

		void SetDecay(const float dec)
		{
			env.decay = dec;
			bDirty = true;
		}

		void SetRelease(const float rel)
		{
			env.release = rel;
			bDirty = true;
		}

		void SetAttackLevel(const float lev)
		{
			env.attlevel = lev;
			bDirty = true;
		}

		void SetSustainLevel(const float lev)
		{
			env.suslevel = lev;
			bDirty = true;
		}

		void SetAttackLevelDB(const float levdb)
		{
			SetAttackLevel(Utility::DBToGain(levdb));
		}

		void SetSustainLevelDB(const float levdb)
		{
			SetSustainLevel(Utility::DBToGain(levdb));
		}

		void SetAttackRamp(const ERampShape ramp)
		{
			env.attramp = ramp;
		}

		void SetDecayRamp(const ERampShape ramp)
		{
			env.decramp = ramp;
		}

		void SetReleaseRamp(const ERampShape ramp)
		{
			env.relramp = ramp;
		}

		void SetRamp(const ERampShape ramp)
		{
			SetAttackRamp(ramp);
			SetDecayRamp(ramp);
			SetReleaseRamp(ramp);
		}

		void SetSweepTime(const float st)
		{
			sweeptime = st;
		}

		void SetDetuneFactor(const float detune)
		{
			detuneFactor = detune;
		}

	protected:
		virtual float AmpMap(const float amp) const { return amp; }

		virtual float GetResetVal() const { return 0.0f; }

	private:
		virtual void AddCompSynthEvent(const size_t samplenum, const CompSynthEventParams& params) override
		{
			const size_t samplenum_end(samplenum + params.dur*params.sampleRate);
			AddCompSynthEventInternal(samplenum, samplenum_end, params.amp, params.freq);
		}

		virtual void AddCompSynthEvent(const size_t samplenum, const CompSynthEventParams_SmpDur& params) override
		{
			const size_t samplenum_end(samplenum + params.smpdur);
			AddCompSynthEventInternal(samplenum, samplenum_end, params.amp, params.freq);
		}

		virtual float GetRelease() const override
		{
			return env.release;
		}

		void AddCompSynthEventInternal(const size_t samplenum, const size_t samplenum_end, const float amp, const float freq)
		{
			NoteData note(samplenum, samplenum_end, AmpMap(amp), freq * detuneFactor);
			const auto itpair(std::equal_range(notes.begin(), notes.end(), note,
					[](const NoteData& lhs, const NoteData& rhs)
					{ return lhs.start < rhs.start; }));
			if (itpair.first == itpair.second)
			{
				notes.emplace(itpair.first, std::move(note));
			}
			else
			{
				*itpair.first = std::move(note);
			}
			bDirty = true;
		}

		void CommitEvents(const unsigned long sampleRate)
		{
			if (!bDirty)
				return;

			const float sampleRateInv(1.0f / (float)sampleRate);
			const size_t attsamples(static_cast<size_t>(env.attack * sampleRate));
			bool bAddLastFreq(true);
			switch (notes.size())
			{
			default:
				{
					bAddLastFreq = false;
					for (Vector<NoteData>::const_reverse_iterator it(notes.crbegin() + 1), next(notes.crbegin()), end(notes.crend()); it != end; ++it, ++next)
					{
						const NoteData& note(*it);
						float nextSweepTime(defaultSweepTime);
						const size_t nextStart(next->start);
						size_t nextFreqStart(nextStart);
						if (bResetOnStart)
							this->AddEvent(note.start, eParamAmp, GetResetVal(), 16*sampleRateInv, ERampShape::SCurve);
						this->AddEvent(note.start + 16*bResetOnStart, eParamAmp, note.amp * env.attlevel, env.attack, env.attramp);
						const size_t attpeak(note.start + attsamples);
						if (attpeak < note.end)
							this->AddEvent(attpeak, eParamAmp, note.amp * env.suslevel, env.decay, env.decramp);
						if (nextStart >= note.end)
						{
							if (!bResetOnStart)
								this->AddEvent(note.end, eParamAmp, GetResetVal(), env.release, env.relramp);
							if (nextStart < (note.end + size_t(std::floor(env.release * sampleRate))))
							{
								const float releaseSweepTime = (nextStart - note.end) * sampleRateInv;
								if (releaseSweepTime < nextSweepTime)
								{
									nextSweepTime = releaseSweepTime;
									nextFreqStart = note.end;
								}
								else
								{
									nextSweepTime = sweeptime;
									nextFreqStart -= size_t(std::floor(nextSweepTime * sampleRate));
								}
							}
						}
						//this->AddEvent(nextFreqStart, eParamFreq, next->freq, nextSweepTime, ERampShape::LogScaleLinear);
						this->AddEvent(nextFreqStart, eParamFreq, next->freq, nextSweepTime, ERampShape::LogScaleSCurve);
					}

					const NoteData& firstNote(*notes.begin());
					//this->AddEvent(firstNote.start, eParamFreq, firstNote.freq, defaultSweepTime, ERampShape::Linear);
					this->AddEvent(firstNote.start, eParamFreq, firstNote.freq, defaultSweepTime, ERampShape::SCurve);
				}
				[[fallthrough]];
			case 1:
				{
					const NoteData& lastnote(*notes.crbegin()); // Confusing: crbegin and begin reference the
																// same note when size is 1 but not when size
																// is greater than 1 and, thus, this code has
																// been reached via fallthrough
					if (bAddLastFreq)
						//this->AddEvent(lastnote.start, eParamFreq, lastnote.freq, defaultSweepTime, ERampShape::Linear);
						this->AddEvent(lastnote.start, eParamFreq, lastnote.freq, defaultSweepTime, ERampShape::SCurve);
					if (bResetOnStart)
						this->AddEvent(lastnote.start, eParamAmp, GetResetVal(), 16*sampleRateInv, ERampShape::SCurve);
					this->AddEvent(lastnote.start + 16*bResetOnStart, eParamAmp, lastnote.amp * env.attlevel, env.attack, env.attramp);
					const size_t attpeak(lastnote.start + attsamples);
					if (attpeak < lastnote.end)
						this->AddEvent(attpeak, eParamAmp, lastnote.amp * env.suslevel, env.decay, env.decramp);
					if (!bResetOnStart)
						this->AddEvent(lastnote.end, eParamAmp, GetResetVal(), env.release, env.relramp);
				} break;
			case 0: break;
			}
		}

	private:
		Vector<NoteData> notes;
		Envelope env;
		float sweeptime;
		float detuneFactor;
		bool bDirty;
	};
}

