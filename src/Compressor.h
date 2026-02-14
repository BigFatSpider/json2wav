// Copyright Dan Price 2026.

#pragma once

#include "IAudioObject.h"
#include "Math.h"
#include "Sample.h"
#include "Utility.h"
#include "Memory.h"
#include "Oversampler.h"
#include <cmath>

namespace json2wav
{
	class ICompressorMeasurer
	{
	public:
		virtual ~ICompressorMeasurer() noexcept {}
		virtual void AddGainComputerRow(
			double x,
			double uk, double uc, double u,
			double xuk, double xuc, double xu,
			double wk, double wc, double g) = 0;
		virtual void ResetInput() = 0;
		virtual double GetNextInput() = 0;
		virtual bool HasMoreInput() = 0;
		virtual void AddGainComputerProcRow(
			double n, double x, double y,
			double u, double xu, double term1, double term2, double term1adaa, double term2adaa) = 0;
		virtual void AddChannelMeasurement(const Vector<Sample>& chInput, const Vector<Sample>& chOutput) = 0;
		virtual void AddChannelMeasurement(const Vector<Sample>& chInput, const Vector<Sample>& chOutput,
			const Vector<double>& gcBuf, const Vector<double>& taBuf, const Vector<double>& geBuf) = 0;
	};

	enum class ECompressorStereoMode
	{
		LR,	// Unlinked; compress left and right with the same settings
		M,	// Compress mid only
		MS	// Compress mid and side with separate settings
	};

	struct CompressorParams
	{
		double attackSamples;
		double releaseSamples;
		double threshold_db;
		double ratio;
		double knee_db;
		float dryVolume_db;
		bool df2;
	};

	template<bool bOwner = false>
	class Compressor : public AudioSum<bOwner>
	{
	public:
		using IMeasurer = ICompressorMeasurer;

	public:
		Compressor() : bInitialized(false), nch(0)
		{
		}

		Compressor(const Compressor&) = default;
		Compressor(Compressor&&) noexcept = default;
		Compressor& operator=(const Compressor&) = default;
		Compressor& operator=(Compressor&&) noexcept = default;
		~Compressor() noexcept = default;

		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			if (numChannels < this->GetNumChannels() || this->GetNumChannels() == 0)
				return;

			if (!bInitialized)
			{
				if (stereoMode == ECompressorStereoMode::LR)
				{
					channels.resize(this->GetNumChannels());
					sidechains.resize(channels.size());
					for (size_t i = 0; i < channels.size(); ++i)
					{
						channels[i].SetParams(params);
						sidechains[i].resize(bufSize);
					}
				}
				else if (stereoMode == ECompressorStereoMode::M)
				{
					channels.resize(1);
					sidechains.resize(1);
					channels[0].SetParams(params);
					sidechains[0].resize(bufSize);
				}
				else if (stereoMode == ECompressorStereoMode::MS)
				{
					channels.resize(this->GetNumChannels());
					sidechains.resize(channels.size());
					for (size_t i = 0; i < channels.size(); ++i)
					{
						channels[i].SetParams((i & 1) ? sideParams : params);
						sidechains[i].resize(bufSize);
					}
				}
				else
				{
					return;
				}
				bInitialized = true;
			}

			if (this->GetInputSamples(bufs, this->GetNumChannels(), bufSize, sampleRate) != AudioSum<bOwner>::EGetInputSamplesResult::SamplesWritten)
				return;

