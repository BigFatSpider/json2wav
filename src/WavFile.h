// Copyright Dan Price 2026.

#include "RiffFile.h"
#include <utility>
#include <cstdint>

#if defined(WAVFILE_LOGGING_ENABLED) && WAVFILE_LOGGING_ENABLED
#include <iostream>
#define WAVLOG(msg) std::cout << "wav log: " << msg << '\n'
#else
#define WAVLOG(msg)
#endif

namespace json2wav::riff
{
	struct WaveFormat
	{
		enum class ValidFormatSizes : uint32_t	// 4 bytes
		{
			PCM = 16,
			PCM_w_cbsize = 18,
			Extended = 40
		} NumBytes;
		uint32_t GetNumBytes() const noexcept { return static_cast<uint32_t>(NumBytes); }

		enum class ValidFormatTags : uint16_t	// 2 bytes
		{
			PCM = 1
		} wFormatTag;
		uint16_t GetFormatTag() const noexcept { return static_cast<uint16_t>(wFormatTag); }

		enum class ValidChannelNums : uint16_t	// 2 bytes
		{
			Mono = 1,
			Stereo = 2
		} nChannels;
		uint16_t GetNumChannels() const noexcept { return static_cast<uint16_t>(nChannels); }

		enum class ValidSampleRates : uint32_t	// 4 bytes
		{
			s8000 = 8000,
			s11025 = 11025,
			s12000 = 12000,
			s16000 = 16000,
			s22050 = 22050,
			s24000 = 24000,
			s32000 = 32000,
			s44100 = 44100,
			s48000 = 48000,
			s64000 = 64000,
			s88200 = 88200,
			s96000 = 96000,
			s128000 = 128000,
			s176400 = 176400,
			s192000 = 192000
		} nSamplesPerSec;
		uint32_t GetSampleRate() const noexcept { return static_cast<uint32_t>(nSamplesPerSec); }

		uint32_t nAvgBytesPerSec;		// 4 bytes
		uint16_t nBlockAlign;			// 2 bytes

		enum class ValidBitDepths : uint16_t	// 2 bytes
		{
			b8 = 8,
			b16 = 16,
			b24 = 24,
			b32 = 32
		} wBitsPerSample;
		uint16_t GetBitDepth() const noexcept { return static_cast<uint16_t>(wBitsPerSample); }

		uint16_t cbSize;				// 2 bytes

		uint16_t wValidBitsPerSample;	// 2 bytes
		uint32_t dwChannelMask;			// 4 bytes
		struct Guid						// 16 bytes
		{
			uint32_t data1;
			uint16_t data2;
			uint16_t data3;
			uint16_t data4;
			uint8_t data5[6];
		} SubFormat;
	};

