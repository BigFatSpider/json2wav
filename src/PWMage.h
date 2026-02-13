// Copyright Dan Price 2026.

#pragma once

#include "Synth.h"
#include "Math.h"
#include "Oversampler.h"
#include <stdexcept>
#include <cstdint>
#include <cmath>
//#include <atomic>

#define DEBUG_PWMAGE 0

#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
#include "Memory.h"
#include <iostream>
#include <fstream>
#endif

namespace json2wav
{
	enum class EPWMageParam
	{
		SynthParam, ModAmt, ModCenter
	};

	template<uint8_t eChanMask>
	class PWMageEvent : public SynthEvent<PWMageEvent<eChanMask>>
	{
	private:
		static ESynthParam VerifyPWMageParam(const EPWMageParam param)
		{
			if (param == EPWMageParam::SynthParam)
			{
				throw std::runtime_error("Don't initialize PWMageEvents with EPWMageParam::SynthParam! Use ESynthParam values instead.");
			}
			return ESynthParam::Phase; // Tell SynthEvent to set phase_ramp
		}

	public:
		template<typename... RampArgTypes>
		PWMageEvent(const ESynthParam param_init, RampArgTypes&&... rampArgs)
			: SynthEvent<PWMageEvent<eChanMask>>(param_init, std::forward<RampArgTypes>(rampArgs)...)
			, pwmageParam(EPWMageParam::SynthParam)
		{
		}

		template<typename... RampArgTypes>
		PWMageEvent(const EPWMageParam param_init, RampArgTypes&&... rampArgs)
			: SynthEvent<PWMageEvent<eChanMask>>(VerifyPWMageParam(param_init), std::forward<RampArgTypes>(rampArgs)...)
			, pwmageParam(param_init)
		{
		}

		virtual void Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const override;

		const EPWMageParam pwmageParam;
	};

	struct PWMSquareState
	{
		PWMSquareState()
			: freqm2(1000.0f), phasem2(1.0-1000.0/44100.0), pwm2(0.3), pwmm2(0.7), pmphasem2(1.0-1000.0/44100.0)
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
			, pwmsq_freqm1(1000.0f), pwmsq_wm1(0.001), pwmsq_phasem1(1.0-1000.0/88200.0), pwmsq_pmphasem1(1.0-1000.0/88200.0)
			, pwmsq_pwm1(0.5), pwmsq_pwmm1(0.5), modampm1(0.5), saw_instaphasem1(1.0-1000.0/88200.0)
			, pmsaw_instaphasem1(1.0-1000.0/88200.0), pmsaw_instafreqm1(1000.0/44100.0)
			, saw_smpm1(0.0), pmsaw_smpm1(0.0), pwmsq_smpm1(0.0), pwmsq_w(0.001)
			, pwmsq_pmphase(1.0-1000.0/44100.0), modamp(0.5), saw_instaphase(1.0-1000.0/44100.0)
			, pmsaw_instaphase(1.0-1000.0/44100.0), pmsaw_instafreq(1000.0/44100.0)
			, saw_smp(0.0), pmsaw_smp(0.0), pwmsq_smp(0.0)
#endif
			, saw_prev{ 0 }, pmsaw_prev{ 0 }
		{
		}
		oversampling::downsampler441_x2<double> ds;
		float freqm2;
		double phasem2;
		double pwm2;
		double pwmm2;
		double pmphasem2;

#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
		float pwmsq_freqm1;
		double pwmsq_wm1;
		double pwmsq_phasem1;
		double pwmsq_pmphasem1;
		double pwmsq_pwm1;
		double pwmsq_pwmm1;
		double modampm1;
		double saw_instaphasem1;
		double pmsaw_instaphasem1;
		double pmsaw_instafreqm1;
		double saw_smpm1;
		double pmsaw_smpm1;
		double pwmsq_smpm1;
		double pwmsq_w;
		double pwmsq_pmphase;
		double modamp;
		double saw_instaphase;
		double pmsaw_instaphase;
		double pmsaw_instafreq;
		double saw_smp;
		double pmsaw_smp;
		double pwmsq_smp;
#endif

		double saw_prev[4];
		double pmsaw_prev[4];

	};

#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
	struct PWMSquareStateCapture
	{
		PWMSquareStateCapture(const PWMSquareStateCapture& other)
			: freqm2(other.freqm2), phasem2(other.phasem2), pwm2(other.pwm2), pwmm2(other.pwmm2), pmphasem2(other.pmphasem2)

			, pwmsq_freqm1(other.pwmsq_freqm1), pwmsq_wm1(other.pwmsq_wm1), pwmsq_phasem1(other.pwmsq_phasem1)
			, pwmsq_pmphasem1(other.pwmsq_pmphasem1), pwmsq_pwm1(other.pwmsq_pwm1), pwmsq_pwmm1(other.pwmsq_pwmm1)
			, modampm1(other.modampm1), saw_instaphasem1(other.saw_instaphasem1), pmsaw_instaphasem1(other.pmsaw_instaphasem1)
			, pmsaw_instafreqm1(other.pmsaw_instafreqm1), saw_smpm1(other.saw_smpm1), pmsaw_smpm1(other.pmsaw_smpm1)
			, pwmsq_smpm1(other.pwmsq_smpm1), pwmsq_w(other.pwmsq_w), pwmsq_pmphase(other.pwmsq_pmphase)
			, modamp(other.modamp), saw_instaphase(other.saw_instaphase)

			, pmsaw_instaphase(other.pmsaw_instaphase), pmsaw_instafreq(other.pmsaw_instafreq)

			, saw_smp(other.saw_smp), pmsaw_smp(other.pmsaw_smp), pwmsq_smp(other.pwmsq_smp)

			, output(other.output)

			, saw_prev{ other.saw_prev[0], other.saw_prev[1], other.saw_prev[2], other.saw_prev[3]
				//, other.saw_prev[4], other.saw_prev[5], other.saw_prev[6]
				}
			, pmsaw_prev{ other.pmsaw_prev[0], other.pmsaw_prev[1], other.pmsaw_prev[2], other.pmsaw_prev[3]
				//, other.pmsaw_prev[4], other.pmsaw_prev[5], other.pmsaw_prev[6]
				}
		{
		}

		PWMSquareStateCapture(const PWMSquareState& other)
			: freqm2(other.freqm2), phasem2(other.phasem2), pwm2(other.pwm2), pwmm2(other.pwmm2), pmphasem2(other.pmphasem2)

			, pwmsq_freqm1(other.pwmsq_freqm1), pwmsq_wm1(other.pwmsq_wm1), pwmsq_phasem1(other.pwmsq_phasem1)
			, pwmsq_pmphasem1(other.pwmsq_pmphasem1), pwmsq_pwm1(other.pwmsq_pwm1), pwmsq_pwmm1(other.pwmsq_pwmm1)
			, modampm1(other.modampm1), saw_instaphasem1(other.saw_instaphasem1), pmsaw_instaphasem1(other.pmsaw_instaphasem1)
			, pmsaw_instafreqm1(other.pmsaw_instafreqm1), saw_smpm1(other.saw_smpm1), pmsaw_smpm1(other.pmsaw_smpm1)
			, pwmsq_smpm1(other.pwmsq_smpm1), pwmsq_w(other.pwmsq_w), pwmsq_pmphase(other.pwmsq_pmphase)
			, modamp(other.modamp), saw_instaphase(other.saw_instaphase)

			, pmsaw_instaphase(other.pmsaw_instaphase), pmsaw_instafreq(other.pmsaw_instafreq)

			, saw_smp(other.saw_smp), pmsaw_smp(other.pmsaw_smp), pwmsq_smp(other.pwmsq_smp)

			, output(0.0f)