			if (this->GetNumChannels() == 1 || stereoMode == ECompressorStereoMode::LR)
			{
				for (size_t ch = 0; ch < channels.size(); ++ch)
				{
					for (size_t i = 0; i < bufSize; ++i)
						sidechains[ch][i] = bufs[ch][i].AsFloat32();
					channels[ch].Process(sidechains[ch].data(), bufs[ch], bufSize, sampleRate);
				}
				if (channels.size() == 1)
					for (size_t i = 1; i < numChannels; ++i)
						for (size_t j = 0; j < bufSize; ++j)
							bufs[i][j] = bufs[0][j];
			}
			else
			{
				// Convert to MS
				for (size_t ch = 1; ch < this->GetNumChannels(); ch += 2)
				{
					for (size_t i = 0; i < bufSize; ++i)
					{
						const float m = bufs[ch - 1][i].AsFloat32() + bufs[ch][i].AsFloat32();
						const float s = bufs[ch - 1][i].AsFloat32() - bufs[ch][i].AsFloat32();
						bufs[ch - 1][i] = m;
						bufs[ch][i] = s;
					}
				}

				if (stereoMode == ECompressorStereoMode::M)
				{
					for (size_t i = 0; i < bufSize; ++i)
						sidechains[0][i] = bufs[0][i].AsFloat32();
					channels[0].Process(sidechains[0].data(), bufs[0], bufSize, sampleRate);
				}
				else if (stereoMode == ECompressorStereoMode::MS)
				{
					for (size_t ch = 0; ch < channels.size(); ++ch)
					{
						for (size_t i = 0; i < bufSize; ++i)
							sidechains[ch][i] = bufs[ch][i].AsFloat32();
						channels[ch].Process(sidechains[ch].data(), bufs[ch], bufSize, sampleRate);
					}
				}

				// Convert to LR
				for (size_t ch = 1; ch < this->GetNumChannels(); ch += 2)
				{
					for (size_t i = 0; i < bufSize; ++i)
					{
						const float l = 0.5f*(bufs[ch - 1][i].AsFloat32() + bufs[ch][i].AsFloat32());
						const float r = 0.5f*(bufs[ch - 1][i].AsFloat32() - bufs[ch][i].AsFloat32());
						bufs[ch - 1][i] = l;
						bufs[ch][i] = r;
					}
				}
			}
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			if (nch == 0)
			{
				for (const typename AudioSum<bOwner>::InputPtr& input : this->GetInputs())
				{
					if (auto locked = Utility::Lock(input))
					{
						const size_t inch = locked->GetNumChannels();
						if (inch > nch)
							nch = inch;
					}
				}
			}
			return nch;
		}

		virtual size_t GetSampleDelay() const noexcept override
		{
			return AudioSum<bOwner>::GetSampleDelay() + 256;
		}

		void Measure(IMeasurer& m, const double threshold_db, const double ratio, const double knee_db,
			const size_t nGainCompPts)
		{
			GainComputer gc;
			gc.Measure(m, threshold_db, ratio, knee_db, nGainCompPts);
		}

		void Measure(IMeasurer& m, const double threshold_db, const double ratio, const double knee_db,
			const double attack_ms, const double release_ms)
		{

			CompressorParams testParams;
			testParams.attackSamples = attack_ms * 44.1;
			testParams.releaseSamples = release_ms * 44.1;
			testParams.threshold_db = threshold_db;
			testParams.ratio = ratio;
			testParams.knee_db = knee_db;

			CompressorChannel ch;
			ch.SetParams(testParams);

			Vector<double> scbuf;
			Vector<Sample> iobuf;
			Vector<double> gcbuf;
			Vector<double> tabuf;
			Vector<double> gebuf;
			m.ResetInput();
			while (m.HasMoreInput())
				scbuf.push_back(m.GetNextInput());
			for (size_t i = 0; i < 256; ++i)
				scbuf.push_back(0.0);
			for (const double xval : scbuf)
				iobuf.push_back(Sample(static_cast<float>(xval)));
			Vector<Sample> chInput(iobuf);
			const size_t dataSize = std::max(scbuf.size(), iobuf.size());
			gcbuf.resize(dataSize, 0.0);
			tabuf.resize(dataSize, 0.0);
			gebuf.resize(dataSize, 0.0);
			ch.Process(scbuf.data(), iobuf.data(), iobuf.size(), 44100, gcbuf.data(), tabuf.data(), gebuf.data());
			m.AddChannelMeasurement(chInput, iobuf, gcbuf, tabuf, gebuf);
		}

		void SetParams(const CompressorParams& paramsToSet, const bool bLink)
		{
			params = paramsToSet;
			stereoMode = (bLink) ? ECompressorStereoMode::M : ECompressorStereoMode::LR;
		}