	namespace valid
	{
		// Valid values
		constexpr const RiffSize validPCMSize(static_cast<RiffSize>(WaveFormat::ValidFormatSizes::PCM));
		constexpr const uint16_t validPCMTag(static_cast<uint16_t>(WaveFormat::ValidFormatTags::PCM));
		constexpr const uint16_t validMono(static_cast<uint16_t>(WaveFormat::ValidChannelNums::Mono));
		constexpr const uint16_t validStereo(static_cast<uint16_t>(WaveFormat::ValidChannelNums::Stereo));
		constexpr const uint32_t valid8k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s8000));
		constexpr const uint32_t valid11k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s11025));
		constexpr const uint32_t valid12k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s12000));
		constexpr const uint32_t valid16k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s16000));
		constexpr const uint32_t valid22k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s22050));
		constexpr const uint32_t valid24k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s24000));
		constexpr const uint32_t valid32k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s32000));
		constexpr const uint32_t valid44k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s44100));
		constexpr const uint32_t valid48k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s48000));
		constexpr const uint32_t valid64k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s64000));
		constexpr const uint32_t valid88k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s88200));
		constexpr const uint32_t valid96k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s96000));
		constexpr const uint32_t valid128k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s128000));
		constexpr const uint32_t valid176k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s176400));
		constexpr const uint32_t valid192k(static_cast<uint32_t>(WaveFormat::ValidSampleRates::s192000));
		constexpr const uint16_t valid8bit(static_cast<uint16_t>(WaveFormat::ValidBitDepths::b8));
		constexpr const uint16_t valid16bit(static_cast<uint16_t>(WaveFormat::ValidBitDepths::b16));
		constexpr const uint16_t valid24bit(static_cast<uint16_t>(WaveFormat::ValidBitDepths::b24));
		constexpr const uint16_t valid32bit(static_cast<uint16_t>(WaveFormat::ValidBitDepths::b32));
	}

	WaveFormat::ValidSampleRates GetValidSampleRate(const uint32_t sampleRate)
	{
		if (sampleRate <= 9512) { return WaveFormat::ValidSampleRates::s8000; }
		if (sampleRate <= 11512) { return WaveFormat::ValidSampleRates::s11025; }
		if (sampleRate < 14000) { return WaveFormat::ValidSampleRates::s12000; }
		if (sampleRate < 19025) { return WaveFormat::ValidSampleRates::s16000; }
		if (sampleRate < 23025) { return WaveFormat::ValidSampleRates::s22050; }
		if (sampleRate < 28000) { return WaveFormat::ValidSampleRates::s24000; }
		if (sampleRate < 38050) { return WaveFormat::ValidSampleRates::s32000; }
		if (sampleRate < 46050) { return WaveFormat::ValidSampleRates::s44100; }
		if (sampleRate < 56000) { return WaveFormat::ValidSampleRates::s48000; }
		if (sampleRate < 76100) { return WaveFormat::ValidSampleRates::s64000; }
		if (sampleRate < 92100) { return WaveFormat::ValidSampleRates::s88200; }
		if (sampleRate < 112000) { return WaveFormat::ValidSampleRates::s96000; }
		if (sampleRate < 152200) { return WaveFormat::ValidSampleRates::s128000; }
		if (sampleRate < 184200) { return WaveFormat::ValidSampleRates::s176400; }
		return WaveFormat::ValidSampleRates::s192000;
	}

	WaveFormat::ValidBitDepths GetValidBitDepth(const uint16_t bitDepth)
	{
		if (bitDepth < 12) { return WaveFormat::ValidBitDepths::b8; }
		if (bitDepth < 20) { return WaveFormat::ValidBitDepths::b16; }
		if (bitDepth < 28) { return WaveFormat::ValidBitDepths::b24; }
		return WaveFormat::ValidBitDepths::b32;
	}

	WaveFormat GetCDWavFormat() noexcept
	{
		WaveFormat fmt;
		fmt.NumBytes = WaveFormat::ValidFormatSizes::PCM;
		fmt.wFormatTag = WaveFormat::ValidFormatTags::PCM;
		fmt.nChannels = WaveFormat::ValidChannelNums::Stereo;
		fmt.nSamplesPerSec = WaveFormat::ValidSampleRates::s44100;
		fmt.nAvgBytesPerSec = 44100 * 4;
		fmt.nBlockAlign = 4;
		fmt.wBitsPerSample = WaveFormat::ValidBitDepths::b16;
		return fmt;
	}

	WaveFormat GetWavFormat(const uint16_t numChannels, const uint32_t sampleRate, const uint16_t bitDepth)
	{
		WaveFormat fmt;
		fmt.NumBytes = WaveFormat::ValidFormatSizes::PCM;
		fmt.wFormatTag = WaveFormat::ValidFormatTags::PCM;
		fmt.nChannels = (numChannels >= 2) ? WaveFormat::ValidChannelNums::Stereo : WaveFormat::ValidChannelNums::Mono;
		fmt.nSamplesPerSec = GetValidSampleRate(sampleRate);
		fmt.wBitsPerSample = GetValidBitDepth(bitDepth);
		fmt.nBlockAlign = (fmt.GetBitDepth() >> 3) * fmt.GetNumChannels();
		fmt.nAvgBytesPerSec = fmt.GetSampleRate() * fmt.nBlockAlign;
		return fmt;
	}

	class WavFile : public RiffFile
	{
	private:
		static ChunkPtr ChunkFactory(std::ifstream& filein)
		{
			return MakePtr<ChunkPtr>(filein);
		}

	public:
		explicit WavFile(std::string filenameInit)
			: RiffFile(std::move(filenameInit), &WavFile::ChunkFactory)
		{
		}

		explicit WavFile(const WaveFormat& wavfmtInit)
			: wavfmt(wavfmtInit)
		{
			SetRiffID(FourCC("WAVE"));
			BytesPtr fmtdataptr(MakePtr<BytesPtr>(GetFmtBytes()));
			Vector<DataPtr> fmtdata;
			fmtdata.emplace_back(std::move(fmtdataptr));
			SetChunk(ChunkID("fmt "), std::move(fmtdata));
		}

		template<typename DataType>
		void SetData(const Vector<Vector<DataType>>& data)
		{
			SetChunk(ChunkID("data"), ToDataPtrVec(data));
			UpdateSize();
		}

		void SetData(Vector<DataPtr>&& data)
		{
			SetChunk(ChunkID("data"), std::move(data));
			UpdateSize();
		}

	private:
		Vector<Byte> GetFmtBytes() const
		{
			Vector<Byte> fmtbytes;
			Vector<Byte> cbsizebytes;
			Vector<Byte> extendedbytes;

			uint16_t cbsize = 0;

			switch (wavfmt.GetNumBytes())
			{
			case 40:

				{
					const Byte* const ValidBitsBytes = reinterpret_cast<const Byte*>(&wavfmt.wValidBitsPerSample);
					extendedbytes.emplace_back(ValidBitsBytes[0]);
					extendedbytes.emplace_back(ValidBitsBytes[1]);

					const Byte* const ChannelMaskBytes = reinterpret_cast<const Byte*>(&wavfmt.dwChannelMask);
					extendedbytes.emplace_back(ChannelMaskBytes[0]);
					extendedbytes.emplace_back(ChannelMaskBytes[1]);
					extendedbytes.emplace_back(ChannelMaskBytes[2]);
					extendedbytes.emplace_back(ChannelMaskBytes[3]);

					const Byte* const GUIDBytes = reinterpret_cast<const Byte*>(&wavfmt.SubFormat);
					for (int i = 0; i < 16; ++i)
					{
						extendedbytes.emplace_back(GUIDBytes[i]);
					}

					cbsize = 22;
				} // Fall through
			case 18:

				{
					const Byte* const CbSizeBytes = reinterpret_cast<const Byte*>(&cbsize);
					cbsizebytes.emplace_back(CbSizeBytes[0]);
					cbsizebytes.emplace_back(CbSizeBytes[1]);
				} // Fall through
			case 16:

				{
					const Byte* const TagBytes = reinterpret_cast<const Byte*>(&wavfmt.wFormatTag);
					fmtbytes.emplace_back(TagBytes[0]);
					fmtbytes.emplace_back(TagBytes[1]);

					const Byte* const ChannelBytes = reinterpret_cast<const Byte*>(&wavfmt.nChannels);
					fmtbytes.emplace_back(ChannelBytes[0]);
					fmtbytes.emplace_back(ChannelBytes[1]);

					const Byte* const SampleRateBytes = reinterpret_cast<const Byte*>(&wavfmt.nSamplesPerSec);
					fmtbytes.emplace_back(SampleRateBytes[0]);
					fmtbytes.emplace_back(SampleRateBytes[1]);
					fmtbytes.emplace_back(SampleRateBytes[2]);
					fmtbytes.emplace_back(SampleRateBytes[3]);

					const Byte* const ByteRateBytes = reinterpret_cast<const Byte*>(&wavfmt.nAvgBytesPerSec);
					fmtbytes.emplace_back(ByteRateBytes[0]);
					fmtbytes.emplace_back(ByteRateBytes[1]);
					fmtbytes.emplace_back(ByteRateBytes[2]);
					fmtbytes.emplace_back(ByteRateBytes[3]);

					const Byte* const BlockAlignBytes = reinterpret_cast<const Byte*>(&wavfmt.nBlockAlign);
					fmtbytes.emplace_back(BlockAlignBytes[0]);
					fmtbytes.emplace_back(BlockAlignBytes[1]);

					const Byte* const BitDepthBytes = reinterpret_cast<const Byte*>(&wavfmt.wBitsPerSample);
					fmtbytes.emplace_back(BitDepthBytes[0]);
					fmtbytes.emplace_back(BitDepthBytes[1]);

					for (const Byte byte : cbsizebytes)
					{
						fmtbytes.emplace_back(byte);
					}
					for (const Byte byte : extendedbytes)
					{
						fmtbytes.emplace_back(byte);
					}
				} break;
			}

			return fmtbytes;
		}

		template<typename DataType>
		Vector<DataPtr> ToDataPtrVec(const Vector<Vector<DataType>>& data)
		{
			Vector<DataPtr> dpv;
			const int byte_depth = wavfmt.GetBitDepth() >> 3;

			if (sizeof(DataType) != byte_depth)
			{
				throw std::invalid_argument("WavFile::ToDataPtrVec: Data type does not match byte depth. Your argument is invalid.");
			}
			else if (data.size() > 0)
			{
				Vector<Byte> bytes;
				bytes.reserve(data.size() * wavfmt.GetNumChannels() * wavfmt.GetBitDepth());

				for (size_t smp = 0; smp < data.size(); ++smp)
				{
					const Vector<DataType>& sample = (data[smp].size() >= wavfmt.GetNumChannels()) ? data[smp] : [this, &data, smp]() -> Vector<DataType>
						{
							Vector<DataType> concat(data[smp]);
							Vector<DataType> empty_samples(wavfmt.GetNumChannels() - data[smp].size(), DataType(0));
							concat.insert(concat.end(), std::make_move_iterator(empty_samples.begin()), std::make_move_iterator(empty_samples.end()));
							return concat;
						}();

					for (int ch = 0; ch < wavfmt.GetNumChannels(); ++ch)
					{
						const Byte* const sample_bytes = reinterpret_cast<const Byte*>(&sample[ch]);
						for (int i = 0; i < byte_depth; ++i)
						{
							bytes.emplace_back(sample_bytes[i]);
						}
					}
				}

				dpv.emplace_back(MakePtr<BytesPtr>(std::move(bytes)));
			}

			return dpv;
		}

		virtual bool Validate(
			const RiffSize filesize,
			const FourCC& riffid,
			const Vector<ChunkPtr>& chunks) const override
		{
			using namespace valid;

			if (riffid != "WAVE")
			{
				WAVLOG("Error: RIFF id is not \"WAVE\"");
				return false;
			}

			const auto calcedsize = CalcSize();
			if (filesize != calcedsize)
			{
				WAVLOG("Error: File size mismatch; stored size = " << filesize << "; calculated size = " << calcedsize);
				return false;
			}

			{
				const ConstChunkPtr fmt(GetChunk(ChunkID("fmt ")));
				if (!fmt)
				{
					WAVLOG("Error: No format chunk");
					return false;
				}

				switch (fmt->GetChunkSize())
				{
				case static_cast<RiffSize>(WaveFormat::ValidFormatSizes::PCM):
				{
					auto IsValidFormat = [](
						const RiffSize NumBytes,
						const uint16_t Tag,
						const uint16_t NumChannels,
						const uint32_t SampleRate,
						const uint32_t BPS,
						const uint16_t BlockAlign,
						const uint16_t BitDepth) -> bool
					{
						if (NumBytes != validPCMSize ||
							Tag != validPCMTag ||
							(NumChannels != validMono && NumChannels != validStereo))
						{
							WAVLOG("Error: Invalid number of bytes, tag, or number of channels");
							WAVLOG("\tNumBytes: " << NumBytes);
							WAVLOG("\tTag: " << Tag);
							WAVLOG("\tNumChannels: " << NumChannels);
							return false;
						}

						auto IsValidSampleRate = [](const uint32_t sr) -> bool
						{
							switch (sr)
							{
							case valid8k:
							case valid11k:
							case valid12k:
							case valid16k:
							case valid22k:
							case valid24k:
							case valid32k:
							case valid44k:
							case valid48k:
							case valid64k:
							case valid88k:
							case valid96k:
							case valid128k:
							case valid176k:
							case valid192k: return true;
							default: break;
							}
							return false;
						};

						if (!IsValidSampleRate(SampleRate))
						{
							WAVLOG("Error: Invalid sample rate; sample rate = " << SampleRate);
							return false;
						}

						const uint16_t validBlockAlign(NumChannels * (BitDepth / 8));
						const uint32_t validBPS(SampleRate * uint32_t(validBlockAlign));
						if (BPS != validBPS ||
							BlockAlign != validBlockAlign ||
							(BitDepth != valid8bit && BitDepth != valid16bit &&
							BitDepth != valid24bit && BitDepth != valid32bit))
						{
							WAVLOG("Error: Invalid bytes-per-second, block align, or bit depth");
							WAVLOG("\tBytes per second: " << BPS);
							WAVLOG("\tBlock align: " << BlockAlign);
							WAVLOG("\tBit depth: " << BitDepth);
							return false;
						}

						return true;
					};

					auto it(fmt->begin());

					auto ExtractFromIterUint16LE = [&it]() -> uint16_t
					{
						return uint16_t(*(++it)) | (uint16_t(*(++it)) << 8);
					};

					auto ExtractFromIterUint32LE = [&it]() -> uint32_t
					{
						return uint32_t(*(++it)) | (uint32_t(*(++it)) << 8) | (uint32_t(*(++it)) << 16) |
							(uint32_t(*(++it)) << 24);
					};

					auto ExtractFromIterRiffSizeLE = [&it]() -> RiffSize
					{
						return RiffSize(*(++it)) | (RiffSize(*(++it)) << 8) | (RiffSize(*(++it)) << 16) |
							(RiffSize(*(++it)) << 24);
					};

					// Chunk ID
					const Byte f(*it);
					const Byte m(*(++it));
					const Byte t(*(++it));
					const Byte fmtspace(*(++it));
					if (f != Byte('f') || m != Byte('m') || t != Byte('t') || fmtspace != Byte(' '))
					{
						WAVLOG("Error: Invalid format chunk id; chunk id = \"" << char(f) << char(m) << char(t) << char(fmtspace) << '"');
						return false;
					}

					// Chunk values
					const RiffSize ckNumBytes(ExtractFromIterRiffSizeLE());
					const uint16_t ckTag(ExtractFromIterUint16LE());
					const uint16_t ckNumChannels(ExtractFromIterUint16LE());
					const uint32_t ckSampleRate(ExtractFromIterUint32LE());
					const uint32_t ckBPS(ExtractFromIterUint32LE());
					const uint16_t ckBlockAlign(ExtractFromIterUint16LE());
					const uint16_t ckBitDepth(ExtractFromIterUint16LE());

					if (!IsValidFormat(ckNumBytes, ckTag, ckNumChannels, ckSampleRate,
						ckBPS, ckBlockAlign, ckBitDepth))
					{
						WAVLOG("Error: Invalid chunk data for format chunk");
						WAVLOG("\tNumber of bytes: " << ckNumBytes);
						WAVLOG("\tTag: " << std::hex << ckTag);
						WAVLOG("\tNumber of channels: " << ckNumChannels);
						WAVLOG("\tSample rate: " << ckSampleRate);
						WAVLOG("\tBytes per second: " << ckBPS);
						WAVLOG("\tBlock align: " << ckBlockAlign);
						WAVLOG("\tBit depth: " << ckBitDepth);
						return false;
					}

					// wavfmt values
					const RiffSize wavfmtNumBytes(static_cast<RiffSize>(wavfmt.NumBytes));
					const uint16_t wavfmtTag(static_cast<uint16_t>(wavfmt.wFormatTag));
					const uint16_t wavfmtNumChannels(static_cast<uint16_t>(wavfmt.nChannels));
					const uint32_t wavfmtSampleRate(static_cast<uint32_t>(wavfmt.nSamplesPerSec));
					const uint32_t wavfmtBPS(static_cast<uint32_t>(wavfmt.nAvgBytesPerSec));
					const uint16_t wavfmtBlockAlign(static_cast<uint16_t>(wavfmt.nBlockAlign));
					const uint16_t wavfmtBitDepth(static_cast<uint16_t>(wavfmt.wBitsPerSample));

					if (!IsValidFormat(wavfmtNumBytes, wavfmtTag, wavfmtNumChannels, wavfmtSampleRate,
						wavfmtBPS, wavfmtBlockAlign, wavfmtBitDepth))
					{
						WAVLOG("Error: Invalid struct data for format chunk");
						WAVLOG("\tNumber of bytes: " << wavfmtNumBytes);
						WAVLOG("\tTag: " << std::hex << wavfmtTag);
						WAVLOG("\tNumber of channels: " << wavfmtNumChannels);
						WAVLOG("\tSample rate: " << wavfmtSampleRate);
						WAVLOG("\tBytes per second: " << wavfmtBPS);
						WAVLOG("\tBlock align: " << wavfmtBlockAlign);
						WAVLOG("\tBit depth: " << wavfmtBitDepth);
						return false;
					}
				} break;
				case static_cast<RiffSize>(WaveFormat::ValidFormatSizes::PCM_w_cbsize):
					WAVLOG("Error: cbsize shouldn't be included in the format size");
					return false;
					break;
				case static_cast<RiffSize>(WaveFormat::ValidFormatSizes::Extended):
					WAVLOG("Error: Extended format not supported");
					return false;
					break;
				default:
					WAVLOG("Error: Unknown, unsupported format size");
					return false;
				}
			}

			{
				const ConstChunkPtr data(GetChunk(ChunkID("data")));
				if (!data)
				{
					WAVLOG("Error: No data chunk");
					return false;
				}
			}

			return true;
		}

	private:
		WaveFormat wavfmt;
	};
}