			, saw_prev{ other.saw_prev[0], other.saw_prev[1], other.saw_prev[2], other.saw_prev[3]
				//, other.saw_prev[4], other.saw_prev[5], other.saw_prev[6]
				}
			, pmsaw_prev{ other.pmsaw_prev[0], other.pmsaw_prev[1], other.pmsaw_prev[2], other.pmsaw_prev[3]
				//, other.pmsaw_prev[4], other.pmsaw_prev[5], other.pmsaw_prev[6]
				}
		{
		}

		PWMSquareStateCapture() : PWMSquareStateCapture(PWMSquareState()) {}

		PWMSquareStateCapture& operator=(const PWMSquareStateCapture& other)
		{
			if (&other != this)
			{
				freqm2 = other.freqm2;
				phasem2 = other.phasem2;
				pwm2 = other.pwm2;
				pwmm2 = other.pwmm2;
				pmphasem2 = other.pmphasem2;

				pwmsq_freqm1 = other.pwmsq_freqm1;
				pwmsq_wm1 = other.pwmsq_wm1;
				pwmsq_phasem1 = other.pwmsq_phasem1;
				pwmsq_pmphasem1 = other.pwmsq_pmphasem1;
				pwmsq_pwm1 = other.pwmsq_pwm1;
				pwmsq_pwmm1 = other.pwmsq_pwmm1;
				modampm1 = other.modampm1;
				saw_instaphasem1 = other.saw_instaphasem1;
				pmsaw_instaphasem1 = other.pmsaw_instaphasem1;
				pmsaw_instafreqm1 = other.pmsaw_instafreqm1;
				saw_smpm1 = other.saw_smpm1;
				pmsaw_smpm1 = other.pmsaw_smpm1;
				pwmsq_smpm1 = other.pwmsq_smpm1;
				pwmsq_w = other.pwmsq_w;
				pwmsq_pmphase = other.pwmsq_pmphase;
				modamp = other.modamp;
				saw_instaphase = other.saw_instaphase;

				pmsaw_instaphase = other.pmsaw_instaphase;
				pmsaw_instafreq = other.pmsaw_instafreq;

				saw_smp = other.saw_smp;
				pmsaw_smp = other.pmsaw_smp;
				pwmsq_smp = other.pwmsq_smp;

				output = other.output;

				saw_prev[0] = other.saw_prev[0];
				saw_prev[1] = other.saw_prev[1];
				saw_prev[2] = other.saw_prev[2];
				saw_prev[3] = other.saw_prev[3];
				//saw_prev[4] = other.saw_prev[4];
				//saw_prev[5] = other.saw_prev[5];
				//saw_prev[6] = other.saw_prev[6];
				pmsaw_prev[0] = other.pmsaw_prev[0];
				pmsaw_prev[1] = other.pmsaw_prev[1];
				pmsaw_prev[2] = other.pmsaw_prev[2];
				pmsaw_prev[3] = other.pmsaw_prev[3];
				//pmsaw_prev[4] = other.pmsaw_prev[4];
				//pmsaw_prev[5] = other.pmsaw_prev[5];
				//pmsaw_prev[6] = other.pmsaw_prev[6];
			}
			return *this;
		}

		PWMSquareStateCapture& operator=(const PWMSquareState& other)
		{
			freqm2 = other.freqm2;
			phasem2 = other.phasem2;
			pwm2 = other.pwm2;
			pwmm2 = other.pwmm2;
			pmphasem2 = other.pmphasem2;

			pwmsq_freqm1 = other.pwmsq_freqm1;
			pwmsq_wm1 = other.pwmsq_wm1;
			pwmsq_phasem1 = other.pwmsq_phasem1;
			pwmsq_pmphasem1 = other.pwmsq_pmphasem1;
			pwmsq_pwm1 = other.pwmsq_pwm1;
			pwmsq_pwmm1 = other.pwmsq_pwmm1;
			modampm1 = other.modampm1;
			saw_instaphasem1 = other.saw_instaphasem1;
			pmsaw_instaphasem1 = other.pmsaw_instaphasem1;
			pmsaw_instafreqm1 = other.pmsaw_instafreqm1;
			saw_smpm1 = other.saw_smpm1;
			pmsaw_smpm1 = other.pmsaw_smpm1;
			pwmsq_smpm1 = other.pwmsq_smpm1;
			pwmsq_w = other.pwmsq_w;
			pwmsq_pmphase = other.pwmsq_pmphase;
			modamp = other.modamp;
			saw_instaphase = other.saw_instaphase;

			pmsaw_instaphase = other.pmsaw_instaphase;
			pmsaw_instafreq = other.pmsaw_instafreq;

			saw_smp = other.saw_smp;
			pmsaw_smp = other.pmsaw_smp;
			pwmsq_smp = other.pwmsq_smp;

			saw_prev[0] = other.saw_prev[0];
			saw_prev[1] = other.saw_prev[1];
			saw_prev[2] = other.saw_prev[2];
			saw_prev[3] = other.saw_prev[3];
			//saw_prev[4] = other.saw_prev[4];
			//saw_prev[5] = other.saw_prev[5];
			//saw_prev[6] = other.saw_prev[6];
			pmsaw_prev[0] = other.pmsaw_prev[0];
			pmsaw_prev[1] = other.pmsaw_prev[1];
			pmsaw_prev[2] = other.pmsaw_prev[2];
			pmsaw_prev[3] = other.pmsaw_prev[3];
			//pmsaw_prev[4] = other.pmsaw_prev[4];
			//pmsaw_prev[5] = other.pmsaw_prev[5];
			//pmsaw_prev[6] = other.pmsaw_prev[6];

			return *this;
		}

		float freqm2;
		double phasem2;
		double pwm2;
		double pwmm2;
		double pmphasem2;

		float pwmsq_freqm1;
		double pwmsq_wm1;
		double pwmsq_phasem1;
		double pwmsq_pmphasem1;
		double pwmsq_pwm1;
		double pwmsq_pwmm1;
		double modampm1;
		double saw_instaphasem1;
		double pmsaw_instaphasem1;
		double pmsaw_instafreqm1;
		double saw_smpm1;
		double pmsaw_smpm1;
		double pwmsq_smpm1;
		double pwmsq_w;
		double pwmsq_pmphase;
		double modamp;
		double saw_instaphase;
		double pmsaw_instaphase;
		double pmsaw_instafreq;
		double saw_smp;
		double pmsaw_smp;
		double pwmsq_smp;
		float output;

		double saw_prev[4];
		double pmsaw_prev[4];

	};
#endif

	namespace EPWMageChanMask
	{
		enum EPWMageChanMask_t : uint8_t
		{
			Mono = 1,
			Stereo = Mono << 1,
			Triple = Mono | Stereo
		};
	}

	template<uint8_t eChanMask> struct PWMageChannels
	{
		static constexpr const uint8_t size = 1;
		static constexpr const uint8_t cidx = 0;
		static constexpr const uint8_t lidx = size;
		static constexpr const uint8_t ridx = size;
	};
	template<> struct PWMageChannels<EPWMageChanMask::Stereo>
	{
		static constexpr const uint8_t size = 2;
		static constexpr const uint8_t cidx = size;
		static constexpr const uint8_t lidx = 0;
		static constexpr const uint8_t ridx = 1;
	};
	template<> struct PWMageChannels<EPWMageChanMask::Triple>
	{
		static constexpr const uint8_t size = 3;
		static constexpr const uint8_t cidx = 2;
		static constexpr const uint8_t lidx = 0;
		static constexpr const uint8_t ridx = 1;
	};

#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
	constexpr const size_t history_size = 512;
	constexpr const size_t history_mask = history_size - 1;
	static_assert((history_size & (history_size - 1)) == 0, "history_size must be a power of 2");
#endif