		void SetParams(const CompressorParams& midParamsToSet, const CompressorParams& sideParamsToSet)
		{
			params = midParamsToSet;
			sideParams = sideParamsToSet;
			stereoMode = ECompressorStereoMode::MS;
		}

	private:
		class GainComputer
		{
		public:
			GainComputer() : xm1(0), um1(0) {}
			GainComputer(const GainComputer&) noexcept = default;
			GainComputer(GainComputer&&) noexcept = default;
			GainComputer& operator=(const GainComputer&) noexcept = default;
			GainComputer& operator=(GainComputer&&) noexcept = default;

			double Compute(const double x)
			{
				// ADAA1
				double a[2];
				a[0] = U(std::abs(x));
				a[1] = -a[0];
				const double u = a[x < 0.0];
				const double dx = x - xm1;
				const double du = u - um1;
				xm1 = x;
				um1 = u;
				static constexpr const double tol = 0.0001;
				if (std::abs(dx) < tol)
					return G(0.5*(x + xm1)) - 1.0;
				return du/dx;
			}

			double Compute(const double x, double (&aout)[6])
			{
				// ADAA1
				static constexpr const double tol2 = 0.00000001;
				const double y = G(0.5*(x + xm1));
				double a[2];
				a[0] = U(std::abs(x));
				a[1] = -a[0];
				const double u = a[x < 0.0];
				aout[0] = u;
				aout[2] = y;
				const double dx = x - xm1;
				const double du = u - um1;
				xm1 = x;
				um1 = u;
				if (dx*dx < tol2)
					return y - 1.0;
				return du/dx;
			}

			void SetParams(const double threshold_db, const double ratio, const double knee_db)
			{
				const double T = threshold_db;
				const double R = (ratio > 1.1) ? ratio : 1.1;
				const double K = (knee_db > 0.1) ? knee_db : 0.1;
				const double TOver20 = T*0.05;
				const double KOver40 = K*0.025;
				double TmK = TOver20 - KOver40;
				T_k1 = std::pow(10.0, TmK);
				T_k2 = std::pow(10.0, TOver20 + KOver40);
				const double b = 0.5*(1.0 - R)/(R*K);
				TmK *= 20.0;
				double c = b*TmK;
				const double d = c*TmK;
				c += c;
				c = 1.0 - c;

				const double b40 = 40.0*b;
				const double mb80inv = -1.0/(b40 + b40);
				const double U_b = std::sqrt(ln10()*mb80inv);

				U_k_erfscale = -std::pow(10.0, d*0.05)*sqrtpi()*U_b*std::pow(10.0, c*c*mb80inv);
				XU_k_erfscale = U_k_erfscale*ln10inv();
				erfarg_scale = U_b*b40;
				erfarg_offset = U_b*c;

				W_c_powscale = std::pow(10.0, T*(R - 1.0)/(20.0*R));
				U_c_powscale = R*W_c_powscale;
				XU_c_powscale = U_c_powscale*ln10inv();
				U_c_exp = 1.0/R;

				U_knee_offset = T_k1 - U_k(T_k1);
				U_comp_offset = U_k(T_k2) - U_c(T_k2) + U_knee_offset;

				XU_knee_offset = T_k1*ln10inv() - XU_k(T_k1);
				XU_comp_offset = XU_k(T_k2) - XU_c(T_k2) + XU_knee_offset;

				W_c_exp = U_c_exp - 1.0;
				W_k_powscale = std::pow(10.0, d*0.05);
				W_k_powarg_scale = 20.0*b;
				W_k_powarg_offset = c - 1.0;
			}

			void Measure(IMeasurer& m,
				const double threshold_db, const double ratio, const double knee_db,
				const size_t nGainCompPts)
			{
				SetParams(threshold_db, ratio, knee_db);
				double x;
				const double ninv = 1.0/static_cast<double>(nGainCompPts);
				for (size_t i = 0; i < nGainCompPts; ++i)
				{
					x = static_cast<double>(i + 1)*ninv;
					m.AddGainComputerRow(x, U_k(x), U_c(x), U(x), XU_k(x), XU_c(x), XU(x), W_k(x), W_c(x), G(x));
				}
				m.ResetInput();
				double c[6];
				for (size_t n = 0; m.HasMoreInput(); ++n)
				{
					x = m.GetNextInput();
					const double y = Compute(x, c);
					m.AddGainComputerProcRow(n, x, y, c[0], c[1], c[2], c[3], c[4], c[5]);
				}
			}

