// Copyright Dan Price. All rights reserved.

#pragma once

#include "IAudioObject.h"
#include "Math.h"
#include "Utility.h"
#include "Memory.h"
#include "AirFilter.h"
#include <random>
#include <type_traits>
#include <cmath>

namespace json2wav
{
	template<size_t n, typename T>
	inline math::matrix::SquareMatrix<n, T> GenRandomOrthonormalBasis()
	{
		using namespace math::matrix;

		// Basis to return
		SquareMatrix<n, T> basis;

		// Initialize normal distribution to generate uniform random unit vectors in any number of dimensions
		std::random_device rd;
		static constexpr const bool bmt64 = sizeof(T) > 4;
		std::conditional_t<bmt64, std::mt19937_64, std::mt19937> mt(rd());
		std::normal_distribution<T> dist(static_cast<T>(0));

		// Generate uniform random unit vectors, orthonormalizing along the way
		T u[n] = { 0 };
		for (size_t i = 0; i < n; ++i)
		{
			for (bool again = true; again;)
			{
				again = false;
				T sumsq = static_cast<T>(0);

				// Generate uniform random unit vector
				for (size_t j = 0; j < n; ++j)
				{
					const T val = dist(mt);
					basis[i][j] = val;
					sumsq += val*val;
				}
				if (sumsq < static_cast<T>(0.00000001))
				{
					// Generated zero vector; try again
					again = true;
					continue;
				}
				T maginv = static_cast<T>(1)/std::sqrt(sumsq);
				for (size_t j = 0; j < n; ++j)
					basis[i][j] *= maginv;

				if (i > 0)
				{
					// Orthogonalize
					for (size_t j = 0; j < n; ++j)
						u[j] = basis[i][j];
					for (size_t k = 0; k < i; ++k)
					{
						// Projection of v onto u is ((v dot u)/(u dot u))*u
						// When u is a unit vector, u dot u = 1, so projection of v onto u is (v dot u)*u
						T proj = static_cast<T>(0);
						for (size_t j = 0; j < n; ++j)
							proj += basis[i][j]*basis[i - 1 - k][j];

						// Subtract projection
						for (size_t j = 0; j < n; ++j)
							u[j] -= proj*basis[i - 1 - k][j];
					}

					// Normalize
					sumsq = static_cast<T>(0);
					for (size_t j = 0; j < n; ++j)
						sumsq += u[j]*u[j];
					if (sumsq < static_cast<T>(0.00000001))
					{
						// Random vector not linearly independent; try again
						again = true;
						continue;
					}
					maginv = static_cast<T>(1)/std::sqrt(sumsq);
					for (size_t j = 0; j < n; ++j)
						basis[i][j] = u[j]*maginv;
				}
			}
		}

		return basis;
	}

	template<size_t n>
	inline math::matrix::ShuffleMatrix<n> GenRandomShuffleMatrix()
	{
		// Initialize RNG
		std::random_device rd;
		static constexpr const bool bmt64 = sizeof(size_t) > 4;
		std::conditional_t<bmt64, std::mt19937_64, std::mt19937> mt(rd());
		using float_t = std::conditional_t<bmt64, double, float>;
		std::uniform_real_distribution<float_t> dist((float_t)0, (float_t)1);
		size_t shuffle[n];
		bool invert[n];
		for (size_t i = 0; i < n; ++i)
			invert[i] = false;
		for (size_t i = 0; i < n; ++i)
		{
			size_t roll = static_cast<size_t>(std::floor(dist(mt)*(n - i)));
			for (size_t j = 0; j <= roll; ++j)
				if (invert[j])
					++roll;
			invert[roll] = true;
			shuffle[i] = roll;
		}
		for (size_t i = 0; i < n; ++i)
			invert[i] = dist(mt) < (float_t)0.5;
		math::matrix::ShuffleMatrix<n> m(shuffle, invert);
		return m;
	}

	template<bool bOwner = false>
	class FDNVerb : public AudioSum<bOwner>
	{
	public:
		FDNVerb(const double rt60Init) : numInputChannels(0), numOutputChannels(2), rt60(rt60Init) {}

		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			if (numInputChannels == 0)
			{
				for (const auto& input : this->GetInputs())
					if (auto locked = Utility::Lock(input); locked && locked->GetNumChannels() > numInputChannels)
						numInputChannels = locked->GetNumChannels();
				if (numInputChannels == 0)
					return;

				if (numOutputChannels > numChannels)
					numOutputChannels = numChannels;

				chs.reserve(numOutputChannels);
				for (size_t i = 0; i < numOutputChannels; ++i)
					chs.emplace_back(rt60);
			}