	template<uint8_t eChanMask>
	class PWMage : public SynthWithCustomEvent<PWMageEvent<eChanMask>>
	{
	public:
		PWMage(const float frequency_init = 1000.0f
			, const float amplitude_init = 0.5f
			, const double phase_init = 0.0)
			: SynthWithCustomEvent<PWMageEvent<eChanMask>>(frequency_init, amplitude_init, phase_init)
			, amt(0.7)
			, center(0.3)
			, bStateInitialized(false)
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
			//, crackle{ 0 }
			//, crackre{ 0 }
			, crackle(history_size, 0)
			, crackre(history_size, 0)
			, cracklepos(0)
			, nsamplesgot(0)
			, lhistory(history_size)
			, rhistory(history_size)
			, chistory(history_size)
#endif
		{
		}

		PWMage(const PWMage& other)
			: SynthWithCustomEvent<PWMageEvent<eChanMask>>(other)
			, amt(other.amt)
			, center(other.center)
			, bStateInitialized(other.bStateInitialized)
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
			//, crackle{ 0 }
			//, crackre{ 0 }
			, crackle(other.crackle)
			, crackre(other.crackre)
			, cracklepos(other.cracklepos)
			, nsamplesgot(other.nsamplesgot)
			, lhistory(other.lhistory)
			, rhistory(other.rhistory)
			, chistory(other.chistory)
#endif
		{
			//for (unsigned int i = 0; i < history_size; ++i)
			//{
				//crackle[i] = other.crackle[i];
				//crackre[i] = other.crackre[i];
				//lhistory[i] = other.lhistory[i];
				//rhistory[i] = other.rhistory[i];
				//chistory[i] = other.chistory[i];
			//}
		}

		PWMage(PWMage&& other) noexcept
			: SynthWithCustomEvent<PWMageEvent<eChanMask>>(std::move(other))
			, amt(other.amt)
			, center(other.center)
			, bStateInitialized(other.bStateInitialized)
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
			//, crackle{ 0 }
			//, crackre{ 0 }
			, crackle(std::move(other.crackle))
			, crackre(std::move(other.crackre))
			, cracklepos(other.cracklepos)
			, nsamplesgot(other.nsamplesgot)
			, lhistory(std::move(other.lhistory))
			, rhistory(std::move(other.rhistory))
			, chistory(std::move(other.chistory))
#endif
		{
			//for (unsigned int i = 0; i < history_size; ++i)
			//{
				//crackle[i] = other.crackle[i];
				//crackre[i] = other.crackre[i];
				//lhistory[i] = other.lhistory[i];
				//rhistory[i] = other.rhistory[i];
				//chistory[i] = other.chistory[i];
			//}
		}

		PWMage& operator=(const PWMage& other)
		{
			if (&other != this)
			{
				amt = other.amt;
				center = other.center;
				bStateInitialized = other.bStateInitialized;
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
				cracklepos = other.cracklepos;
				nsamplesgot = other.nsamplesgot;
				//for (unsigned int i = 0; i < history_size; ++i)
				//{
					//crackle[i] = other.crackle[i];
					//crackre[i] = other.crackre[i];
					//lhistory[i] = other.lhistory[i];
					//rhistory[i] = other.rhistory[i];
					//chistory[i] = other.chistory[i];
				//}
				crackle = other.crackle;
				crackre = other.crackre;
				lhistory = other.lhistory;
				rhistory = other.rhistory;
				chistory = other.chistory;
#endif
			}
			return *this;
		}

