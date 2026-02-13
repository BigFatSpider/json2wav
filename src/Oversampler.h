// Copyright Dan Price 2026.

#pragma once

#include "OversamplerFilters.h"
//#include <iostream>
#include <cstddef>

namespace json2wav::oversampling
{
	struct fastfilts
	{
		template<typename T, size_t n> using filtarr_t = const T[n];

		template<typename T, size_t n>
		static constexpr const T& idxfilt(const filtarr_t<T, n>& filt, const size_t idx)
		{
			return filt[(idx <= (n >> 1)) ? idx : (n - idx)];
		}

		template<typename T, size_t n>
		static constexpr const T& idxfilthb(const filtarr_t<T, n>& filt, const size_t idx)
		{
			return filt[(idx < (n >> 1)) ? idx : (n - 1 - idx)];
		}

		template<typename T, size_t n> struct filt_t
		{
			filt_t(const filtarr_t<T, n>& filtinit) : filt(filtinit) {}
			const T& operator[](const size_t idx) const { return idxfilt(filt, idx); }
			const filtarr_t<T, n>& filt;
		};

		template<typename T, size_t n> struct filthb_t
		{
			filthb_t(const filtarr_t<T, n>& filtinit) : filt(filtinit) {}
			const T& operator[](const size_t idx) const { return idxfilthb(filt, idx); }
			const filtarr_t<T, n>& filt;
		};

		template<typename T> static const filt_t<T, 256>& os441_1to2()
		{
			static const filt_t<T, 256> filt(osfilts::os441_1to2<T>());
			return filt;
		}
		template<typename T> static const filthb_t<T, 24>& os441_2to4hb()
		{
			static const filthb_t<T, 24> filt(osfilts::os441_2to4hb<T>());
			return filt;
		}
		template<typename T> static const filthb_t<T, 16>& os441_4to8hb()
		{
			static const filthb_t<T, 16> filt(osfilts::os441_4to8hb<T>());
			return filt;
		}
		template<typename T> static const filthb_t<T, 16>& os441_8to16hb()
		{
			static const filthb_t<T, 16> filt(osfilts::os441_8to16hb<T>());
			return filt;
		}
		template<typename T> static const filthb_t<T, 16>& os441_16to32hb()
		{
			static const filthb_t<T, 16> filt(osfilts::os441_16to32hb<T>());
			return filt;
		}
	};

	using filts = osfilts;

	template<typename insample_t, typename outsample_t, size_t n>
	inline void interpolate2(
		const insample_t* const inbuf, // size n
		const size_t instride,
		outsample_t (&prevbuf)[n], // size n
		//const sample_t (&filt)[n << 1], // size 2n
		const filts::filt_t<outsample_t, n << 1>& filt,
		outsample_t* const outbuf, // size 2n
		const size_t outstride,
		const size_t nout = n << 1)
	{
		constexpr const size_t twon = n << 1;
		const size_t halfnout = nout >> 1;
		thread_local outsample_t intmp[n] = { 0 };
		for (size_t tmpidx = 0, inidx = 0; tmpidx < halfnout; ++tmpidx, inidx += instride)
			intmp[tmpidx] = inbuf[inidx];
		outsample_t sumbuf[n] = { 0 };
		for (size_t out_n = 0, writeidx = 0; out_n < nout; ++out_n, writeidx += outstride)
		{
			size_t sumidx = 0;
			size_t filtidx = out_n & 1;
			for (size_t inidx = out_n >> 1; inidx > 0; --inidx, filtidx += 2)
				sumbuf[sumidx++] = filt[filtidx] * intmp[inidx];
			sumbuf[sumidx++] = filt[filtidx] * intmp[0];
			filtidx += 2;
			for (size_t previdx = n - 1; previdx > 0 && filtidx < twon; --previdx, filtidx += 2)
				sumbuf[sumidx++] = filt[filtidx] * prevbuf[previdx];
			for (size_t sumstride = 2; sumstride < twon; sumstride <<= 1)
			{
				const size_t pairstride = sumstride >> 1;
				for (size_t pairidx = 0; pairidx + pairstride < n; pairidx += sumstride)
					sumbuf[pairidx] += sumbuf[pairidx + pairstride];
			}
			outbuf[writeidx] = sumbuf[0];
		}

		const size_t nin = nout >> 1;
		const size_t nprev = n - nin;
		for (size_t i = 0; i < nprev; ++i)
			prevbuf[i] = prevbuf[nin + i];
		for (size_t i = 0; i < nin; ++i)
			prevbuf[nprev + i] = intmp[i];

	}