			if (this->GetInputSamples(bufs, numInputChannels, bufSize, sampleRate) != AudioSum<bOwner>::EGetInputSamplesResult::SamplesWritten)
				return;

			if (numOutputChannels > numInputChannels)
				for (size_t ch = numInputChannels; ch < numOutputChannels; ++ch)
					for (size_t i = 0; i < bufSize; ++i)
						bufs[ch][i] = bufs[ch - numInputChannels][i];

			for (size_t ch = 0; ch < numOutputChannels; ++ch)
				chs[ch].Process(bufs[ch], bufSize, sampleRate);
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return numOutputChannels;
		}

		void SetParams(const size_t numOutChs)
		{
			numOutputChannels = numOutChs;
		}

	private:
		class ReverbChannel
		{
		private:
			class Diffuser
			{
			public:
				Diffuser(const size_t randomDelayMin, const size_t randomDelayMax, const double rt60, const unsigned long sr = 44100)
					: shufmtx(GenRandomShuffleMatrix<8>())
				{
					std::random_device rd;
					static constexpr const bool bmt64 = sizeof(size_t) > 4;
					std::conditional_t<bmt64, std::mt19937_64, std::mt19937> mt(rd());

#if 0
					std::uniform_int_distribution<size_t> dist(randomDelayMin, randomDelayMax);
					Vector<bool> delayCheck(randomDelayMax, false);
					size_t maxdly = randomDelayMin;
					for (size_t i = 0; i < 8; ++i)
					{
						size_t roll = dist(mt);
						while (delayCheck[roll])
							roll = dist(mt);
						delayCheck[roll] = true;
						if (roll > maxdly)
							maxdly = roll;
						delays[i] = roll;
					}
#endif

					const size_t randomDelayRange = randomDelayMax - randomDelayMin;
					const size_t rangeLimits[9] = {
						randomDelayMin,
						randomDelayMin + randomDelayRange/8,
						randomDelayMin + randomDelayRange/4,
						randomDelayMin + randomDelayRange*3/8,
						randomDelayMin + randomDelayRange/2,
						randomDelayMin + randomDelayRange*5/8,
						randomDelayMin + randomDelayRange*3/4,
						randomDelayMin + randomDelayRange*7/8,
						randomDelayMax
					};
					std::uniform_int_distribution<size_t> dists[8] = {
						std::uniform_int_distribution<size_t>(rangeLimits[0], rangeLimits[1]),
						std::uniform_int_distribution<size_t>(rangeLimits[1], rangeLimits[2]),
						std::uniform_int_distribution<size_t>(rangeLimits[2], rangeLimits[3]),
						std::uniform_int_distribution<size_t>(rangeLimits[3], rangeLimits[4]),
						std::uniform_int_distribution<size_t>(rangeLimits[4], rangeLimits[5]),
						std::uniform_int_distribution<size_t>(rangeLimits[5], rangeLimits[6]),
						std::uniform_int_distribution<size_t>(rangeLimits[6], rangeLimits[7]),
						std::uniform_int_distribution<size_t>(rangeLimits[7], rangeLimits[8])
					};
					for (size_t v = 0; v < 8; ++v)
						delays[v] = dists[v](mt);
					const size_t maxdly = delays[7];

					tmpbufs.resize(maxdly);
					dlybufs.resize(maxdly);
					for (size_t i = 0; i < maxdly; ++i)
						for (size_t v = 0; v < 8; ++v)
							dlybufs[i][v] = 0;
					const double dt = 1.0/double(sr);
					for (size_t v = 0; v < 8; ++v)
					{
						const double airtime = dt*double(delays[v]);
						const airfilt::biquad filt = airfilt::get_airfilt((airtime > 0.015) ? airtime : 0.015, sr);
						b0[v] = (float)filt.b0;
						b1[v] = (float)filt.b1;
						b2[v] = (float)filt.b2;
						a1[v] = (float)filt.a1;
						a2[v] = (float)filt.a2;
						const double attenuationDB = -60.0/rt60*airtime;
						attenuation[v] = Utility::DBToGain(attenuationDB);
					}
				}
				Diffuser(const Diffuser&) = default;
				Diffuser(Diffuser&&) noexcept = default;
				Diffuser& operator=(const Diffuser&) = default;
				Diffuser& operator=(Diffuser&&) noexcept = default;
				~Diffuser() noexcept = default;

