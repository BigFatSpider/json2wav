// Copyright Dan Price 2026.

#pragma once

#include "FastSin.h"
#include <new>
#include <stdexcept>
#include <random>
#include <mutex>
#include <cstdint>
#include <cstdlib>
#include <cmath>

#define INT24_MAX static_cast<int32_t>(8388607)
#define INT24_MIN static_cast<int32_t>(-8388608)

namespace json2wav
{
	class OutOfSampleMemory {};

	enum class ESampleType : uint8_t
	{
		Int16, Int24, Float32
	};

	constexpr inline size_t GetSampleSize(const ESampleType sampleType)
	{
		return (sampleType == ESampleType::Int16) ? 2 : (sampleType == ESampleType::Int24) ? 3 : 4;
	}

	inline float dither()
	{
		static std::random_device rd;
		static std::mt19937 mt(rd());
		static std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
		return dist(mt);
	}

	struct Sample
	{
	public:
		Sample() : data(0.0f) {}
		Sample(const Sample& other) : data(other.data) {}
		Sample(Sample&& other) noexcept : data(other.data) {}
		explicit Sample(const float dataInit) : data(dataInit) {}

		int16_t AsInt16() const
		{
			return static_cast<int16_t>((data > 1.0f) ? INT16_MAX :
			   (data < -1.0f) ? INT16_MIN : std::round(data *
				   (static_cast<float>(INT16_MAX) + 0.49f) - 0.5f + dither()));
		}

		int32_t AsInt24() const
		{
			return static_cast<int32_t>((data > 1.0f) ? INT24_MAX :
			   (data < -1.0f) ? INT24_MIN : std::round(data *
				  (static_cast<float>(INT24_MAX) + 0.49f) - 0.5f + dither()));
		}

		float AsFloat32() const
		{
			return data;
		}

		operator float() const
		{
			return data;
		}

		Sample& operator=(const float value)
		{
			data = value;
			return *this;
		}
		Sample& operator+=(const float value)
		{
			data += value;
			return *this;
		}
		Sample& operator-=(const float value)
		{
			data -= value;
			return *this;
		}
		Sample& operator*=(const float value)
		{
			data *= value;
			return *this;
		}
		Sample& operator/=(const float value)
		{
			data /= value;
			return *this;
		}

		Sample& operator=(const Sample& other)
		{
			data = other.data;
			return *this;
		}
		Sample& operator+=(const Sample& other)
		{
			data += other.data;
			return *this;
		}
		Sample& operator-=(const Sample& other)
		{
			data -= other.data;
			return *this;
		}
		Sample& operator*=(const Sample& other)
		{
			data *= other.data;
			return *this;
		}
		Sample& operator/=(const Sample& other)
		{
			data /= other.data;
			return *this;
		}

		Sample& operator=(Sample&& other) noexcept
		{
			data = other.data;
			return *this;
		}

	private:
		float data;
	};

	class SampleBuf
	{
	private:
		static Sample* salloc(const size_t numSamples);
		static void sfree(Sample* const mem) noexcept;
		static Sample** challoc(const size_t numChannels);
		static void chfree(Sample** const mem) noexcept;

		static std::mutex allocmtx;

		static Sample** InitializeBufs(const size_t bufSize, const size_t numChannels)
		{
			Sample** const bufs = [numChannels]() { std::scoped_lock lock(allocmtx); return challoc(numChannels); }();
			if (bufs)
			{
				for (size_t ch = 0; ch < numChannels; ++ch)
				{
					Sample* const buf = [bufSize]() { std::scoped_lock lock(allocmtx); return salloc(bufSize); }();
					if (buf)
						for (size_t i = 0; i < bufSize; ++i)
							new (buf + i) Sample();
					bufs[ch] = buf;
				}
			}
			return bufs;
		}

	public:
		SampleBuf() noexcept
			: bufs(nullptr), numChannels(0), bufSize(0), bInitialized(false), bZeroOnReinit(true)
		{
		}

		SampleBuf(const SampleBuf& other)
			: bufs((other.bInitialized && other.bufs) ? InitializeBufs(other.bufSize, other.numChannels) : nullptr),
			numChannels((bufs) ? other.numChannels : 0),
			bufSize((bufs) ? other.bufSize : 0),
			bInitialized(other.bInitialized),
			bZeroOnReinit(other.bZeroOnReinit)
		{
			if (bufs)
			{
				for (size_t ch = 0; ch < numChannels; ++ch)
				{
					Sample* const buf = bufs[ch];
					const Sample* const otherBuf = other.bufs[ch];
					for (size_t smpnum = 0; smpnum < bufSize; ++smpnum)
						buf[smpnum] = otherBuf[smpnum];
				}
			}
		}

		SampleBuf(SampleBuf&& other) noexcept
			: bufs(other.bufs),
			numChannels(other.numChannels),
			bufSize(other.bufSize),
			bInitialized(other.bInitialized),
			bZeroOnReinit(other.bZeroOnReinit)
		{
			other.bufs = nullptr;
		}