	template<typename sample_t, size_t n>
	inline void interpolatehb(
		const sample_t* const inbuf, // size n
		const size_t instride,
		sample_t (&prevbuf)[n], // size n
		//const sample_t (&filthb)[n], // size n
		const filts::filthb_t<sample_t, n>& filthb,
		sample_t* const outbuf, // size 2n
		const size_t outstride,
		const size_t nout = n << 1)
	{
		constexpr const size_t twon = n << 1;
		constexpr const size_t halfn = n >> 1;
		const size_t halfnout = nout >> 1;
		thread_local sample_t intmp[n] = { 0 };
		for (size_t tmpidx = 0, inidx = 0; tmpidx < halfnout; ++tmpidx, inidx += instride)
			intmp[tmpidx] = inbuf[inidx];
		sample_t sumbuf[n] = { 0 };
		for (size_t out_n = 0, writeidx = 0, combidx = halfn; out_n < nout; ++out_n, writeidx += outstride)
		{
			if (out_n & 1)
			{
				size_t sumidx = 0;
				size_t filtidx = 0;
				for (size_t inidx = out_n >> 1; inidx > 0; --inidx, ++filtidx)
					sumbuf[sumidx++] = filthb[filtidx] * intmp[inidx];
				sumbuf[sumidx++] = filthb[filtidx] * intmp[0];
				++filtidx;
				for (size_t previdx = n - 1; previdx > 0 && filtidx < n; --previdx, ++filtidx)
					sumbuf[sumidx++] = filthb[filtidx] * prevbuf[previdx];
				for (size_t sumstride = 2; sumstride < twon; sumstride <<= 1)
				{
					const size_t pairstride = sumstride >> 1;
					for (size_t pairidx = 0; pairidx + pairstride < n; pairidx += sumstride)
						sumbuf[pairidx] += sumbuf[pairidx + pairstride];
				}
				outbuf[writeidx] = sumbuf[0];
			}
			else
			{
				if (combidx < n)
					outbuf[writeidx] = prevbuf[combidx];
				else
					outbuf[writeidx] = intmp[combidx - n];
				++combidx;
			}
		}

		const size_t nin = nout >> 1;
		const size_t nprev = n - nin;
		for (size_t i = 0; i < nprev; ++i)
			prevbuf[i] = prevbuf[nin + i];
		for (size_t i = 0; i < nin; ++i)
			prevbuf[nprev + i] = intmp[i];

	}

	template<typename insample_t, typename outsample_t, size_t twon>
	inline void decimate2(
		const insample_t* const inbuf, // size 2n
		const size_t instride,
		insample_t (&prevbuf)[twon], // size 2n
		//const sample_t (&filt)[twon], // size 2n
		const filts::filt_t<insample_t, twon>& filt,
		outsample_t* const outbuf, // size n
		const size_t outstride,
		const size_t nout = twon >> 1)
	{
		constexpr const size_t fourn = twon << 1;
		constexpr const insample_t half = 0.5;
		const size_t twonout = nout << 1;
		thread_local insample_t intmp[twon] = { 0 };
		for (size_t tmpidx = 0, inidx = 0; tmpidx < twonout; ++tmpidx, inidx += instride)
			intmp[tmpidx] = inbuf[inidx];
		insample_t sumbuf[twon] = { 0 };
		for (size_t out_n = 0, writeidx = 0; out_n < nout; ++out_n, writeidx += outstride)
		{
			size_t sumidx = 0;
			size_t filtidx = 0;
			for (size_t inidx = out_n << 1; inidx > 0; --inidx, ++filtidx)
				sumbuf[sumidx++] = filt[filtidx] * intmp[inidx];
			sumbuf[sumidx++] = filt[filtidx] * intmp[0];
			++filtidx;
			for (size_t previdx = twon - 1; previdx > 0 && filtidx < twon; --previdx, ++filtidx)
				sumbuf[sumidx++] = filt[filtidx] * prevbuf[previdx];
			for (size_t sumstride = 2; sumstride < fourn; sumstride <<= 1)
			{
				const size_t pairstride = sumstride >> 1;
				for (size_t pairidx = 0; pairidx + pairstride < twon; pairidx += sumstride)
					sumbuf[pairidx] += sumbuf[pairidx + pairstride];
			}
			outbuf[writeidx] = static_cast<outsample_t>(half * sumbuf[0]);
		}

		const size_t nin = nout << 1;
		const size_t nprev = twon - nin;
		for (size_t i = 0; i < nprev; ++i)
			prevbuf[i] = prevbuf[nin + i];
		for (size_t i = 0; i < nin; ++i)
			prevbuf[nprev + i] = intmp[i];

	}