				void Diffuse(Vector<math::matrix::VerticalVector<8, float>>& work)
				{
					Delay(work);
					Filter(work);
					Shuffle(work);
					Spread(work);
				}

			private:
				void Delay(Vector<math::matrix::VerticalVector<8, float>>& work)
				{
					// delays = { 3, 1, 4, 7, 5, 9, 2, 6 }
					// input = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }
					// input2 = { 1, 2, 3, 4, 5 }
					// maxdly = 9
					// dlybufs = {
					//     { 0,  0,  0,  0,  0,  0,  0,  0,  0 },
					//     { 0,  0,  0,  0,  0,  0,  0,  0,  0 },
					//     { 0,  0,  0,  0,  0,  0,  0,  0,  0 },
					//     { 0,  0,  0,  0,  0,  0,  0,  0,  0 },
					//     { 0,  0,  0,  0,  0,  0,  0,  0,  0 },
					//     { 0,  0,  0,  0,  0,  0,  0,  0,  0 },
					//     { 0,  0,  0,  0,  0,  0,  0,  0,  0 },
					//     { 0,  0,  0,  0,  0,  0,  0,  0,  0 }
					// }
					// tmpbufs[0] = work[16 - 9 + 0]
					// tmpbufs[1] = work[16 - 9 + 1]
					// ...
					// tmpbufs[8] = work[16 - 9 + 8]
					// tmpbufs2[0] = dlybufs[5 + 0]
					// tmpbufs2[1] = dlybufs[5 + 1]
					// tmpbufs2[2] = dlybufs[5 + 2]
					// tmpbufs2[3] = dlybufs[5 + 3]
					// tmpbufs2[4] = work[5 - 9 + 4]
					// tmpbufs2[5] = work[5 - 9 + 5]
					// tmpbufs2[6] = work[5 - 9 + 6]
					// tmpbufs2[7] = work[5 - 9 + 7]
					// tmpbufs2[8] = work[5 - 9 + 8]
					// tmpbufs = {
					//     { 8,  9, 10, 11, 12, 13, 14, 15, 16 },
					//     { 8,  9, 10, 11, 12, 13, 14, 15, 16 },
					//     { 8,  9, 10, 11, 12, 13, 14, 15, 16 },
					//     { 8,  9, 10, 11, 12, 13, 14, 15, 16 },
					//     { 8,  9, 10, 11, 12, 13, 14, 15, 16 },
					//     { 8,  9, 10, 11, 12, 13, 14, 15, 16 },
					//     { 8,  9, 10, 11, 12, 13, 14, 15, 16 },
					//     { 8,  9, 10, 11, 12, 13, 14, 15, 16 }
					// }
					// tmpbufs2 = {
					//     { 0,  0,  0,  0,  1,  2,  3,  4,  5 },
					//     { 0,  0,  0,  0,  1,  2,  3,  4,  5 },
					//     { 0,  0,  0,  0,  1,  2,  3,  4,  5 },
					//     { 0,  0,  0,  0,  1,  2,  3,  4,  5 },
					//     { 0,  0,  0,  0,  1,  2,  3,  4,  5 },
					//     { 0,  0,  0,  0,  1,  2,  3,  4,  5 },
					//     { 0,  0,  0,  0,  1,  2,  3,  4,  5 },
					//     { 0,  0,  0,  0,  1,  2,  3,  4,  5 }
					// }
					// work[15][0] = work[15 - 3][0]
					// work[15][1] = work[15 - 1][1]
					// work[15][2] = work[15 - 4][2]
					// work[15][3] = work[15 - 7][3]
					// work[15][4] = work[15 - 5][4]
					// work[15][5] = work[15 - 9][5]
					// work[15][6] = work[15 - 2][6]
					// work[15][7] = work[15 - 6][7]
					// work[14][0] = work[14 - 3][0]
					// ...
					// work[14][7] = work[14 - 6][7]
					// ...
					// work[9][0] = work[9 - 3][0]
					// ...
					// work[9][7] = work[9 - 6][7]
					// work[8][0] = work[8 - 3][0]
					// work[8][1] = work[8 - 1][1]
					// work[8][2] = work[8 - 4][2]
					// work[8][3] = work[8 - 7][3]
					// work[8][4] = work[8 - 5][4]
					// work[8][5] = dlybufs[9 + 8 - 9][5]
					// work[8][6] = work[8 - 2][6]
					// work[8][7] = work[8 - 6][7]
					// work[7][0] = work[7 - 3][0]
					// work[7][1] = work[7 - 1][1]
					// work[7][2] = work[7 - 4][2]
					// work[7][3] = work[7 - 7][3]
					// work[7][4] = work[7 - 5][4]
					// work[7][5] = dlybufs[9 + 7 - 9][5]
					// work[7][6] = work[7 - 2][6]
					// work[7][7] = work[7 - 6][7]
					// work[6][0] = work[6 - 3][0]
					// work[6][1] = work[6 - 1][1]
					// work[6][2] = work[6 - 4][2]
					// work[6][3] = dlybufs[9 + 6 - 7][3]
					// work[6][4] = work[6 - 5][4]
					// work[6][5] = dlybufs[9 + 6 - 9][5]
					// work[6][6] = work[6 - 2][6]
					// work[6][7] = work[6 - 6][7]
					// ...
					// work[0][0] = dlybufs[9 + 0 - 3][0]
					// work[0][1] = dlybufs[9 + 0 - 1][1]
					// work[0][2] = dlybufs[9 + 0 - 4][2]
					// work[0][3] = dlybufs[9 + 0 - 7][3]
					// work[0][4] = dlybufs[9 + 0 - 5][4]
					// work[0][5] = dlybufs[9 + 0 - 9][5]
					// work[0][6] = dlybufs[9 + 0 - 2][6]
					// work[0][7] = dlybufs[9 + 0 - 6][7]
					// work = {
					//     { 0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13 },
					//     { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
					//     { 0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12 },
					//     { 0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9 },
					//     { 0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11 },
					//     { 0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7 },
					//     { 0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14 },
					//     { 0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10 }
					// }
					// work20 = {
					//     { 0,  0,  0,  1,  2 },
					//     { 0,  1,  2,  3,  4 },
					//     { 0,  0,  0,  0,  1 },
					//     { 0,  0,  0,  0,  0 },
					//     { 0,  0,  0,  0,  0 },
					//     { 0,  0,  0,  0,  0 },
					//     { 0,  0,  1,  2,  3 },
					//     { 0,  0,  0,  0,  0 }
					// }
					// work21 = {
					//     { 3,  4,  5,  6,  7 },
					//     { 5,  6,  7,  8,  9 },
					//     { 2,  3,  4,  5,  6 },
					//     { 0,  0,  1,  2,  3 },
					//     { 1,  2,  3,  4,  5 },
					//     { 0,  0,  0,  0,  1 },
					//     { 4,  5,  6,  7,  8 },
					//     { 0,  1,  2,  3,  4 }
					// }
					// dlybufs = tmpbufs

					const size_t maxdly = dlybufs.size();
					for (size_t i = 0; i < maxdly; ++i)
					{
						const size_t dlyidx = work.size() + i;
						tmpbufs[i] = (dlyidx < maxdly)
							? dlybufs[dlyidx]
							: work[dlyidx - maxdly];
					}
					for (size_t i = work.size(); i > 0; --i)
						for (size_t v = 0; v < 8; ++v)
							work[i-1][v] = (delays[v] > i-1)
								? dlybufs[maxdly + i-1 - delays[v]][v]
								: work[i-1 - delays[v]][v];
					for (size_t i = 0; i < maxdly; ++i)
						dlybufs[i] = tmpbufs[i];

				}