		private:
			static double sqrtpi()
			{
				static const double sqrtpi_v = std::sqrt(math::f64::pi);
				return sqrtpi_v;
			}

			static double ln10()
			{
				static const double ln10_v = std::log(10.0);
				return ln10_v;
			}

			static double ln10inv()
			{
				static const double ln10inv_v = 1.0/ln10();
				return ln10inv_v;
			}

			double U_k(const double x)
			{
				return U_k_erfscale*std::erf(erfarg_scale*std::log10(x) + erfarg_offset);
			}

			double XU_k(const double x)
			{
				return XU_k_erfscale*std::erf(erfarg_scale*std::log10(x) + erfarg_offset);
			}

			double U_c(const double x)
			{
				return U_c_powscale*std::pow(x, U_c_exp);
			}

			double XU_c(const double x)
			{
				return XU_c_powscale*std::pow(x, U_c_exp);
			}

			double U(const double x)
			{
				if (x <= T_k1)
					return 0.0;
				if (x <= T_k2)
					return U_k(x) - x + U_knee_offset;
				return U_c(x) - x + U_comp_offset;
			}

			double XU(const double x)
			{
				if (x <= T_k1)
					return 0.0;
				if (x <= T_k2)
					return XU_k(x) - ln10inv()*x + XU_knee_offset;
				return XU_c(x) - ln10inv()*x + XU_comp_offset;
			}

			double W_k(const double x)
			{
				const double log10x = std::log10(x);
				return W_k_powscale*std::pow(x, W_k_powarg_scale*log10x + W_k_powarg_offset);
			}

			double W_c(const double x)
			{
				return W_c_powscale*std::pow(x, W_c_exp);
			}

			double G(const double x_in)
			{
				const double x = std::abs(x_in);
				if (x <= T_k1)
					return 1.0;
				if (x <= T_k2)
					return W_k(x);
				return W_c(x);
			}

		private:
			// Cached calculations from parameters
			double T_k1;
			double T_k2;
			double erfarg_scale;
			double erfarg_offset;
			double U_c_exp;
			double U_k_erfscale;
			double XU_k_erfscale;
			double U_c_powscale;
			double XU_c_powscale;
			double U_knee_offset;
			double U_comp_offset;
			double XU_knee_offset;
			double XU_comp_offset;
			double W_c_powscale;
			double W_c_exp;
			double W_k_powscale;
			double W_k_powarg_scale;
			double W_k_powarg_offset;

			// DSP state
			double xm1;
			double um1;
		};

		class CompressorChannel
		{
		public:
			CompressorChannel()
				: inputDelay{ static_cast<Sample>(0.0f) },
				passThruDelay{ static_cast<Sample>(0.0f) },
				gm1(0.0), vm1(0.0), dryVolume(0.0f), pEnvelopeFilter(tdf2())
			{
			}
			CompressorChannel(const CompressorChannel&) = default;
			CompressorChannel(CompressorChannel&&) noexcept = default;
			CompressorChannel& operator=(const CompressorChannel&) = default;
			CompressorChannel& operator=(CompressorChannel&&) noexcept = default;
			~CompressorChannel() noexcept {}

			void SetParams(const CompressorParams& paramsToSet)
			{
				gc.SetParams(paramsToSet.threshold_db, paramsToSet.ratio, paramsToSet.knee_db);
				gm1 = 0.0;
				vm1 = 0.0;
				b = std::sqrt(paramsToSet.attackSamples + paramsToSet.releaseSamples);
				at2 = paramsToSet.attackSamples*2.0;
				rt2 = paramsToSet.releaseSamples*2.0;
				dryVolume = (paramsToSet.dryVolume_db > -100.0f) ? Utility::DBToGain(paramsToSet.dryVolume_db) : 0.0f;
				pEnvelopeFilter = (paramsToSet.df2) ? df2() : tdf2();
			}