		PWMage& operator=(PWMage&& other) noexcept
		{
			if (&other != this)
			{
				amt = other.amt;
				center = other.center;
				bStateInitialized = other.bStateInitialized;
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
				cracklepos = other.cracklepos;
				nsamplesgot = other.nsamplesgot;
				//for (unsigned int i = 0; i < history_size; ++i)
				//{
					//crackle[i] = other.crackle[i];
					//crackre[i] = other.crackre[i];
					//lhistory[i] = other.lhistory[i];
					//rhistory[i] = other.rhistory[i];
					//chistory[i] = other.chistory[i];
				//}
				crackle = std::move(other.crackle);
				crackre = std::move(other.crackre);
				lhistory = std::move(other.lhistory);
				rhistory = std::move(other.rhistory);
				chistory = std::move(other.chistory);
#endif
			}
			return *this;
		}

		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t numSamples,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{

			const double deltaTime = 1.0 / static_cast<double>(sampleRate);
			do
			{
				this->GetSynthSamples(bufs, 2, numSamples, false, [this, bufs, deltaTime](const size_t i)
					{
						constexpr const double one_third = 1.0/3.0;
						constexpr const double two_thirds = 2.0/3.0;
						this->Increment(deltaTime);
						this->IncrementPW(deltaTime);
						const float freqnow = this->GetFrequency();
						const float ampnow = this->GetAmplitude();
						const double phasenow = this->GetInstantaneousPhase();
						const double pwnow = this->GetModCenter();
						const double pwmnow = this->GetModAmt();
						float pwL = 0.0f;
						float pwC = 0.0f;
						float pwR = 0.0f;
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
						PWMSquareStateCapture lcap;
						PWMSquareStateCapture rcap;
						PWMSquareStateCapture ccap;
#endif
						if constexpr ((eChanMask & EPWMageChanMask::Mono) == EPWMageChanMask::Mono)
						{
							pwC = math::f32::sq2inv*GenPWMSquare(deltaTime, freqnow, phasenow, 0.0, pwnow, pwmnow, pwStates[PWMageChannels<eChanMask>::cidx]);
							//pwC = math::f32::sq2inv*GenPWMSquare(deltaTime, freqnow, phasenow, one_third, pwnow, pwmnow, pwStates[PWMageChannels<eChanMask>::cidx]);
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
							ccap = pwStates[PWMageChannels<eChanMask>::cidx];
							ccap.output = pwC;
#endif
						}
						if constexpr ((eChanMask & EPWMageChanMask::Stereo) == EPWMageChanMask::Stereo)
						{
							pwL = GenPWMSquare(deltaTime, freqnow, phasenow, one_third, pwnow, pwmnow, pwStates[PWMageChannels<eChanMask>::lidx]);
							pwR = GenPWMSquare(deltaTime, freqnow, phasenow, two_thirds, pwnow, pwmnow, pwStates[PWMageChannels<eChanMask>::ridx]);
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
							lcap = pwStates[PWMageChannels<eChanMask>::lidx];
							rcap = pwStates[PWMageChannels<eChanMask>::ridx];
							lcap.output = pwL;
							rcap.output = pwR;
#endif
							bufs[0][i] = ampnow*(pwL + pwC);
							bufs[1][i] = ampnow*(pwR + pwC);
						}
						else
						{
							pwC *= ampnow;
							bufs[0][i] = pwC;
							bufs[1][i] = pwC;
						}

#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
						const float l0(bufs[0][i].AsFloat32());
						const float r0(bufs[1][i].AsFloat32());

						const float lm1(crackle[(cracklepos - 1) & history_mask]);
						const float lm2(crackle[(cracklepos - 2) & history_mask]);
						const float lm3(crackle[(cracklepos - 3) & history_mask]);
						const float lm4(crackle[(cracklepos - 4) & history_mask]);
						const float lm5(crackle[(cracklepos - 5) & history_mask]);
						const float lm6(crackle[(cracklepos - 6) & history_mask]);
						const float lm7(crackle[(cracklepos - 7) & history_mask]);
						const float lm8(crackle[(cracklepos - 8) & history_mask]);
						const float lm9(crackle[(cracklepos - 9) & history_mask]);
						const float lm10(crackle[(cracklepos - 10) & history_mask]);
						const float lm11(crackle[(cracklepos - 11) & history_mask]);
						const float lm12(crackle[(cracklepos - 12) & history_mask]);
						const float lm13(crackle[(cracklepos - 13) & history_mask]);
						const float lm14(crackle[(cracklepos - 14) & history_mask]);
						const float lm15(crackle[(cracklepos - 15) & history_mask]);
						const float lm16(crackle[(cracklepos - 16) & history_mask]);
						const float lm17(crackle[(cracklepos - 17) & history_mask]);
						const float lm18(crackle[(cracklepos - 18) & history_mask]);
						const float lm19(crackle[(cracklepos - 19) & history_mask]);
						const float lm20(crackle[(cracklepos - 20) & history_mask]);
						const float lm21(crackle[(cracklepos - 21) & history_mask]);
						const float lm22(crackle[(cracklepos - 22) & history_mask]);
						const float lm23(crackle[(cracklepos - 23) & history_mask]);
						const float lm24(crackle[(cracklepos - 24) & history_mask]);
						const float lm25(crackle[(cracklepos - 25) & history_mask]);
						const float lm26(crackle[(cracklepos - 26) & history_mask]);
						const float lm27(crackle[(cracklepos - 27) & history_mask]);
						const float lm28(crackle[(cracklepos - 28) & history_mask]);
						const float lm29(crackle[(cracklepos - 29) & history_mask]);
						const float lm30(crackle[(cracklepos - 30) & history_mask]);
						const float lm31(crackle[(cracklepos - 31) & history_mask]);
						const float lm32(crackle[cracklepos & history_mask]);

						const float rm1(crackre[(cracklepos - 1) & history_mask]);
						const float rm2(crackre[(cracklepos - 2) & history_mask]);
						const float rm3(crackre[(cracklepos - 3) & history_mask]);
						const float rm4(crackre[(cracklepos - 4) & history_mask]);
						const float rm5(crackre[(cracklepos - 5) & history_mask]);
						const float rm6(crackre[(cracklepos - 6) & history_mask]);
						const float rm7(crackre[(cracklepos - 7) & history_mask]);
						const float rm8(crackre[(cracklepos - 8) & history_mask]);
						const float rm9(crackre[(cracklepos - 9) & history_mask]);
						const float rm10(crackre[(cracklepos - 10) & history_mask]);
						const float rm11(crackre[(cracklepos - 11) & history_mask]);
						const float rm12(crackre[(cracklepos - 12) & history_mask]);
						const float rm13(crackre[(cracklepos - 13) & history_mask]);
						const float rm14(crackre[(cracklepos - 14) & history_mask]);
						const float rm15(crackre[(cracklepos - 15) & history_mask]);
						const float rm16(crackre[(cracklepos - 16) & history_mask]);
						const float rm17(crackre[(cracklepos - 17) & history_mask]);
						const float rm18(crackre[(cracklepos - 18) & history_mask]);
						const float rm19(crackre[(cracklepos - 19) & history_mask]);
						const float rm20(crackre[(cracklepos - 20) & history_mask]);
						const float rm21(crackre[(cracklepos - 21) & history_mask]);
						const float rm22(crackre[(cracklepos - 22) & history_mask]);
						const float rm23(crackre[(cracklepos - 23) & history_mask]);
						const float rm24(crackre[(cracklepos - 24) & history_mask]);
						const float rm25(crackre[(cracklepos - 25) & history_mask]);
						const float rm26(crackre[(cracklepos - 26) & history_mask]);
						const float rm27(crackre[(cracklepos - 27) & history_mask]);
						const float rm28(crackre[(cracklepos - 28) & history_mask]);
						const float rm29(crackre[(cracklepos - 29) & history_mask]);
						const float rm30(crackre[(cracklepos - 30) & history_mask]);
						const float rm31(crackre[(cracklepos - 31) & history_mask]);
						const float rm32(crackre[cracklepos & history_mask]);

						const float slope = (l0 - lm8)*0.125f;
						const float srope = (r0 - rm8)*0.125f;
						auto line = [l0, slope](const float x) { return slope*x + l0; };
						auto rine = [r0, srope](const float x) { return srope*x + r0; };

						constexpr const float CRACKLE_CLOSE = 0.1f;

						const float lm1dist(lm1 - line(-1.0f));
						const float lm2dist(lm2 - line(-2.0f));
						const float lm4dist(lm4 - line(-4.0f));
						const float lm5dist(lm5 - line(-5.0f));

						const float rm1dist(rm1 - rine(-1.0f));
						const float rm2dist(rm2 - rine(-2.0f));
						const float rm4dist(rm4 - rine(-4.0f));
						const float rm5dist(rm5 - rine(-5.0f));

						if (((lm1dist < 0.0f) == (lm2dist < 0.0f)) &&
							((lm4dist < 0.0f) == (lm5dist < 0.0f)) &&
							((lm1dist < 0.0f) != (lm4dist < 0.0f)) &&
							(lm1dist*lm1dist >= CRACKLE_CLOSE) &&
							(lm2dist*lm2dist >= CRACKLE_CLOSE) &&
							(lm4dist*lm4dist >= CRACKLE_CLOSE) &&
							(lm5dist*lm5dist >= CRACKLE_CLOSE))
						{
							//std::cout << "Crackle left at sample " << (nsamplesgot + i) << '\n';
						}

						if (((rm1dist < 0.0f) == (rm2dist < 0.0f)) &&
							((rm4dist < 0.0f) == (rm5dist < 0.0f)) &&
							((rm1dist < 0.0f) != (rm4dist < 0.0f)) &&
							(rm1dist*rm1dist >= CRACKLE_CLOSE) &&
							(rm2dist*rm2dist >= CRACKLE_CLOSE) &&
							(rm4dist*rm4dist >= CRACKLE_CLOSE) &&
							(rm5dist*rm5dist >= CRACKLE_CLOSE))
						{
							//std::cout << "Crackle right at sample " << (nsamplesgot + i) << '\n';
						}

						if ((nsamplesgot + i) == 762862)
						{
							std::cout << "762862 left:\n";
							std::cout << "l0 = " << l0 << '\n';
							std::cout << "lm1 = " << lm1 << '\n';
							std::cout << "lm2 = " << lm2 << '\n';
							std::cout << "lm3 = " << lm3 << '\n';
							std::cout << "lm4 = " << lm4 << '\n';
							std::cout << "lm5 = " << lm5 << '\n';
							std::cout << "lm6 = " << lm6 << '\n';
							std::cout << "lm7 = " << lm7 << '\n';
							std::cout << "lm8 = " << lm8 << '\n';
							std::cout << "lm9 = " << lm9 << '\n';
							std::cout << "lm10 = " << lm10 << '\n';
							std::cout << "lm11 = " << lm11 << '\n';
							std::cout << "lm12 = " << lm12 << '\n';
							std::cout << "lm13 = " << lm13 << '\n';
							std::cout << "lm14 = " << lm14 << '\n';
							std::cout << "lm15 = " << lm15 << '\n';
							std::cout << "lm16 = " << lm16 << '\n';
							std::cout << "lm17 = " << lm17 << '\n';
							std::cout << "lm18 = " << lm18 << '\n';
							std::cout << "lm19 = " << lm19 << '\n';
							std::cout << "lm20 = " << lm20 << '\n';
							std::cout << "lm21 = " << lm21 << '\n';
							std::cout << "lm22 = " << lm22 << '\n';
							std::cout << "lm23 = " << lm23 << '\n';
							std::cout << "lm24 = " << lm24 << '\n';
							std::cout << "lm25 = " << lm25 << '\n';
							std::cout << "lm26 = " << lm26 << '\n';
							std::cout << "lm27 = " << lm27 << '\n';
							std::cout << "lm28 = " << lm28 << '\n';
							std::cout << "lm29 = " << lm29 << '\n';
							std::cout << "lm30 = " << lm30 << '\n';
							std::cout << "lm31 = " << lm31 << '\n';
							std::cout << "lm32 = " << lm32 << '\n';
							std::cout << "Slope = " << slope << '\n';
							std::cout << "lm1dist = " << lm1dist << '\n';
							std::cout << "lm2dist = " << lm2dist << '\n';
							std::cout << "lm4dist = " << lm4dist << '\n';
							std::cout << "lm5dist = " << lm5dist << '\n';

							std::cout << "762862 right:\n";
							std::cout << "r0 = " << r0 << '\n';
							std::cout << "rm1 = " << rm1 << '\n';
							std::cout << "rm2 = " << rm2 << '\n';
							std::cout << "rm3 = " << rm3 << '\n';
							std::cout << "rm4 = " << rm4 << '\n';
							std::cout << "rm5 = " << rm5 << '\n';
							std::cout << "rm6 = " << rm6 << '\n';
							std::cout << "rm7 = " << rm7 << '\n';
							std::cout << "rm8 = " << rm8 << '\n';
							std::cout << "rm9 = " << rm9 << '\n';
							std::cout << "rm10 = " << rm10 << '\n';
							std::cout << "rm11 = " << rm11 << '\n';
							std::cout << "rm12 = " << rm12 << '\n';
							std::cout << "rm13 = " << rm13 << '\n';
							std::cout << "rm14 = " << rm14 << '\n';
							std::cout << "rm15 = " << rm15 << '\n';
							std::cout << "rm16 = " << rm16 << '\n';
							std::cout << "rm17 = " << rm17 << '\n';
							std::cout << "rm18 = " << rm18 << '\n';
							std::cout << "rm19 = " << rm19 << '\n';
							std::cout << "rm20 = " << rm20 << '\n';
							std::cout << "rm21 = " << rm21 << '\n';
							std::cout << "rm22 = " << rm22 << '\n';
							std::cout << "rm23 = " << rm23 << '\n';
							std::cout << "rm24 = " << rm24 << '\n';
							std::cout << "rm25 = " << rm25 << '\n';
							std::cout << "rm26 = " << rm26 << '\n';
							std::cout << "rm27 = " << rm27 << '\n';
							std::cout << "rm28 = " << rm28 << '\n';
							std::cout << "rm29 = " << rm29 << '\n';
							std::cout << "rm30 = " << rm30 << '\n';
							std::cout << "rm31 = " << rm31 << '\n';
							std::cout << "rm32 = " << rm32 << '\n';
							std::cout << "Srope = " << srope << '\n';
							std::cout << "rm1dist = " << rm1dist << '\n';
							std::cout << "rm2dist = " << rm2dist << '\n';
							std::cout << "rm4dist = " << rm4dist << '\n';
							std::cout << "rm5dist = " << rm5dist << '\n';

							std::cout << "762862 PWMSquareState left:\n";
							std::cout << "Minus 0: ";
							std::cout << "freqm2 = " << lcap.freqm2 << '\n';

							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "freqm2 = " << lhistory[idx].freqm2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "phasem2 = " << lcap.phasem2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "phasem2 = " << lhistory[idx].phasem2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pwm2 = " << lcap.pwm2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pwm2 = " << lhistory[idx].pwm2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pwmm2 = " << lcap.pwmm2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pwmm2 = " << lhistory[idx].pwmm2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmphasem2 = " << lcap.pmphasem2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmphasem2 = " << lhistory[idx].pmphasem2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_instaphase = " << lcap.pmsaw_instaphase << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_instaphase = " << lhistory[idx].pmsaw_instaphase << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_instafreq = " << lcap.pmsaw_instafreq << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_instafreq = " << lhistory[idx].pmsaw_instafreq << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "saw_prev[0] = " << lcap.saw_prev[0] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "saw_prev[0] = " << lhistory[idx].saw_prev[0] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "saw_prev[1] = " << lcap.saw_prev[1] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "saw_prev[1] = " << lhistory[idx].saw_prev[1] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "saw_prev[2] = " << lcap.saw_prev[2] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "saw_prev[2] = " << lhistory[idx].saw_prev[2] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_prev[0] = " << lcap.pmsaw_prev[0] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_prev[0] = " << lhistory[idx].pmsaw_prev[0] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_prev[1] = " << lcap.pmsaw_prev[1] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_prev[1] = " << lhistory[idx].pmsaw_prev[1] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_prev[2] = " << lcap.pmsaw_prev[2] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_prev[2] = " << lhistory[idx].pmsaw_prev[2] << '\n';
							}

							std::cout << "762862 PWMSquareState right:\n";
							std::cout << "Minus 0: ";
							std::cout << "freqm2 = " << rcap.freqm2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "freqm2 = " << rhistory[idx].freqm2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "phasem2 = " << rcap.phasem2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "phasem2 = " << rhistory[idx].phasem2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pwm2 = " << rcap.pwm2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pwm2 = " << rhistory[idx].pwm2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pwmm2 = " << rcap.pwmm2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pwmm2 = " << rhistory[idx].pwmm2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmphasem2 = " << rcap.pmphasem2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmphasem2 = " << rhistory[idx].pmphasem2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_instaphase = " << rcap.pmsaw_instaphase << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_instaphase = " << rhistory[idx].pmsaw_instaphase << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_instafreq = " << rcap.pmsaw_instafreq << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_instafreq = " << rhistory[idx].pmsaw_instafreq << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "saw_prev[0] = " << rcap.saw_prev[0] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "saw_prev[0] = " << rhistory[idx].saw_prev[0] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "saw_prev[1] = " << rcap.saw_prev[1] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "saw_prev[1] = " << rhistory[idx].saw_prev[1] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "saw_prev[2] = " << rcap.saw_prev[2] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "saw_prev[2] = " << rhistory[idx].saw_prev[2] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_prev[0] = " << rcap.pmsaw_prev[0] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_prev[0] = " << rhistory[idx].pmsaw_prev[0] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_prev[1] = " << rcap.pmsaw_prev[1] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_prev[1] = " << rhistory[idx].pmsaw_prev[1] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_prev[2] = " << rcap.pmsaw_prev[2] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_prev[2] = " << rhistory[idx].pmsaw_prev[2] << '\n';
							}

							std::cout << "762862 PWMSquareState center:\n";
							std::cout << "Minus 0: ";
							std::cout << "freqm2 = " << ccap.freqm2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "freqm2 = " << chistory[idx].freqm2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "phasem2 = " << ccap.phasem2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "phasem2 = " << chistory[idx].phasem2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pwm2 = " << ccap.pwm2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pwm2 = " << chistory[idx].pwm2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pwmm2 = " << ccap.pwmm2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pwmm2 = " << chistory[idx].pwmm2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmphasem2 = " << ccap.pmphasem2 << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmphasem2 = " << chistory[idx].pmphasem2 << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_instaphase = " << ccap.pmsaw_instaphase << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_instaphase = " << chistory[idx].pmsaw_instaphase << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_instafreq = " << ccap.pmsaw_instafreq << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_instafreq = " << chistory[idx].pmsaw_instafreq << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "saw_prev[0] = " << ccap.saw_prev[0] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "saw_prev[0] = " << chistory[idx].saw_prev[0] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "saw_prev[1] = " << ccap.saw_prev[1] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "saw_prev[1] = " << chistory[idx].saw_prev[1] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "saw_prev[2] = " << ccap.saw_prev[2] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "saw_prev[2] = " << chistory[idx].saw_prev[2] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_prev[0] = " << ccap.pmsaw_prev[0] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_prev[0] = " << chistory[idx].pmsaw_prev[0] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_prev[1] = " << ccap.pmsaw_prev[1] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_prev[1] = " << chistory[idx].pmsaw_prev[1] << '\n';
							}
							std::cout << "Minus 0: ";
							std::cout << "pmsaw_prev[2] = " << ccap.pmsaw_prev[2] << '\n';
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;
								std::cout << "Minus " << (j + 1) << ": ";
								std::cout << "pmsaw_prev[2] = " << chistory[idx].pmsaw_prev[2] << '\n';
							}

							std::ofstream f("onlyfearblip.csv");
							f << "l,r,";
							f << "lf,lw,lph,lpmph,lpw,lpwm,lmod,liph,lpmiph,lpmif,lsmp,lpmsmp,lsqsmp,lout,";
							f << "rf,rw,rph,rpmph,rpw,rpwm,rmod,riph,rpmiph,rpmif,rsmp,rpmsmp,rsqsmp,rout,";
							f << "cf,cw,cph,cpmph,cpw,cpwm,cmod,ciph,cpmiph,cpmif,csmp,cpmsmp,csqsmp,cout\n";
							for (unsigned int j = 0; j < history_size; ++j)
							{
								const unsigned int idx = (cracklepos - j - 1) & history_mask;

								f << crackle[idx] << ',';
								f << crackre[idx] << ',';

								f << lhistory[idx].freqm2 << ',';
								f << lhistory[idx].pwmsq_w << ',';
								f << lhistory[idx].phasem2 << ',';
								f << lhistory[idx].pwmsq_pmphase << ',';
								f << lhistory[idx].pwm2 << ',';
								f << lhistory[idx].pwmm2 << ',';
								f << lhistory[idx].modamp << ',';
								f << lhistory[idx].saw_instaphase << ',';
								f << lhistory[idx].pmsaw_instaphase << ',';
								f << lhistory[idx].pmsaw_instafreq << ',';
								f << lhistory[idx].saw_smp << ',';
								f << lhistory[idx].pmsaw_smp << ',';
								f << lhistory[idx].pwmsq_smp << ',';
								f << lhistory[idx].output << ',';

								f << rhistory[idx].freqm2 << ',';
								f << rhistory[idx].pwmsq_w << ',';
								f << rhistory[idx].phasem2 << ',';
								f << rhistory[idx].pwmsq_pmphase << ',';
								f << rhistory[idx].pwm2 << ',';
								f << rhistory[idx].pwmm2 << ',';
								f << rhistory[idx].modamp << ',';
								f << rhistory[idx].saw_instaphase << ',';
								f << rhistory[idx].pmsaw_instaphase << ',';
								f << rhistory[idx].pmsaw_instafreq << ',';
								f << rhistory[idx].saw_smp << ',';
								f << rhistory[idx].pmsaw_smp << ',';
								f << rhistory[idx].pwmsq_smp << ',';
								f << rhistory[idx].output << ',';

								f << chistory[idx].freqm2 << ',';
								f << chistory[idx].pwmsq_w << ',';
								f << chistory[idx].phasem2 << ',';
								f << chistory[idx].pwmsq_pmphase << ',';
								f << chistory[idx].pwm2 << ',';
								f << chistory[idx].pwmm2 << ',';
								f << chistory[idx].modamp << ',';
								f << chistory[idx].saw_instaphase << ',';
								f << chistory[idx].pmsaw_instaphase << ',';
								f << chistory[idx].pmsaw_instafreq << ',';
								f << chistory[idx].saw_smp << ',';
								f << chistory[idx].pmsaw_smp << ',';
								f << chistory[idx].pwmsq_smp << ',';
								f << chistory[idx].output << '\n';

								f << ",,";

								f << lhistory[idx].pwmsq_freqm1 << ',';
								f << lhistory[idx].pwmsq_wm1 << ',';
								f << lhistory[idx].pwmsq_phasem1 << ',';
								f << lhistory[idx].pwmsq_pmphasem1 << ',';
								f << lhistory[idx].pwmsq_pwm1 << ',';
								f << lhistory[idx].pwmsq_pwmm1 << ',';
								f << lhistory[idx].modampm1 << ',';
								f << lhistory[idx].saw_instaphasem1 << ',';
								f << lhistory[idx].pmsaw_instaphasem1 << ',';
								f << lhistory[idx].pmsaw_instafreqm1 << ',';
								f << lhistory[idx].saw_smpm1 << ',';
								f << lhistory[idx].pmsaw_smpm1 << ',';
								f << lhistory[idx].pwmsq_smpm1 << ',';
								f << ',';

								f << rhistory[idx].pwmsq_freqm1 << ',';
								f << rhistory[idx].pwmsq_wm1 << ',';
								f << rhistory[idx].pwmsq_phasem1 << ',';
								f << rhistory[idx].pwmsq_pmphasem1 << ',';
								f << rhistory[idx].pwmsq_pwm1 << ',';
								f << rhistory[idx].pwmsq_pwmm1 << ',';
								f << rhistory[idx].modampm1 << ',';
								f << rhistory[idx].saw_instaphasem1 << ',';
								f << rhistory[idx].pmsaw_instaphasem1 << ',';
								f << rhistory[idx].pmsaw_instafreqm1 << ',';
								f << rhistory[idx].saw_smpm1 << ',';
								f << rhistory[idx].pmsaw_smpm1 << ',';
								f << rhistory[idx].pwmsq_smpm1 << ',';
								f << ',';

								f << chistory[idx].pwmsq_freqm1 << ',';
								f << chistory[idx].pwmsq_wm1 << ',';
								f << chistory[idx].pwmsq_phasem1 << ',';
								f << chistory[idx].pwmsq_pmphasem1 << ',';
								f << chistory[idx].pwmsq_pwm1 << ',';
								f << chistory[idx].pwmsq_pwmm1 << ',';
								f << chistory[idx].modampm1 << ',';
								f << chistory[idx].saw_instaphasem1 << ',';
								f << chistory[idx].pmsaw_instaphasem1 << ',';
								f << chistory[idx].pmsaw_instafreqm1 << ',';
								f << chistory[idx].saw_smpm1 << ',';
								f << chistory[idx].pmsaw_smpm1 << ',';
								f << chistory[idx].pwmsq_smpm1 << ',';
								f << '\n';

								/*
								f << lhistory[idx].saw_prev[0] << ',';
								f << lhistory[idx].saw_prev[1] << ',';
								f << lhistory[idx].saw_prev[2] << ',';
								f << lhistory[idx].pmsaw_prev[0] << ',';
								f << lhistory[idx].pmsaw_prev[1] << ',';
								f << lhistory[idx].pmsaw_prev[2] << ',';

								f << rhistory[idx].freqm2 << ',';
								f << rhistory[idx].phasem2 << ',';
								f << rhistory[idx].pwm2 << ',';
								f << rhistory[idx].pwmm2 << ',';
								f << rhistory[idx].pmphasem2 << ',';
								f << rhistory[idx].pmsaw_instaphase << ',';
								f << rhistory[idx].pmsaw_instafreq << ',';
								f << rhistory[idx].saw_prev[0] << ',';
								f << rhistory[idx].saw_prev[1] << ',';
								f << rhistory[idx].saw_prev[2] << ',';
								f << rhistory[idx].pmsaw_prev[0] << ',';
								f << rhistory[idx].pmsaw_prev[1] << ',';
								f << rhistory[idx].pmsaw_prev[2] << ',';

								f << chistory[idx].freqm2 << ',';
								f << chistory[idx].phasem2 << ',';
								f << chistory[idx].pwm2 << ',';
								f << chistory[idx].pwmm2 << ',';
								f << chistory[idx].pmphasem2 << ',';
								f << chistory[idx].pmsaw_instaphase << ',';
								f << chistory[idx].pmsaw_instafreq << ',';
								f << chistory[idx].saw_prev[0] << ',';
								f << chistory[idx].saw_prev[1] << ',';
								f << chistory[idx].saw_prev[2] << ',';
								f << chistory[idx].pmsaw_prev[0] << ',';
								f << chistory[idx].pmsaw_prev[1] << ',';
								f << chistory[idx].pmsaw_prev[2];

								f << '\n';
								*/
							}

						}