				void Filter(Vector<math::matrix::VerticalVector<8, float>>& work)
				{
					for (auto& frame : work)
					{
						auto output = b0*frame + s1;
						s1 = b1*frame - a1*output + s2;
						s2 = b2*frame - a2*output;
						frame = attenuation*output;
					}
				}

				void Shuffle(Vector<math::matrix::VerticalVector<8, float>>& work)
				{
					for (auto& frame : work)
						frame = shufmtx*frame;
				}

				void Spread(Vector<math::matrix::VerticalVector<8, float>>& work)
				{
					//static constexpr const float sq8inv = static_cast<float>(0.5*math::f64::sq2inv);
					static const math::matrix::HadamardMatrix<8> hadmtx;
					for (auto& frame : work)
						frame = (hadmtx*frame);
				}

			private:
				size_t delays[8];
				Vector<math::matrix::VerticalVector<8, float>> dlybufs;
				Vector<math::matrix::VerticalVector<8, float>> tmpbufs;
				math::matrix::DiagonalMatrix<8, float> b0;
				math::matrix::DiagonalMatrix<8, float> b1;
				math::matrix::DiagonalMatrix<8, float> b2;
				math::matrix::DiagonalMatrix<8, float> a1;
				math::matrix::DiagonalMatrix<8, float> a2;
				math::matrix::VerticalVector<8, float> s1;
				math::matrix::VerticalVector<8, float> s2;
				math::matrix::DiagonalMatrix<8, float> attenuation;
				math::matrix::ShuffleMatrix<8> shufmtx;
			};

