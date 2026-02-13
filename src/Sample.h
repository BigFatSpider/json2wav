// Copyright Dan Price. All rights reserved.

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

	struct Sample_old
	{
		// ctors
		Sample_old() noexcept {}
		Sample_old(const Sample_old& other) noexcept { *this = other; }
		Sample_old(Sample_old&& other) noexcept { *this = other; }
		explicit Sample_old(const int16_t val) noexcept : type(ESampleType::Int16) { value.i16 = val; }
		explicit Sample_old(const int32_t val) noexcept : type(ESampleType::Int24)
		{
			if (val > INT24_MAX)
			{
				value.i24 = INT24_MAX;
			}
			else if (val < INT24_MIN)
			{
				value.i24 = INT24_MIN;
			}
			else
			{
				value.i24 = val;
			}
		}
		explicit Sample_old(const float val) noexcept : type(ESampleType::Float32) { value.f32 = val; }

		// dtor
		~Sample_old() noexcept {}

	private:
		// data members
		union
		{
			int16_t i16;
			int32_t i24;
			float f32;
		} value;
		ESampleType type;

	public:
		// assignment
		Sample_old& operator=(const Sample_old& other) noexcept
		{
			switch (other.type)
			{
			case ESampleType::Int16: value.i16 = other.value.i16; break;
			case ESampleType::Int24: value.i24 = other.value.i24; break;
			case ESampleType::Float32: value.f32 = other.value.f32; break;
			default: return *this;
			}

			type = other.type;

			return *this;
		}

		Sample_old& operator=(Sample_old&& other) noexcept
		{
			return *this = other;
		}

		Sample_old& operator=(const int16_t val) noexcept
		{
			value.i16 = val;
			type = ESampleType::Int16;
			return *this;
		}

		Sample_old& operator=(const int32_t val) noexcept
		{
			if (val > INT24_MAX)
			{
				value.i24 = INT24_MAX;
			}
			else if (val < INT24_MIN)
			{
				value.i24 = INT24_MIN;
			}
			else
			{
				value.i24 = val;
			}

			type = ESampleType::Int24;

			return *this;
		}

		Sample_old& operator=(const float val) noexcept
		{
			value.f32 = val;
			type = ESampleType::Float32;
			return *this;
		}

		Sample_old& operator+=(const Sample_old& other) noexcept
		{
			//if (type == other.type)
			//{
				switch (type)
				{
				case ESampleType::Int16:
					{
						const int32_t sum = static_cast<int32_t>(value.i16) + static_cast<int32_t>(other.AsInt16());
						if (sum > static_cast<int32_t>(INT16_MAX))
						{
							value.i16 = INT16_MAX;
						}
						else if (sum < static_cast<int32_t>(INT16_MIN))
						{
							value.i16 = INT16_MIN;
						}
						else
						{
							value.i16 = static_cast<int16_t>(sum);
						}
					} break;
				case ESampleType::Int24:
					{
						const int32_t sum = value.i24 + other.AsInt24();
						if (sum > INT24_MAX)
						{
							value.i24 = INT24_MAX;
						}
						else if (sum < INT24_MIN)
						{
							value.i24 = INT24_MIN;
						}
						else
						{
							value.i24 = sum;
						}
					} break;
				case ESampleType::Float32: value.f32 += other.AsFloat32(); break;
				default: break;
				}
			//}

			return *this;
		}

		Sample_old& operator+=(const int16_t otherValue) noexcept { return *this += Sample_old(otherValue); }
		Sample_old& operator+=(const int32_t otherValue) noexcept { return *this += Sample_old(otherValue); }
		Sample_old& operator+=(const float otherValue) noexcept { return *this += Sample_old(otherValue); }

		Sample_old& operator-=(const Sample_old& other) noexcept
		{
			//if (type == other.type)
			//{
				switch (type)
				{
				case ESampleType::Int16:
					{
						const int32_t diff = static_cast<int32_t>(value.i16) - static_cast<int32_t>(other.AsInt16());
						if (diff > static_cast<int32_t>(INT16_MAX))
						{
							value.i16 = INT16_MAX;
						}
						else if (diff < static_cast<int32_t>(INT16_MIN))
						{
							value.i16 = INT16_MIN;
						}
						else
						{
							value.i16 = static_cast<int16_t>(diff);
						}
					} break;
				case ESampleType::Int24:
					{
						const int32_t diff = value.i24 - other.AsInt24();
						if (diff > INT24_MAX)
						{
							value.i24 = INT24_MAX;
						}
						else if (diff < INT24_MIN)
						{
							value.i24 = INT24_MIN;
						}
						else
						{
							value.i24 = diff;
						}
					} break;
				case ESampleType::Float32: value.f32 -= other.AsFloat32(); break;
				default: break;
				}
			//}

			return *this;
		}

		Sample_old& operator-=(const int16_t otherValue) noexcept { return *this -= Sample_old(otherValue); }
		Sample_old& operator-=(const int32_t otherValue) noexcept { return *this -= Sample_old(otherValue); }
		Sample_old& operator-=(const float otherValue) noexcept { return *this -= Sample_old(otherValue); }

		Sample_old& operator*=(const Sample_old& other) noexcept
		{
			switch (other.type)
			{
			case ESampleType::Int16: return *this *= other.value.i16;
			case ESampleType::Int24: return *this *= other.value.i24;
			case ESampleType::Float32: return *this *= other.value.f32;
			default: break;
			}
			return *this;
		}

		Sample_old& operator*=(const int16_t otherValue) noexcept { return *this = AsInt16() * otherValue; }
		Sample_old& operator*=(const int32_t otherValue) noexcept { return *this = AsInt24() * otherValue; }
		Sample_old& operator*=(const float otherValue) noexcept { return *this = AsFloat32() * otherValue; }

		Sample_old& operator/=(const Sample_old& other) noexcept
		{
			switch (other.type)
			{
			case ESampleType::Int16: return *this /= other.value.i16;
			case ESampleType::Int24: return *this /= other.value.i24;
			case ESampleType::Float32: return *this /= other.value.f32;
			default: break;
			}
			return *this;
		}

		Sample_old& operator/=(const int16_t otherValue) noexcept { return *this = AsInt16() / otherValue; }
		Sample_old& operator/=(const int32_t otherValue) noexcept { return *this = AsInt24() / otherValue; }
		Sample_old& operator/=(const float otherValue) noexcept { return *this = AsFloat32() / otherValue; }

		// conversion
		int16_t AsInt16() const noexcept
		{
			switch (type)
			{
			case ESampleType::Int16: return value.i16;
			case ESampleType::Int24: return Sample_old(AsFloat32()).AsInt16(); // Convert to float 32 for dither
				//return static_cast<int16_t>(((value.i24 & 0xffffff00) >> 8) |
				//							 ((value.i24 & 0x80000000) ? 0xff000000 : 0));
			case ESampleType::Float32: return static_cast<int16_t>((value.f32 > 1.0f) ? INT16_MAX :
											   (value.f32 < -1.0f) ? INT16_MIN : std::round(value.f32 *
												   (static_cast<float>(INT16_MAX) + 0.49f) - 0.5f + dither()));
			/*case ESampleType::Float32:
				{
					if (value.f32 > 1.0f)
						return INT16_MAX;
					if (value.f32 < -1.0f)
						return INT16_MIN;
					uint32_t fBits;
					const char* const src(reinterpret_cast<const char*>(&value.f32));
					char* const dest(reinterpret_cast<char*>(&fBits));
					dest[0] = src[0];
					dest[1] = src[1];
					dest[2] = src[2];
					dest[3] = src[3];
					return static_cast<int16_t>(std::round(value.f32 *
						(static_cast<float>(INT16_MAX) + (fBits >> 31)) + dither()));
				}*/
			}
			return 0;
		}

		int32_t AsInt24() const noexcept
		{
			switch (type)
			{
			case ESampleType::Int16: return static_cast<int32_t>(value.i16) << 8;
			case ESampleType::Int24: return value.i24;
			case ESampleType::Float32: return static_cast<int32_t>((value.f32 > 1.0f) ? INT24_MAX :
											   (value.f32 < -1.0f) ? INT24_MIN : std::round(value.f32 *
												  (static_cast<float>(INT24_MAX) + 0.49f) - 0.5f + dither()));
			/*case ESampleType::Float32:
				{
					if (value.f32 > 1.0f)
						return INT24_MAX;
					if (value.f32 < -1.0f)
						return INT24_MIN;
					uint32_t fBits;
					const char* const src(reinterpret_cast<const char*>(&value.f32));
					char* const dest(reinterpret_cast<char*>(&fBits));
					dest[0] = src[0];
					dest[1] = src[1];
					dest[2] = src[2];
					dest[3] = src[3];
					return static_cast<int32_t>(std::round(value.f32 *
						(static_cast<float>(INT24_MAX) + (fBits >> 31)) + dither()));
				}*/
			}
			return 0;
		}

		float AsFloat32() const noexcept
		{
			switch (type)
			{
			case ESampleType::Int16: return (static_cast<float>(value.i16) + 0.5f) / (static_cast<float>(INT16_MAX) + 0.5f);
			case ESampleType::Int24: return (static_cast<float>(value.i24) + 0.5f) / (static_cast<float>(INT24_MAX) + 0.5f);
			//case ESampleType::Int16: return static_cast<float>(value.i16) / (static_cast<float>(INT16_MAX) + (value.i16 >> 15));
			//case ESampleType::Int24: return static_cast<float>(value.i24) / (static_cast<float>(INT24_MAX) + ((value.i24 >> 23) & 1));
			case ESampleType::Float32: return value.f32;
			}
			return 0.0f;
		}
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
			//Sample* buf = static_cast<Sample*>(salloc(bufSize * sizeof(Sample)));
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
			: bufs((other.bInitialized && other.bufs) ? InitializeBufs(other.bufSize, other.numChannels) : nullptr)
			, numChannels((bufs) ? other.numChannels : 0)
			, bufSize((bufs) ? other.bufSize : 0)
			, bInitialized(other.bInitialized)
			, bZeroOnReinit(other.bZeroOnReinit)
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
			: bufs(other.bufs)
			, numChannels(other.numChannels)
			, bufSize(other.bufSize)
			, bInitialized(other.bInitialized)
			, bZeroOnReinit(other.bZeroOnReinit)
		{
			other.bufs = nullptr;
		}

		explicit SampleBuf(const size_t numChannelsInit, const size_t bufSizeInit, const bool bZeroOnReinitInit = true)
			: bufs(InitializeBufs(bufSizeInit, numChannelsInit))
			, numChannels((bufs) ? numChannelsInit : 0)
			, bufSize((bufs) ? bufSizeInit : 0)
			, bInitialized(true)
			, bZeroOnReinit(bZeroOnReinitInit)
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
				//throw std::out_of_range("SampleBuf::operator[]: Index out of range");
				return nullptr;
			}
			return bufs[idx];
		}

		const Sample* operator[](const size_t idx) const noexcept
		{
			if (idx >= numChannels)
			{
				//throw std::out_of_range("SampleBuf::operator[]: Index out of range");
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