		explicit SampleBuf(const size_t numChannelsInit, const size_t bufSizeInit, const bool bZeroOnReinitInit = true)
			: bufs(InitializeBufs(bufSizeInit, numChannelsInit)),
			numChannels((bufs) ? numChannelsInit : 0),
			bufSize((bufs) ? bufSizeInit : 0),
			bInitialized(true),
			bZeroOnReinit(bZeroOnReinitInit)
		{
		}

		SampleBuf& operator=(const SampleBuf& other)
		{
			if (&other != this)
			{
				if (numChannels != other.numChannels || bufSize != other.bufSize || !other.bInitialized)
					Destroy();

				if (!bInitialized && other.bInitialized && other.bufs)
					Initialize(other.numChannels, other.bufSize);

				bZeroOnReinit = other.bZeroOnReinit;

				for (size_t ch = 0; ch < numChannels; ++ch)
				{
					Sample* const buf = bufs[ch];
					const Sample* const otherBuf = other.bufs[ch];
					for (size_t smpnum = 0; smpnum < bufSize; ++smpnum)
						buf[smpnum] = otherBuf[smpnum];
				}
			}

			return *this;
		}

		SampleBuf& operator=(SampleBuf&& other) noexcept
		{
			if (&other != this)
			{
				Destroy();
				bufs = other.bufs;
				numChannels = other.numChannels;
				bufSize = other.bufSize;
				bInitialized = other.bInitialized;
				bZeroOnReinit = other.bZeroOnReinit;
				other.bufs = nullptr;
			}
			return *this;
		}

		void Initialize(const size_t numChannelsInit, const size_t bufSizeInit)
		{
			if (bInitialized)
				return;

			if (!bufs)
			{
				bufs = InitializeBufs(bufSizeInit, numChannelsInit);
				numChannels = (bufs) ? numChannelsInit : 0;
				bufSize = (bufs) ? bufSizeInit : 0;
			}
			bInitialized = !!bufs;
		}

		void Reinitialize(const size_t numChannelsInit, const size_t bufSizeInit)
		{
			if (bInitialized)
			{
				if (numChannelsInit == numChannels && bufSizeInit == bufSize)
				{
					if (bZeroOnReinit)
						zero();
					return;
				}
				Destroy();
			}
			Initialize(numChannelsInit, bufSizeInit);
		}

		bool Initialized() const noexcept { return bInitialized; }

		Sample** get() noexcept { return bufs; }
		const Sample* const* get() const noexcept { return bufs; }

		Sample* GetChannel(const size_t chnum) noexcept
		{
			return (chnum < numChannels) ? bufs[chnum] : nullptr;
		}
		const Sample* GetChannel(const size_t chnum) const noexcept
		{
			return (chnum < numChannels) ? bufs[chnum] : nullptr;
		}

		Sample& GetSample(const size_t chnum, const size_t smpnum)
		{
			if (chnum >= numChannels || smpnum >= bufSize)
				throw std::out_of_range("SampleBuf::getsample: Index out of range");
			return bufs[chnum][smpnum];
		}
		const Sample& GetSample(const size_t chnum, const size_t smpnum) const
		{
			if (chnum >= numChannels || smpnum >= bufSize)
				throw std::out_of_range("SampleBuf::getsample: Index out of range");
			return bufs[chnum][smpnum];
		}

		size_t size() const noexcept { return bufSize * numChannels; }

		size_t GetBufSize() const noexcept { return bufSize; }
		size_t GetNumChannels() const noexcept { return numChannels; }

		Sample* operator[](const size_t idx) noexcept
		{
			if (idx >= numChannels)
			{
				return nullptr;
			}
			return bufs[idx];
		}

		const Sample* operator[](const size_t idx) const noexcept
		{
			if (idx >= numChannels)
			{
				return nullptr;
			}
			return bufs[idx];
		}

		void zero() noexcept
		{
			for (size_t ch = 0; ch < numChannels; ++ch)
			{
				Sample* buf(bufs[ch]);
				for (size_t idx = 0; idx < bufSize; ++idx)
					buf[idx] = 0.0f;
			}
		}

		void SetZeroOnReinit(const bool bZeroOnReinitVal) noexcept
		{
			bZeroOnReinit = bZeroOnReinitVal;
		}

	private:
		void Destroy() noexcept
		{
			if (bufs)
			{
				for (size_t ch = 1; ch <= numChannels; ++ch)
				{
					Sample* const buf = bufs[numChannels - ch];
					for (size_t i = 1; i <= bufSize; ++i)
						(buf + bufSize - i)->~Sample();
				}
				{
					std::scoped_lock lock(allocmtx);
					for (size_t ch = 1; ch <= numChannels; ++ch)
						sfree(bufs[numChannels - ch]);
					chfree(bufs);
				}
				bufs = nullptr;
			}
			numChannels = 0;
			bufSize = 0;
			bInitialized = false;
		}

	public:
		~SampleBuf() noexcept
		{
			Destroy();
		}

	private:
		Sample** bufs;
		size_t numChannels;
		size_t bufSize;
		bool bInitialized;
		bool bZeroOnReinit;
	};

	constexpr const size_t sampleChunkSize = 16 * 1024;
	constexpr const size_t sampleChunkNum = sampleChunkSize / sizeof(Sample);
	static_assert(sizeof(Sample) == 4, "Sample is too big");
}