		public:
			ReverbChannel(const double rt60)
				: diffuser0(1, 4410, rt60),
				diffuser1(1, 4410, rt60),
				diffuser2(1, 8820, rt60),
				diffuser3(1, 8820, rt60),
				diffuser4(1, 8820, rt60)
			{
				math::matrix::SquareMatrix<8, double> basis(GenRandomOrthonormalBasis<8, double>());
				for (size_t i = 0; i < 8; ++i)
					for (size_t j = 0; j < 8; ++j)
						reflector[i][j] = static_cast<float>(Utility::DBToGain(-12.0/rt60)*basis[i][j]);
			}
			ReverbChannel(const ReverbChannel&) = default;
			ReverbChannel(ReverbChannel&&) noexcept = default;
			ReverbChannel& operator=(const ReverbChannel&) = default;
			ReverbChannel& operator=(ReverbChannel&&) noexcept = default;
			~ReverbChannel() noexcept = default;

			void Process(Sample* const iobuf, const size_t bufSize, const unsigned long sampleRate)
			{
				chwork.resize(bufSize);
				for (size_t i = 0; i < bufSize; ++i)
					for (size_t v = 0; v < 8; ++v)
						chwork[i][v] = 0.125f*iobuf[i].AsFloat32(); // Divide voltage
				Diffuse();
				Echo();
				for (size_t i = 0; i < bufSize; ++i)
				{
					iobuf[i] = chwork[i][0];
					for (size_t v = 1; v < 8; ++v)
						iobuf[i] += chwork[i][v];
				}
			}

		private:
			void Diffuse()
			{
				const size_t bufSize = chwork.size();
				diffused.resize(bufSize);
				for (size_t i = 0; i < bufSize; ++i)
					for (size_t v = 0; v < 8; ++v)
						diffused[i][v] = 0.0f;
				diffuser0.Diffuse(chwork);
				for (size_t i = 0; i < bufSize; ++i)
					diffused[i] += chwork[i];
				diffuser1.Diffuse(chwork);
				for (size_t i = 0; i < bufSize; ++i)
					diffused[i] += chwork[i];
				diffuser2.Diffuse(chwork);
				for (size_t i = 0; i < bufSize; ++i)
					diffused[i] += chwork[i];
				diffuser3.Diffuse(chwork);
				for (size_t i = 0; i < bufSize; ++i)
					diffused[i] += chwork[i];
				diffuser4.Diffuse(chwork);
				for (size_t i = 0; i < bufSize; ++i)
					chwork[i] += diffused[i];
			}

