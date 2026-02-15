// Copyright Dan Price 2026.

#pragma once

#include "IAudioObject.h"
#include "Sample.h"
#include "Memory.h"
#include "WavFile.h"
#include <string>
#include <fstream>
#include <iostream>
#include <utility>
#include <chrono>
#include <cstdint>
#include <cmath>

namespace json2wav
{
	typedef void (*SerializeSampleFunc)(Vector<riff::Byte>&, const Sample&);

	void SerializeSample16(Vector<riff::Byte>& bytes, const Sample& sample)
	{
		const int16_t sample16 = sample.AsInt16();
		bytes.push_back(sample16 & 0xff);
		bytes.push_back((sample16 >> 8) & 0xff);
	}

	void SerializeSample24(Vector<riff::Byte>& bytes, const Sample& sample)
	{
		const int32_t sample24 = sample.AsInt24();
		bytes.push_back(sample24 & 0xff);
		bytes.push_back((sample24 >> 8) & 0xff);
		bytes.push_back((sample24 >> 16) & 0xff);
	}

	void SerializeSample32(Vector<riff::Byte>& bytes, const Sample& sample)
	{
		const float sample32Float = sample.AsFloat32();
		const char* const sampleFloatBytes = reinterpret_cast<const char*>(&sample32Float);
		const uint32_t sample32 = [sampleFloatBytes]()
		{
			uint32_t sample32;
			char* sampleBytes = reinterpret_cast<char*>(&sample32);
			for (size_t i = 0; i < 4; ++i)
			{
				sampleBytes[i] = sampleFloatBytes[i];
			}
			return sample32;
		}();
		bytes.push_back(sample32 & 0xff);
		bytes.push_back((sample32 >> 8) & 0xff);
		bytes.push_back((sample32 >> 16) & 0xff);
		bytes.push_back((sample32 >> 24) & 0xff);
	}

	Vector<riff::Byte> GetBytes(
		Sample* const* const bufs,
		const size_t numChannels,
		const size_t numSamples,
		ESampleType sampleType)
	{
		Vector<riff::Byte> bytes;
		bytes.reserve(numSamples * numChannels * GetSampleSize(sampleType));

		SerializeSampleFunc serialize;
		switch (sampleType)
		{
		case ESampleType::Int16: serialize = &SerializeSample16; break;
		case ESampleType::Int24: serialize = &SerializeSample24; break;
		case ESampleType::Float32: serialize = &SerializeSample32; break;
		}

		for (size_t smpnum = 0; smpnum < numSamples; ++smpnum)
		{
			for (size_t ch = 0; ch < numChannels; ++ch)
			{
				serialize(bytes, bufs[ch][smpnum]);
			}
		}

		return bytes;
	}

	template<bool bOwner = false>
	class AudioFileOut
	{
	public:
		void Write(const std::string& filename, const size_t numSamples, const unsigned long sampleRate = 44100,
			const ESampleType sampleType = ESampleType::Int16, const size_t numChannels = 2)
		{
			std::cout << "Rendering audio for " << filename << "...\n";
			SampleBuf buf(numChannels, numSamples);
			Vector<Sample*> choffsets(numChannels, nullptr);
			const float nsf = static_cast<float>(numSamples);
			float pertwentdone = -1.0f;
			auto renderstart = std::chrono::steady_clock::now();
			for (size_t offset = 0, samplesLeft = numSamples, readSamples = 0; samplesLeft > 0;)
			{
				const float nextpertwentdone = std::floorf(25.0f * (static_cast<float>(offset) / nsf));
				if (nextpertwentdone > pertwentdone)
				{
					pertwentdone = nextpertwentdone;
					std::cout << (pertwentdone * 4.0f) << "%\n";
				}
				for (size_t ch = 0; ch < numChannels; ++ch)
					choffsets[ch] = buf.get()[ch] + offset;
				readSamples = (samplesLeft < sampleChunkNum) ? samplesLeft : sampleChunkNum;
				inputs.GetSamples(choffsets.data(), numChannels, readSamples, sampleRate, nullptr);
				samplesLeft -= readSamples;
				offset += readSamples;
			}
			auto renderstop = std::chrono::steady_clock::now();
			std::cout << "100.0%\n";
			std::cout << "Render took " << (0.001*static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(renderstop - renderstart).count())) << " seconds\n";
			Vector<riff::DataPtr> bytesVec;
			bytesVec.emplace_back(riff::MakePtr<riff::BytesPtr>(GetBytes(buf.get(), numChannels, numSamples, sampleType)));
			std::cout << "Writing " << filename << "...\n";
			switch (sampleType)
			{
			case ESampleType::Int16:
				if (sampleRate == 44100 && numChannels == 2)
				{
					riff::WavFile wav(riff::GetCDWavFormat());
					wav.SetData(std::move(bytesVec));
					wav.SaveAs(filename);
				}
				else
				{
					riff::WavFile wav(riff::GetWavFormat(numChannels, sampleRate, 16));
					wav.SetData(std::move(bytesVec));
					wav.SaveAs(filename);
				} break;
			case ESampleType::Int24:
				{
					riff::WavFile wav(riff::GetWavFormat(numChannels, sampleRate, 24));
					wav.SetData(std::move(bytesVec));
					wav.SaveAs(filename);
				} break;
			case ESampleType::Float32:
				{
					riff::WavFile wav(riff::GetWavFormat(numChannels, sampleRate, 32));
					wav.SetData(std::move(bytesVec));
					wav.SaveAs(filename);
				} break;
			default: break;
			}
			std::cout << "Done writing " << filename << ".\n";

#ifdef ALBUMBOT_DEBUGNEW
			json2wav::PrintAllocTimes("just after writing wav to disk");
#endif
		}

		bool AddInput(SharedPtr<IAudioObject> inputNode)
		{
			return inputs.AddInput(std::move(inputNode));
		}

		bool RemoveInput(SharedPtr<IAudioObject> inputNode)
		{
			return inputs.RemoveInput(std::move(inputNode));
		}

	private:
		BasicAudioSum<bOwner> inputs;
	};

	class AudioFileIn : public IAudioObject
	{
	};
}