	template<typename sample_t, size_t n>
	inline void decimatehb(
		const sample_t* const inbuf, // size 2n
		const size_t instride,
		sample_t (&prevbuf)[n << 1], // size 2n
		//const sample_t (&filthb)[n], // size n
		const filts::filthb_t<sample_t, n>& filthb,
		sample_t* const outbuf, // size n
		const size_t outstride,
		const size_t nout = n)
	{
		constexpr const size_t twon = n << 1;
		constexpr const sample_t half = 0.5;
		const size_t twonout = nout << 1;
		thread_local sample_t intmp[twon] = { 0 };
		for (size_t tmpidx = 0, inidx = 0; tmpidx < twonout; ++tmpidx, inidx += instride)
			intmp[tmpidx] = inbuf[inidx];
		sample_t sumbuf[n] = { 0 };
		{
			size_t sumidx = 0;
			size_t filtidx = 0;
			for (size_t previdx = twon - 1; filtidx < n; previdx -= 2, ++filtidx)
				sumbuf[sumidx++] = filthb[filtidx] * prevbuf[previdx];
			for (size_t sumstride = 2; sumstride < twon; sumstride <<= 1)
			{
				const size_t pairstride = sumstride >> 1;
				for (size_t pairidx = 0; pairidx + pairstride < n; pairidx += sumstride)
					sumbuf[pairidx] += sumbuf[pairidx + pairstride];
			}
			outbuf[0] = half * (sumbuf[0] + prevbuf[n]);
		}
		for (size_t out_n = 1, writeidx = outstride, combidx = n + 2; out_n < nout; ++out_n, writeidx += outstride, combidx += 2)
		{
			size_t sumidx = 0;
			size_t filtidx = 0;
			for (size_t inidx = (out_n << 1) - 1; inidx > 1; inidx -= 2, ++filtidx)
				sumbuf[sumidx++] = filthb[filtidx] * intmp[inidx];
			sumbuf[sumidx++] = filthb[filtidx] * intmp[1];
			++filtidx;
			for (size_t previdx = twon - 1; filtidx < n; previdx -= 2, ++filtidx)
				sumbuf[sumidx++] = filthb[filtidx] * prevbuf[previdx];
			for (size_t sumstride = 2; sumstride < twon; sumstride <<= 1)
			{
				const size_t pairstride = sumstride >> 1;
				for (size_t pairidx = 0; pairidx + pairstride < n; pairidx += sumstride)
					sumbuf[pairidx] += sumbuf[pairidx + pairstride];
			}
			if (combidx < twon)
				sumbuf[0] += prevbuf[combidx];
			else
				sumbuf[0] += intmp[combidx - twon];
			outbuf[writeidx] = half * sumbuf[0];
		}

		const size_t nin = nout << 1;
		const size_t nprev = twon - nin;
		for (size_t i = 0; i < nprev; ++i)
			prevbuf[i] = prevbuf[nin + i];
		for (size_t i = 0; i < nin; ++i)
			prevbuf[nprev + i] = intmp[i];

	}