			void Echo()
			{

				static constexpr const float lpb0 = float(airfilt::lp200ms::b0);
				static constexpr const float lpb1 = float(airfilt::lp200ms::b1);
				static constexpr const float lpb2 = float(airfilt::lp200ms::b2);
				static constexpr const float lpa1 = float(airfilt::lp200ms::a1);
				static constexpr const float lpa2 = float(airfilt::lp200ms::a2);

				static constexpr const float hsb0 = float(airfilt::hs200ms::b0);
				static constexpr const float hsb1 = float(airfilt::hs200ms::b1);
				static constexpr const float hsb2 = float(airfilt::hs200ms::b2);
				static constexpr const float hsa1 = float(airfilt::hs200ms::a1);
				static constexpr const float hsa2 = float(airfilt::hs200ms::a2);

				auto airfilter200ms = [this](math::matrix::VerticalVector<8, float>& frame)
				{
					auto output = lpb0*frame + lps1;
					lps1 = lpb1*frame - lpa1*output + lps2;
					lps2 = lpb2*frame - lpa2*output;
					frame = output;
					output = hsb0*frame + hss1;
					hss1 = hsb1*frame - hsa1*output + hss2;
					hss2 = hsb2*frame - hsa2*output;
					frame = output;
				};

				if (delaybuf.size() == 0)
				{
					std::random_device rd;
					static constexpr const bool bmt64 = sizeof(size_t) > 4;
					std::conditional_t<bmt64, std::mt19937_64, std::mt19937> mt(rd());
					static constexpr const size_t randomDelayMin = 8820 - 88; // 198ms
					static constexpr const size_t randomDelayMax = 8820 + 89; // 202ms
					std::uniform_int_distribution<size_t> dist(randomDelayMin, randomDelayMax);
					Vector<bool> delayCheck(randomDelayMax, false);
					size_t echotime = randomDelayMin;
					for (size_t i = 0; i < 8; ++i)
					{
						delays[i] = dist(mt);
						while (delayCheck[delays[i]])
							delays[i] = dist(mt);
						delayCheck[delays[i]] = true;
						if (delays[i] > echotime)
							echotime = delays[i];
					}
					delaybuf.resize(echotime);
					//tmpbuf.resize(echotime);
					for (size_t i = 0; i < echotime; ++i)
						for (size_t v = 0; v < 8; ++v)
							delaybuf[i][v] = 0;
					//delayline.resize(chwork.size());
				}

				for (size_t i = 0; i < chwork.size(); ++i)
				{
					math::matrix::VerticalVector<8, float> frame;
					for (size_t v = 0; v < 8; ++v)
						frame[v] = (i < delays[v]) ? delaybuf[delaybuf.size() + i - delays[v]][v] : chwork[i - delays[v]][v];
					frame = reflector*frame;
					airfilter200ms(frame);
					chwork[i] += frame;
				}
				for (size_t i = 0; i < delaybuf.size(); ++i)
				{
					delaybuf[i] = (chwork.size() + i < delaybuf.size()) ? delaybuf[chwork.size() + i] : chwork[chwork.size() + i - delaybuf.size()];
				}

#if 0
				// For each sample:
				// 1. Add delay output
				// 2. Copy to output and to delay input

				// Copy current signal
				for (size_t i = 0; i < chwork.size(); ++i)
					delayline[i] = chwork[i];

				// Delay copied signal
				const size_t maxdly = delaybuf.size();
				for (size_t i = 0; i < maxdly; ++i)
				{
					const size_t dlyidx = delayline.size() + i;
					tmpbuf[i] = (dlyidx < maxdly)
						? delaybuf[dlyidx]
						: delayline[dlyidx - maxdly];
				}
				for (size_t i = delayline.size(); i > 0; --i)
					for (size_t v = 0; v < 8; ++v)
						delayline[i-1][v] = (delays[v] > i-1)
							? delaybuf[maxdly + i-1 - delays[v]][v]
							: delayline[i-1 - delays[v]][v];
				for (size_t i = 0; i < maxdly; ++i)
					delaybuf[i] = tmpbuf[i];

				// Filter/reflect copied, delayed signal and add to current signal
				for (size_t i = 0; i < chwork.size(); ++i)
				{
					auto frame = reflector*delayline[i];
					airfilter200ms(frame);
					chwork[i] += frame;
				}

				for (size_t i = 0, end = std::min(delaybuf.size(), chwork.size()); i < end; ++i)
					chwork[i] += delaybuf[i];

				const size_t echotime = delaybuf.size();
				for (size_t i = echotime; i < chwork.size(); ++i)
				{
					auto frame = reflector*chwork[i - echotime];
					airfilter200ms(frame);
					chwork[i] += frame;
				}

				for (size_t i = 0; i < echotime; ++i)
				{
					if (chwork.size() + i < echotime)
					{
						delaybuf[i] = delaybuf[chwork.size() + i];
					}
					else
					{
						auto frame = reflector*chwork[chwork.size() + i - echotime];
						airfilter200ms(frame);
						delaybuf[i] = frame;
					}
				}
#endif

			}

		private:
			size_t delays[8];
			Vector<math::matrix::VerticalVector<8, float>> chwork;
			Vector<math::matrix::VerticalVector<8, float>> diffused;
			//Vector<math::matrix::VerticalVector<8, float>> delayline;
			Vector<math::matrix::VerticalVector<8, float>> delaybuf;
			//Vector<math::matrix::VerticalVector<8, float>> tmpbuf;
			Diffuser diffuser0;
			Diffuser diffuser1;
			Diffuser diffuser2;
			Diffuser diffuser3;
			Diffuser diffuser4;
			math::matrix::VerticalVector<8, float> lps1;
			math::matrix::VerticalVector<8, float> lps2;
			math::matrix::VerticalVector<8, float> hss1;
			math::matrix::VerticalVector<8, float> hss2;
			math::matrix::SquareMatrix<8, float> reflector;
		};

	private:
		size_t numInputChannels;
		size_t numOutputChannels;
		double rt60;
		Vector<ReverbChannel> chs;
	};
}