			void Process(double* const scbuf, Sample* const iobuf, const size_t bufSize, const unsigned long sampleRate,
				double* const gcbuf = nullptr, double* const tabuf = nullptr, double* const gebuf = nullptr)
			{
				thread_local Vector<double> workbuf;
				thread_local Vector<double> iobuf_up;
				thread_local Vector<Sample> drySignal;
				workbuf.resize(bufSize << 1);
				iobuf_up.resize(bufSize << 1);
				us_gc.process_unsafe(bufSize, scbuf, workbuf.data());
				for (size_t i = 0; i < workbuf.size(); ++i)
					workbuf[i] = gc.Compute(workbuf[i]);
				ds_gc.process_unsafe(bufSize, workbuf.data(), scbuf);
				if (gcbuf)
					for (size_t i = 0; i < bufSize; ++i)
						gcbuf[i] = scbuf[i];
				if (tabuf)
					for (size_t i = 0; i < bufSize; ++i)
						scbuf[i] = EnvelopeFilter(scbuf[i], tabuf[i]);
				else
					for (size_t i = 0; i < bufSize; ++i)
						scbuf[i] = EnvelopeFilter(scbuf[i]);
				if (gebuf)
					for (size_t i = 0; i < bufSize; ++i)
						gebuf[i] = scbuf[i];
				if (std::abs(dryVolume) > 0.00001) // > -100 dB
				{
					drySignal.resize(bufSize);
					for (size_t i = 0; i < 128; ++i)
					{
						drySignal[i] = dryVolume*passThruDelay[i];
						drySignal[128 + i] = dryVolume*inputDelay[i];
					}
					for (size_t i = 256; i < bufSize; ++i)
						drySignal[i] = dryVolume*iobuf[i - 256];
				}
				Sample atmp[128] = { static_cast<Sample>(0.0f) };
				for (size_t i = 0; i < 128; ++i)
				{
					passThruDelay[i] = iobuf[bufSize - 256 + i];
					atmp[i] = iobuf[bufSize - 128 + i];
					for (size_t j = 256; j - i <= bufSize; j += 128)
						iobuf[bufSize - (j - 128) + i] = iobuf[bufSize - j + i];
				}
				for (size_t i = 0; i < 128; ++i)
				{
					iobuf[i] = inputDelay[i];
					inputDelay[i] = atmp[i];
				}
				us_in.process_unsafe(bufSize, iobuf, iobuf_up.data());
				us_ge.process_unsafe(bufSize, scbuf, workbuf.data());
				for (size_t i = 0; i < workbuf.size(); ++i)
					workbuf[i] = (workbuf[i] + 1.0)*iobuf_up[i];
				ds_rm.process_unsafe(bufSize, workbuf.data(), iobuf);
				if (std::abs(dryVolume) > 0.00001) // > -100 dB
					for (size_t i = 0; i < bufSize; ++i)
						iobuf[i] += drySignal[i];
			}

			double EnvelopeFilterTimeArg(const double x)
			{
				const double bxp = std::pow(b, x);
				return (bxp + 1.0)/(rt2*bxp + at2);
			}

			double EnvelopeFilter(const double x)
			{
				return pEnvelopeFilter->Process(this, x);
			}

			double EnvelopeFilter(const double x, double& fout)
			{
				return pEnvelopeFilter->Process(this, x, fout);
			}

		private:
			class EnvelopeFilterStrategy
			{
			public:
				virtual double Process(CompressorChannel* pThis, const double x) = 0;
				virtual double Process(CompressorChannel* pThis, const double x, double& fout) = 0;
				virtual ~EnvelopeFilterStrategy() noexcept {}
			};