	template<typename sample_t, size_t n>
	inline void convolve(
		const sample_t* const inbuf, // size n
		const size_t instride,
		sample_t (&prevbuf)[n], // size n
		//const sample_t (&filt)[n], // size n
		const filts::filt_t<sample_t, n>& filt,
		sample_t* const outbuf, // size n
		const size_t outstride,
		const size_t nout = n)
	{
		constexpr const size_t twon = n << 1;
		thread_local sample_t intmp[n] = { 0 };
		for (size_t tmpidx = 0, inidx = 0; tmpidx < nout; ++tmpidx, inidx += instride)
			intmp[tmpidx] = inbuf[inidx];
		sample_t sumbuf[n] = { 0 };
		for (size_t out_n = 0, writeidx = 0; out_n < nout; ++out_n, writeidx += outstride)
		{
			size_t sumidx = 0;
			size_t filtidx = 0;
			for (size_t inidx = out_n; inidx > 0; --inidx, ++filtidx)
				sumbuf[sumidx++] = filt[filtidx] * intmp[inidx];
			sumbuf[sumidx++] = filt[filtidx] * intmp[0];
			++filtidx;
			for (size_t previdx = n - 1; previdx > 0 && filtidx < n; --previdx, ++filtidx)
				sumbuf[sumidx++] = filt[filtidx] * prevbuf[previdx];
			for (size_t sumstride = 2; sumstride < twon; sumstride <<= 1)
			{
				const size_t pairstride = sumstride >> 1;
				for (size_t pairidx = 0; pairidx + pairstride < n; pairidx += sumstride)
					sumbuf[pairidx] += sumbuf[pairidx + pairstride];
			}
			outbuf[writeidx] = sumbuf[0];
		}

		const size_t nin = nout;
		const size_t nprev = n - nin;
		for (size_t i = 0; i < nprev; ++i)
			prevbuf[i] = prevbuf[nin + i];
		for (size_t i = 0; i < nin; ++i)
			prevbuf[nprev + i] = intmp[i];

	}

#if 0
	template<typename sample_t>
	class oversampler441_x2
	{
	private:
		sample_t buf1to2[128];
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class oversampler441_x4
	{
	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class oversampler441_x8
	{
	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
		sample_t buf4to8[16];
		sample_t buf8to4[32];
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class oversampler441_x16
	{
	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
		sample_t buf4to8[16];
		sample_t buf8to16[16];
		sample_t buf16to8[32];
		sample_t buf8to4[32];
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class oversampler441_x32
	{
	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
		sample_t buf4to8[16];
		sample_t buf8to16[16];
		sample_t buf16to32[16];
		sample_t buf32to16[32];
		sample_t buf16to8[32];
		sample_t buf8to4[32];
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};
#endif

	template<typename sample_t>
	class upsampler441_x2
	{
	public:
		upsampler441_x2() : buf1to2{ 0 } {}

		template<size_t m>
		void process(const sample_t (&inbuf)[m], sample_t (&outbuf)[2*m], const size_t n = m)
		{
			const size_t twon = n << 1;
			size_t instride = 1;
			size_t outstride = 1;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < twon; inoffset += inincr, outoffset += outincr)
				interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride);
			size_t nout = twon - outoffset;
			interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride, nout);
		}

		template<typename insample_t>
		void process_unsafe(const size_t m, const insample_t* const inbuf /*m*/, sample_t* const outbuf /*2m*/)
		{
			const size_t n = m;
			const size_t twon = n << 1;
			size_t instride = 1;
			size_t outstride = 1;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < twon; inoffset += inincr, outoffset += outincr)
				interpolate2(inbuf + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), outbuf + outoffset, outstride);
			size_t nout = twon - outoffset;
			interpolate2(inbuf + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), outbuf + outoffset, outstride, nout);
		}

	private:
		sample_t buf1to2[128];
	};

	template<typename sample_t>
	class upsampler441_x2_qsmp
	{
	public:
		upsampler441_x2_qsmp() : buf1to2{ 0 } {}

		template<size_t m>
		void process(const sample_t (&inbuf)[m], sample_t (&outbuf)[2*m], const size_t n = m)
		{
			const size_t twon = n << 1;
			size_t instride = 1;
			size_t outstride = 1;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < twon; inoffset += inincr, outoffset += outincr)
				interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2_qsmp<sample_t>(), &outbuf[0] + outoffset, outstride);
			size_t nout = twon - outoffset;
			interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2_qsmp<sample_t>(), &outbuf[0] + outoffset, outstride, nout);
		}

		template<typename insample_t>
		void process_unsafe(const size_t m, const insample_t* const inbuf /*m*/, sample_t* const outbuf /*2m*/)
		{
			const size_t n = m;
			const size_t twon = n << 1;
			size_t instride = 1;
			size_t outstride = 1;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < twon; inoffset += inincr, outoffset += outincr)
				interpolate2(inbuf + inoffset, instride, buf1to2, filts::os441_1to2_qsmp<sample_t>(), outbuf + outoffset, outstride);
			size_t nout = twon - outoffset;
			interpolate2(inbuf + inoffset, instride, buf1to2, filts::os441_1to2_qsmp<sample_t>(), outbuf + outoffset, outstride, nout);
		}

	private:
		sample_t buf1to2[128];
	};

	template<typename sample_t>
	class upsampler441_x4
	{
	public:
		upsampler441_x4() : buf1to2{ 0 }, buf2to4{ 0 } {}

		template<size_t m>
		void process(const sample_t (&inbuf)[m], sample_t (&outbuf)[4*m], const size_t n = m)
		{
			const size_t fourn = n << 2;
			size_t instride = 1;
			size_t outstride = 2;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < fourn; inoffset += inincr, outoffset += outincr)
				interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride);
			size_t nout = (fourn - outoffset) >> 1;
			interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*24;
			outincr = outstride*48;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < fourn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = fourn - outoffset;
			interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);
		}

	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
	};

	template<typename sample_t>
	class upsampler441_x8
	{
	public:
		upsampler441_x8() : buf1to2{ 0 }, buf2to4{ 0 }, buf4to8{ 0 } {}

		template<size_t m>
		void process(const sample_t (&inbuf)[m], sample_t (&outbuf)[8*m], const size_t n = m)
		{
			const size_t eightn = n << 3;
			size_t instride = 1;
			size_t outstride = 4;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < eightn; inoffset += inincr, outoffset += outincr)
				interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride);
			size_t nout = (eightn - outoffset) >> 2;
			interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*24;
			outincr = outstride*48;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < eightn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (eightn - outoffset) >> 1;
			interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < eightn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = eightn - outoffset;
			interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);
		}

	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
		sample_t buf4to8[16];
	};

	template<typename sample_t>
	class upsampler441_x16
	{
	public:
		upsampler441_x16() : buf1to2{ 0 }, buf2to4{ 0 }, buf4to8{ 0 }, buf8to16{ 0 } {}

		template<size_t m>
		void process(const sample_t (&inbuf)[m], sample_t (&outbuf)[16*m], const size_t n = m)
		{
			const size_t sixteenn = n << 4;
			size_t instride = 1;
			size_t outstride = 8;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < sixteenn; inoffset += inincr, outoffset += outincr)
				interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride);
			size_t nout = (sixteenn - outoffset) >> 3;
			interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*24;
			outincr = outstride*48;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixteenn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (sixteenn - outoffset) >> 2;
			interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixteenn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (sixteenn - outoffset) >> 1;
			interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixteenn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf8to16, filts::os441_8to16hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = sixteenn - outoffset;
			interpolatehb(&outbuf[0] + inoffset, instride, buf8to16, filts::os441_8to16hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);
		}

	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
		sample_t buf4to8[16];
		sample_t buf8to16[16];
	};

	template<typename sample_t>
	class upsampler441_x32
	{
	public:
		upsampler441_x32() : buf1to2{ 0 }, buf2to4{ 0 }, buf4to8{ 0 }, buf8to16{ 0 }, buf16to32{ 0 } {}

		template<size_t m>
		void process(const sample_t (&inbuf)[m], sample_t (&outbuf)[32*m], const size_t n = m)
		{
			const size_t thirtytwon = n << 5;
			size_t instride = 1;
			size_t outstride = 16;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < thirtytwon; inoffset += inincr, outoffset += outincr)
				interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride);
			size_t nout = (thirtytwon - outoffset) >> 4;
			interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*24;
			outincr = outstride*48;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < thirtytwon; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (thirtytwon - outoffset) >> 3;
			interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < thirtytwon; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (thirtytwon - outoffset) >> 2;
			interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < thirtytwon; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf8to16, filts::os441_8to16hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (thirtytwon - outoffset) >> 1;
			interpolatehb(&outbuf[0] + inoffset, instride, buf8to16, filts::os441_8to16hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < thirtytwon; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf16to32, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = thirtytwon - outoffset;
			interpolatehb(&outbuf[0] + inoffset, instride, buf16to32, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);
		}

	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
		sample_t buf4to8[16];
		sample_t buf8to16[16];
		sample_t buf16to32[16];
	};

	template<typename sample_t>
	class upsampler441_x64
	{
	public:
		upsampler441_x64() : buf1to2{ 0 }, buf2to4{ 0 }, buf4to8{ 0 }, buf8to16{ 0 }, buf16to32{ 0 }, buf32to64{ 0 } {}

		template<size_t m>
		void process(const sample_t (&inbuf)[m], sample_t (&outbuf)[64*m], const size_t n = m)
		{
			const size_t sixtyfourn = n << 6;
			size_t instride = 1;
			size_t outstride = 32;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride);
			size_t nout = (sixtyfourn - outoffset) >> 5;
			interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*24;
			outincr = outstride*48;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (sixtyfourn - outoffset) >> 4;
			interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (sixtyfourn - outoffset) >> 3;
			interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf8to16, filts::os441_8to16hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (sixtyfourn - outoffset) >> 2;
			interpolatehb(&outbuf[0] + inoffset, instride, buf8to16, filts::os441_8to16hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf16to32, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (sixtyfourn - outoffset) >> 1;
			interpolatehb(&outbuf[0] + inoffset, instride, buf16to32, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf32to64, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = sixtyfourn - outoffset;
			interpolatehb(&outbuf[0] + inoffset, instride, buf32to64, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);
		}

	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
		sample_t buf4to8[16];
		sample_t buf8to16[16];
		sample_t buf16to32[16];
		sample_t buf32to64[16];
	};

	template<typename sample_t>
	class upsampler441_x128
	{
	public:
		upsampler441_x128() : buf1to2{ 0 }, buf2to4{ 0 }, buf4to8{ 0 }, buf8to16{ 0 }, buf16to32{ 0 }, buf32to64{ 0 }, buf64to128{ 0 } {}

		template<size_t m>
		void process(const sample_t (&inbuf)[m], sample_t (&outbuf)[128*m], const size_t n = m)
		{
			const size_t bign = n << 7;
			size_t instride = 1;
			size_t outstride = 64;
			size_t inincr = instride*128;
			size_t outincr = outstride*256;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride);
			size_t nout = (bign - outoffset) >> 6;
			interpolate2(&inbuf[0] + inoffset, instride, buf1to2, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*24;
			outincr = outstride*48;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 5;
			interpolatehb(&outbuf[0] + inoffset, instride, buf2to4, filts::os441_2to4hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 4;
			interpolatehb(&outbuf[0] + inoffset, instride, buf4to8, filts::os441_4to8hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf8to16, filts::os441_8to16hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 3;
			interpolatehb(&outbuf[0] + inoffset, instride, buf8to16, filts::os441_8to16hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf16to32, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 2;
			interpolatehb(&outbuf[0] + inoffset, instride, buf16to32, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf32to64, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 1;
			interpolatehb(&outbuf[0] + inoffset, instride, buf32to64, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride >>= 1;
			inincr = instride*16;
			outincr = outstride*32;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				interpolatehb(&outbuf[0] + inoffset, instride, buf64to128, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride);
			nout = bign - outoffset;
			interpolatehb(&outbuf[0] + inoffset, instride, buf64to128, filts::os441_16to32hb<sample_t>(), &outbuf[0] + outoffset, outstride, nout);
		}

	private:
		sample_t buf1to2[128];
		sample_t buf2to4[24];
		sample_t buf4to8[16];
		sample_t buf8to16[16];
		sample_t buf16to32[16];
		sample_t buf32to64[16];
		sample_t buf64to128[16];
	};

	template<typename sample_t>
	class downsampler441_x2
	{
	public:
		downsampler441_x2() : buf2to1{ 0 } {}

		template<size_t m>
		void process(sample_t (&inbuf)[2*m], sample_t (&outbuf)[m], const size_t n = m)
		{
			size_t instride = 1;
			size_t inincr = instride*256;
			size_t outincr = 128;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1);
			size_t nout = n - outoffset;
			decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1, nout);
		}

		template<typename outsample_t>
		void process_unsafe(const size_t m, const sample_t* const inbuf /*2m*/, outsample_t* const outbuf /*m*/)
		{
			const size_t n = m;
			size_t instride = 1;
			size_t inincr = instride*256;
			size_t outincr = 128;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(inbuf + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), outbuf + outoffset, 1);
			size_t nout = n - outoffset;
			decimate2(inbuf + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), outbuf + outoffset, 1, nout);
		}

	private:
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class downsampler441_x2_qsmp
	{
	public:
		downsampler441_x2_qsmp() : buf2to1{ 0 } {}

		template<size_t m>
		void process(sample_t (&inbuf)[2*m], sample_t (&outbuf)[m], const size_t n = m)
		{
			size_t instride = 1;
			size_t inincr = instride*256;
			size_t outincr = 128;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2_qsmp<sample_t>(), &outbuf[0] + outoffset, 1);
			size_t nout = n - outoffset;
			decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2_qsmp<sample_t>(), &outbuf[0] + outoffset, 1, nout);
		}

		template<typename outsample_t>
		void process_unsafe(const size_t m, const sample_t* const inbuf /*2m*/, outsample_t* const outbuf /*m*/)
		{
			const size_t n = m;
			size_t instride = 1;
			size_t inincr = instride*256;
			size_t outincr = 128;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(inbuf + inoffset, instride, buf2to1, filts::os441_1to2_qsmp<sample_t>(), outbuf + outoffset, 1);
			size_t nout = n - outoffset;
			decimate2(inbuf + inoffset, instride, buf2to1, filts::os441_1to2_qsmp<sample_t>(), outbuf + outoffset, 1, nout);
		}

	private:
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class downsampler441_x4
	{
	public:
		downsampler441_x4() : buf4to2{ 0 }, buf2to1{ 0 } {}

		template<size_t m>
		void process(sample_t (&inbuf)[4*m], sample_t (&outbuf)[m], const size_t n = m)
		{
			const size_t fourn = n << 2;
			size_t instride = 1;
			size_t outstride = 2;
			size_t inincr = instride*48;
			size_t outincr = outstride*24;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < fourn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			size_t nout = (fourn - outoffset) >> 1;
			decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			inincr = instride*256;
			outincr = 128;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1);
			nout = n - outoffset;
			decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1, nout);
		}

	private:
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class downsampler441_x8
	{
	public:
		downsampler441_x8() : buf8to4{ 0 }, buf4to2{ 0 }, buf2to1{ 0 } {}

		template<size_t m>
		void process(sample_t (&inbuf)[8*m], sample_t (&outbuf)[m], const size_t n = m)
		{
			const size_t eightn = n << 3;
			size_t instride = 1;
			size_t outstride = 2;
			size_t inincr = instride*32;
			size_t outincr = outstride*16;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < eightn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			size_t nout = (eightn - outoffset) >> 1;
			decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*48;
			outincr = outstride*24;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < eightn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (eightn - outoffset) >> 2;
			decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			inincr = instride*256;
			outincr = 128;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1);
			nout = n - outoffset;
			decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1, nout);
		}

	private:
		sample_t buf8to4[32];
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class downsampler441_x16
	{
	public:
		downsampler441_x16() : buf16to8{ 0 }, buf8to4{ 0 }, buf4to2{ 0 }, buf2to1{ 0 } {}

		template<size_t m>
		void process(sample_t (&inbuf)[16*m], sample_t (&outbuf)[m], const size_t n = m)
		{
			const size_t sixteenn = n << 4;
			size_t instride = 1;
			size_t outstride = 2;
			size_t inincr = instride*32;
			size_t outincr = outstride*16;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < sixteenn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf16to8, filts::os441_8to16hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			size_t nout = (sixteenn - outoffset) >> 1;
			decimatehb(&inbuf[0] + inoffset, instride, buf16to8, filts::os441_8to16hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixteenn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (sixteenn - outoffset) >> 2;
			decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*48;
			outincr = outstride*24;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixteenn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (sixteenn - outoffset) >> 3;
			decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			inincr = instride*256;
			outincr = 128;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1);
			nout = n - outoffset;
			decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1, nout);
		}

	private:
		sample_t buf16to8[32];
		sample_t buf8to4[32];
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class downsampler441_x32
	{
	public:
		downsampler441_x32() : buf32to16{ 0 }, buf16to8{ 0 }, buf8to4{ 0 }, buf4to2{ 0 }, buf2to1{ 0 } {}

		template<size_t m>
		void process(sample_t (&inbuf)[32*m], sample_t (&outbuf)[m], const size_t n = m)
		{
			const size_t thirtytwon = n << 5;
			size_t instride = 1;
			size_t outstride = 2;
			size_t inincr = instride*32;
			size_t outincr = outstride*16;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < thirtytwon; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf32to16, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			size_t nout = (thirtytwon - outoffset) >> 1;
			decimatehb(&inbuf[0] + inoffset, instride, buf32to16, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < thirtytwon; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf16to8, filts::os441_8to16hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (thirtytwon - outoffset) >> 2;
			decimatehb(&inbuf[0] + inoffset, instride, buf16to8, filts::os441_8to16hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < thirtytwon; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (thirtytwon - outoffset) >> 3;
			decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*48;
			outincr = outstride*24;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < thirtytwon; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (thirtytwon - outoffset) >> 4;
			decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			inincr = instride*256;
			outincr = 128;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1);
			nout = n - outoffset;
			decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1, nout);
		}

	private:
		sample_t buf32to16[32];
		sample_t buf16to8[32];
		sample_t buf8to4[32];
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class downsampler441_x64
	{
	public:
		downsampler441_x64() : buf64to32{ 0 }, buf32to16{ 0 }, buf16to8{ 0 }, buf8to4{ 0 }, buf4to2{ 0 }, buf2to1{ 0 } {}

		template<size_t m>
		void process(sample_t (&inbuf)[64*m], sample_t (&outbuf)[m], const size_t n = m)
		{
			const size_t sixtyfourn = n << 6;
			size_t instride = 1;
			size_t outstride = 2;
			size_t inincr = instride*32;
			size_t outincr = outstride*16;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf64to32, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			size_t nout = (sixtyfourn - outoffset) >> 1;
			decimatehb(&inbuf[0] + inoffset, instride, buf64to32, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf32to16, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (sixtyfourn - outoffset) >> 2;
			decimatehb(&inbuf[0] + inoffset, instride, buf32to16, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf16to8, filts::os441_8to16hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (sixtyfourn - outoffset) >> 3;
			decimatehb(&inbuf[0] + inoffset, instride, buf16to8, filts::os441_8to16hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (sixtyfourn - outoffset) >> 4;
			decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*48;
			outincr = outstride*24;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < sixtyfourn; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (sixtyfourn - outoffset) >> 5;
			decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			inincr = instride*256;
			outincr = 128;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1);
			nout = n - outoffset;
			decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1, nout);
		}

	private:
		sample_t buf64to32[32];
		sample_t buf32to16[32];
		sample_t buf16to8[32];
		sample_t buf8to4[32];
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};

	template<typename sample_t>
	class downsampler441_x128
	{
	public:
		downsampler441_x128() : buf128to64{ 0 }, buf64to32{ 0 }, buf32to16{ 0 }, buf16to8{ 0 }, buf8to4{ 0 }, buf4to2{ 0 }, buf2to1{ 0 } {}

		template<size_t m>
		void process(sample_t (&inbuf)[128*m], sample_t (&outbuf)[m], const size_t n = m)
		{
			const size_t bign = n << 7;
			size_t instride = 1;
			size_t outstride = 2;
			size_t inincr = instride*32;
			size_t outincr = outstride*16;
			size_t inoffset = 0;
			size_t outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf128to64, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			size_t nout = (bign - outoffset) >> 1;
			decimatehb(&inbuf[0] + inoffset, instride, buf128to64, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf64to32, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 2;
			decimatehb(&inbuf[0] + inoffset, instride, buf64to32, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf32to16, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 3;
			decimatehb(&inbuf[0] + inoffset, instride, buf32to16, filts::os441_16to32hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf16to8, filts::os441_8to16hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 4;
			decimatehb(&inbuf[0] + inoffset, instride, buf16to8, filts::os441_8to16hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*32;
			outincr = outstride*16;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 5;
			decimatehb(&inbuf[0] + inoffset, instride, buf8to4, filts::os441_4to8hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			outstride <<= 1;
			inincr = instride*48;
			outincr = outstride*24;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < bign; inoffset += inincr, outoffset += outincr)
				decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride);
			nout = (bign - outoffset) >> 6;
			decimatehb(&inbuf[0] + inoffset, instride, buf4to2, filts::os441_2to4hb<sample_t>(), &inbuf[0] + outoffset, outstride, nout);

			instride = outstride;
			inincr = instride*256;
			outincr = 128;
			inoffset = 0;
			outoffset = 0;
			for (; outoffset + outincr < n; inoffset += inincr, outoffset += outincr)
				decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1);
			nout = n - outoffset;
			decimate2(&inbuf[0] + inoffset, instride, buf2to1, filts::os441_1to2<sample_t>(), &outbuf[0] + outoffset, 1, nout);
		}

	private:
		sample_t buf128to64[32];
		sample_t buf64to32[32];
		sample_t buf32to16[32];
		sample_t buf16to8[32];
		sample_t buf8to4[32];
		sample_t buf4to2[48];
		sample_t buf2to1[256];
	};

}