						crackle[cracklepos & history_mask] = l0;
						crackre[cracklepos & history_mask] = r0;
						lhistory[cracklepos & history_mask] = lcap;
						rhistory[cracklepos & history_mask] = rcap;
						chistory[cracklepos & history_mask] = ccap;
						++cracklepos;
#endif
					});
			} while (NeedsInit());
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
			nsamplesgot += numSamples;
#endif
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return 2;
		}

		void SetModAmt(const PreciseRamp& ramp)
		{
			amt_ramp = ramp;
		}

		void SetModCenter(const PreciseRamp& ramp)
		{
			center_ramp = ramp;
		}

		double GetModAmt() const
		{
			return amt;
		}

		double GetModCenter() const
		{
			return center;
		}

	private:
		bool NeedsInit()
		{
			if (bStateInitialized)
				return false;
			this->SetSampleNum(0);
			bStateInitialized = true;
			return true;
		}

		void IncrementPW(const double deltaTime)
		{
			amt_ramp.Increment(amt, deltaTime);
			center_ramp.Increment(center, deltaTime);
		}

		static float GenPWMSquare(
			const double deltaTime,
			const float pwmsq_freq,
			const double pwmsq_phase,
			const double pwmsq_phaseoffset,
			const double pwmsq_pw,
			const double pwmsq_pwm,
			PWMSquareState& pwmsq_state)
		{
			// Antialiased pulse-width-modulated square wave. Combines three antialiasing techniques:
			// Oversampling x2, PolyBLEP (polynomial bandlimited step), and third-order DPW
			// (differentiated polynomial wave). Accomplishes pulse width modulation by summing a sawtooth
			// with a phase-modulated inverted and shifted sawtooth.
			const double os2dt = 0.5*deltaTime;
			const float pwmsq_freqm1 = 0.5f*(pwmsq_freq + pwmsq_state.freqm2);
			const double pwmsq_wm1 = static_cast<double>(pwmsq_freqm1)*os2dt;
			double pwmsq_phasem1 = 0.5*(pwmsq_phase + pwmsq_state.phasem2 + (pwmsq_phase < pwmsq_state.phasem2));
			pwmsq_phasem1 -= std::floor(pwmsq_phasem1);
			double pwmsq_pmphasem1 = pwmsq_state.pmphasem2 + 0.5*pwmsq_wm1;
			pwmsq_pmphasem1 -= std::floor(pwmsq_pmphasem1);
			const double pwmsq_pwm1 = 0.5*(pwmsq_pw + pwmsq_state.pwm2);
			const double pwmsq_pwmm1 = 0.5*(pwmsq_pwm + pwmsq_state.pwmm2);

			const double modampm1 = (0.5 - std::abs(pwmsq_pwm1 - 0.5))*pwmsq_pwmm1;
			double saw_instaphasem1 = pwmsq_phasem1 + pwmsq_phaseoffset;
			saw_instaphasem1 -= std::floor(saw_instaphasem1);
			// phase = mod1(x - pm*np.sin(tau*pharg))
			double pmsaw_instaphasem1 = saw_instaphasem1 - pwmsq_pwm1 - modampm1*std::sin(math::f64::tau*pwmsq_pmphasem1);
			pmsaw_instaphasem1 -= std::floor(pmsaw_instaphasem1);
			// freq = w - pm*pi*w*np.cos(tau*pharg)
			const double pmsaw_instafreqm1 = pwmsq_wm1 - math::f64::pi*pwmsq_wm1*modampm1*std::cos(math::f64::tau*pwmsq_pmphasem1);

			const double saw_smpm1 = quablepsaw(pwmsq_wm1, saw_instaphasem1, pwmsq_state.saw_prev);
			const double pmsaw_smpm1 = quablepsaw(pmsaw_instafreqm1, pmsaw_instaphasem1, pwmsq_state.pmsaw_prev);
			const double pwmsq_smpm1 = saw_smpm1 - pmsaw_smpm1;

			const double pwmsq_w = static_cast<double>(pwmsq_freq)*os2dt;
			double pwmsq_pmphase = pwmsq_pmphasem1 + 0.5*pwmsq_w;
			pwmsq_pmphase -= std::floor(pwmsq_pmphase);

			//const double saw_instaphase = pwmsq_phase;
			const double modamp = (0.5 - std::abs(pwmsq_pw - 0.5))*pwmsq_pwm;
			double saw_instaphase = pwmsq_phase + pwmsq_phaseoffset;
			saw_instaphase -= std::floor(saw_instaphase);
			double pmsaw_instaphase = saw_instaphase - pwmsq_pw - modamp*std::sin(math::f64::tau*pwmsq_pmphase);
			pmsaw_instaphase -= std::floor(pmsaw_instaphase);
			const double pmsaw_instafreq = pwmsq_w - math::f64::pi*pwmsq_w*modamp*std::cos(math::f64::tau*pwmsq_pmphase);

			const double saw_smp = quablepsaw(pwmsq_w, saw_instaphase, pwmsq_state.saw_prev);
			const double pmsaw_smp = quablepsaw(pmsaw_instafreq, pmsaw_instaphase, pwmsq_state.pmsaw_prev);
			const double pwmsq_smp = saw_smp - pmsaw_smp;

			//static std::atomic<unsigned int> count = 0;
			//if (++count <= 5000)
				//std::cout << "pwmsq_wm1 = " << pwmsq_wm1 << ", pmsaw_instafreqm1 = " << pmsaw_instafreqm1 << '\n' <<
					//"pwmsq_w = " << pwmsq_w << ", pmsaw_instafreq = " << pmsaw_instafreq << '\n';

			double dsinput[2] = { pwmsq_smpm1, pwmsq_smp };
			double output[1] = { 0.0 };
			pwmsq_state.ds.process(dsinput, output);

			/*{
				downsampler441_x2<double> ds;
				float freqm2;
				double phasem2;
				double pwm2;
				double pwmm2;
				double pmphasem2;
				double saw_prev[3];
				double pmsaw_prev[3];
			}*/

			pwmsq_state.freqm2 = pwmsq_freq;
			pwmsq_state.phasem2 = pwmsq_phase;
			pwmsq_state.pwm2 = pwmsq_pw;
			pwmsq_state.pwmm2 = pwmsq_pwm;
			pwmsq_state.pmphasem2 = pwmsq_pmphase;

#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
			pwmsq_state.pwmsq_freqm1 = pwmsq_freqm1;
			pwmsq_state.pwmsq_wm1 = pwmsq_wm1;
			pwmsq_state.pwmsq_phasem1 = pwmsq_phasem1;
			pwmsq_state.pwmsq_pmphasem1 = pwmsq_pmphasem1;
			pwmsq_state.pwmsq_pwm1 = pwmsq_pwm1;
			pwmsq_state.pwmsq_pwmm1 = pwmsq_pwmm1;
			pwmsq_state.modampm1 = modampm1;
			pwmsq_state.saw_instaphasem1 = saw_instaphasem1;
			pwmsq_state.pmsaw_instaphasem1 = pmsaw_instaphasem1;
			pwmsq_state.pmsaw_instafreqm1 = pmsaw_instafreqm1;
			pwmsq_state.saw_smpm1 = saw_smpm1;
			pwmsq_state.pmsaw_smpm1 = pmsaw_smpm1;
			pwmsq_state.pwmsq_smpm1 = pwmsq_smpm1;
			pwmsq_state.pwmsq_w = pwmsq_w;
			pwmsq_state.pwmsq_pmphase = pwmsq_pmphase;
			pwmsq_state.modamp = modamp;
			pwmsq_state.saw_instaphase = saw_instaphase;
			pwmsq_state.pmsaw_instaphase = pmsaw_instaphase;
			pwmsq_state.pmsaw_instafreq = pmsaw_instafreq;
			pwmsq_state.saw_smp = saw_smp;
			pwmsq_state.pmsaw_smp = pmsaw_smp;
			pwmsq_state.pwmsq_smp = pwmsq_smp;
#endif

			return static_cast<float>(output[0]);

			/*

				def naivepbla(x):
					return msixth + x - x*x

				def naivequartic(x):
					return threesixtieth + mtwelveth*x*x*(1.0 + x*(-2.0 + x))

				def postquablep(w, x):
					binom = 1.0 - x
					wbinom = w*binom
					wbinom2 = wbinom*binom
					return sixtieth*wbinom*wbinom2*wbinom2

				def prequablep(w, x):
					binom = 1.0 + x
					wbinom = w*binom
					wbinom2 = wbinom*binom
					return sixtieth*wbinom*wbinom2*wbinom2

				def quablep(w, x):
					if x < w:
						return postquablep(w, x/w)
					elif x > 1.0 - w:
						return prequablep(w, (x - 1.0)/w)
					return floattype(0.0)

				def quartic(w, x):
					return twelveth*w*w*naivepbla(x) + naivequartic(x) + quablep(w, x)

				def quablepsaw(w, x, yprev):
					winv = 1.0/w
					y3 = quartic(w, x)
					y2 = (y3 - yprev[3])*winv
					y1 = (y2 - yprev[2])*winv
					y0 = (y1 - yprev[1])*winv
					return y0, [y0, y1, y2, y3]

				x = 0.0
				w = f/sr
				winv = sr/f

				y3prev = quartic(w, mod1(x - w))
				y3prev2 = quartic(w, mod1(x - 2.0*w))
				y3prev3 = quartic(w, mod1(x - 3.0*w))
				y2prev = (y3prev - y3prev2)*winv
				y2prev2 = (y3prev2 - y3prev3)*winv
				y1prev = (y2prev - y2prev2)*winv
				yprev = [0.0, y1prev, y2prev, y3prev]

				pharg = floattype(0.0)
				y = [floattype(0.0)] * nsamples
				for n in range(nsamples):
					phase = mod1(x - pm*np.sin(tau*pharg))
					freq = w - pm*pi*w*np.cos(tau*pharg)
					y[n], yprev = quablepsaw(freq, phase, yprev)
					x = phasor(w, x)
					pharg = phasor(0.5*w, pharg)

			*/

		}

		static double quablepsaw(const double w, const double p, double (&prev)[4])
		{
			//const double winv = 1.0/w;
			const double winv = 0.7937/w; // Cube root of 1/2
			//const double winv = 0.39685/w; // Cube root of 1/16
			const double y4 = quartic(w, p);
			//const double y7 = quartic(w, p);

			//const double y6 = y7 + prev[6]; // No Nyquist
			//const double y5 = (y6 - prev[5])*winv;
			//const double y4 = y5 + prev[4]; // No Nyquist
			//const double y3 = (y4 - prev[3])*winv;
			//const double y2 = y3 + prev[2]; // No Nyquist
			//const double y1 = (y2 - prev[1])*winv;
			//const double y0 = y1 + prev[0]; // No Nyquist

			const double y3 = y4 + prev[3]; // No Nyquist
			const double y2 = (y3 - prev[2])*winv;
			const double y1 = (y2 - prev[1])*winv;
			const double y0 = (y1 - prev[0])*winv;

			//const double y3 = (y4 + prev[3])*winv; // No Nyquist
			//const double y2 = (y3 - prev[2])*winv;
			//const double y1 = (y2 - prev[1])*winv;
			//const double y0 = y1 - prev[0];

			//const double y3 = (y4 - prev[3])*winv;
			//const double y2 = (y3 - prev[2])*winv;
			//const double y1 = (y2 - prev[1])*winv;
			//const double y0 = y1 + prev[0]; // No Nyquist

			//prev[6] = y7;
			//prev[5] = y6;
			//prev[4] = y5;
			prev[3] = y4;
			prev[2] = y3;
			prev[1] = y2;
			prev[0] = y1;
			return y0;
		}

		static constexpr const double mtwo = -2.0;
		static constexpr const double msixth = -1.0/6.0;
		static constexpr const double twelveth = 1.0/12.0;
		static constexpr const double mtwelveth = -twelveth;
		static constexpr const double sixtieth = 1.0/60.0;
		static constexpr const double threesixtieth = 1.0/360.0;

		static double quartic(const double w, const double x)
		{
			return twelveth*w*w*naivepbla(x) + naivequartic(x) + quablep(w, x);
		}

		static double naivepbla(const double x)
		{
			return msixth + x - x*x;
		}

		static double naivequartic(const double x)
		{
			return threesixtieth + mtwelveth*x*x*(1.0 + x*(mtwo + x));
		}

		static double quablep(const double w, const double x)
		{
			if (x < 0.0)
				std::cout << "x<0 quablep\n";
			if (x == 0.0)
				std::cout << "x=0 quablep\n";
			if (x == 1.0)
				std::cout << "x=1 quablep\n";
			if (x > 1.0)
				std::cout << "x>1 quablep\n";
			if (x < w)
				return postquablep(w, x/w);
			else if (x > 1.0 - w)
				return prequablep(w, -((1.0 - x)/w));
			return 0.0;
		}

		static double postquablep(const double w, const double x)
		{
			const double binom = 1.0 - x;
			const double wbinom = w*binom;
			const double wbinom2 = wbinom*binom;
			return sixtieth*wbinom*wbinom2*wbinom2;
		}

		static double prequablep(const double w, const double x)
		{
			const double binom = 1.0 + x;
			const double wbinom = w*binom;
			const double wbinom2 = wbinom*binom;
			return sixtieth*wbinom*wbinom2*wbinom2;
		}

	private:
		double amt;
		double center;
		PreciseRamp amt_ramp;
		PreciseRamp center_ramp;
		PWMSquareState pwStates[PWMageChannels<eChanMask>::size];
		bool bStateInitialized;