			class EnvelopeFilterTDF2 : public EnvelopeFilterStrategy
			{
			private:
				virtual double Process(CompressorChannel* pThis, const double x) override
				{
					// a1 = (1 - cot(x))/(1 + cot(x)) == tan(x - tau/8)
					static constexpr const double eighthTau = math::f64::pi*0.25;
					const double tanshift = std::tan(pThis->EnvelopeFilterTimeArg(x - pThis->gm1) - eighthTau);
					// b0 = b1 = 1/(1 + cot(x)) == 0.5*tan(x - tau/8) + 0.5
					const double bx = (0.5*tanshift + 0.5)*x;
					const double g = bx + pThis->vm1;
					pThis->vm1 = bx - tanshift*g;
					pThis->gm1 = g;
					return g;
				}

				virtual double Process(CompressorChannel* pThis, const double x, double& fout) override
				{
					// a1 = (1 - cot(x))/(1 + cot(x)) == tan(x - tau/8)
					static constexpr const double eighthTau = math::f64::pi*0.25;
					const double efta = pThis->EnvelopeFilterTimeArg(x - pThis->gm1);
					fout = efta;
					const double tanshift = std::tan(efta - eighthTau);
					// b0 = b1 = 1/(1 + cot(x)) == 0.5*tan(x - tau/8) + 0.5
					const double bx = (0.5*tanshift + 0.5)*x;
					const double g = bx + pThis->vm1;
					pThis->vm1 = bx - tanshift*g;
					pThis->gm1 = g;
					return g;
				}
			};

			class EnvelopeFilterDF2 : public EnvelopeFilterStrategy
			{
			private:
				virtual double Process(CompressorChannel* pThis, const double x) override
				{
					// a1 = (1 - cot(x))/(1 + cot(x)) == tan(x - tau/8)
					static constexpr const double eighthTau = math::f64::pi*0.25;
					const double tanshift = std::tan(pThis->EnvelopeFilterTimeArg(x - pThis->gm1) - eighthTau);
					const double v = x - pThis->vm1*tanshift;
					// b0 = b1 = 1/(1 + cot(x)) == 0.5*tan(x - tau/8) + 0.5
					const double g = (0.5*tanshift + 0.5)*(v + pThis->vm1);
					pThis->vm1 = v;
					pThis->gm1 = g;
					return g;
				}

				virtual double Process(CompressorChannel* pThis, const double x, double& fout) override
				{
					// a1 = (1 - cot(x))/(1 + cot(x)) == tan(x - tau/8)
					static constexpr const double eighthTau = math::f64::pi*0.25;
					const double efta = pThis->EnvelopeFilterTimeArg(x - pThis->gm1);
					fout = efta;
					const double tanshift = std::tan(efta - eighthTau);
					const double v = x - pThis->vm1*tanshift;
					// b0 = b1 = 1/(1 + cot(x)) == 0.5*tan(x - tau/8) + 0.5
					const double g = (0.5*tanshift + 0.5)*(v + pThis->vm1);
					pThis->vm1 = v;
					pThis->gm1 = g;
					return g;
				}
			};

			static EnvelopeFilterStrategy* tdf2()
			{
				static EnvelopeFilterTDF2 eftdf2;
				return &eftdf2;
			}

			static EnvelopeFilterStrategy* df2()
			{
				static EnvelopeFilterDF2 efdf2;
				return &efdf2;
			}

		private:
			oversampling::upsampler441_x2_qsmp<double> us_gc; // ADAA1 in gain computer causes half-sample delay
			oversampling::downsampler441_x2<double> ds_gc;
			oversampling::upsampler441_x2<double> us_in;
			oversampling::upsampler441_x2<double> us_ge;
			oversampling::downsampler441_x2<double> ds_rm;
			GainComputer gc;
			Sample inputDelay[128];
			Sample passThruDelay[128];
			double gm1;
			double vm1;
			double b;
			double at2;
			double rt2;
			float dryVolume;
			EnvelopeFilterStrategy* pEnvelopeFilter;
		};

	private:
		bool bInitialized;
		CompressorParams params;
		CompressorParams sideParams;
		ECompressorStereoMode stereoMode;
		Vector<CompressorChannel> channels;
		Vector<Vector<double>> sidechains;
		mutable size_t nch;
	};
}