#if defined(DEBUG_PWMAGE) && DEBUG_PWMAGE
		//float crackle[history_size];
		//float crackre[history_size];
		Vector<float> crackle;
		Vector<float> crackre;
		size_t cracklepos;
		size_t nsamplesgot;
		//PWMSquareStateCapture lhistory[history_size];
		//PWMSquareStateCapture rhistory[history_size];
		//PWMSquareStateCapture chistory[history_size];
		Vector<PWMSquareStateCapture> lhistory;
		Vector<PWMSquareStateCapture> rhistory;
		Vector<PWMSquareStateCapture> chistory;
#endif
	};

	template<uint8_t eChanMask>
	void PWMageEvent<eChanMask>::Activate(ControlObjectHolder& ctrl, const size_t sampleNum) const
	{
		if (pwmageParam == EPWMageParam::SynthParam)
		{
			SynthEvent<PWMageEvent<eChanMask>>::Activate(ctrl, sampleNum);
			return;
		}

		PWMage<eChanMask>& pwmage = ctrl.Get<PWMage<eChanMask>>();
		switch (pwmageParam)
		{
		case EPWMageParam::SynthParam: break;
		case EPWMageParam::ModAmt: pwmage.SetModAmt(SynthEvent<PWMageEvent<eChanMask>>::phase_ramp); break;
		case EPWMageParam::ModCenter: pwmage.SetModCenter(SynthEvent<PWMageEvent<eChanMask>>::phase_ramp); break;
		}
	}
}

