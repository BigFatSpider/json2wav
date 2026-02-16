// Copyright Dan Price 2026.

#include "RiffData.h"
#include "FourCC.h"
#include "Memory.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <cstdint>

namespace json2wav
{
	inline void SerializeUint32LE(std::ofstream& o, const uint32_t data)
	{
		char buf[4] = {(char(data & 0xff)),
			(char((data >> 8) & 0xff)),
			(char((data >> 16) & 0xff)),
			(char((data >> 24) & 0xff))};
		o.write(buf, 4);
	}
	
	inline uint32_t Uint32FromCharBufLE(const char* buf) noexcept
	{
		return uint32_t(buf[0]) | uint32_t(buf[1] << 8) | uint32_t(buf[2] << 16) | uint32_t(buf[3] << 24);
	}

	namespace riff
	{
		using ChunkID = FourCC;

		class RiffBytes : public RiffData
		{
		private:
			virtual Byte& GetByte(const RiffSize position) override
			{
				return bytes[position];
			}

			virtual Byte GetByte(const RiffSize position) const override
			{
				return bytes[position];
			}

		public:
			explicit RiffBytes(std::ifstream& filein, const RiffSize readsize)
			{
				if (readsize)
				{
					bytes.resize(readsize);
					filein.read(reinterpret_cast<char*>(&bytes[0]), bytes.size());
					if (bytes.size() != readsize)
					{
						throw std::logic_error("RiffBytes didn't read the correct number of bytes!");
					}
					if (readsize % 2)
					{
						char c;
						filein.read(&c, 1);
					}
				}
			}

			explicit RiffBytes(std::vector<Byte>&& data)
				: bytes(std::move(data))
			{
			}
		
		private:
			virtual void Serialize(std::ofstream& fileout) const override
			{
				if (bytes.size())
				{
					fileout.write(reinterpret_cast<const char*>(&bytes[0]), bytes.size());
					if (bytes.size() % 2)
					{
						const char c('\0');
						fileout.write(&c, 1);
					}
				}
			}

			virtual RiffSize GetSize() const override
			{
				return bytes.size();
			}

		private:
			std::vector<Byte> bytes;
		};

		using BytesPtr = SharedPtr<RiffBytes>;
		using ConstBytesPtr = SharedPtr<const RiffBytes>;
		template<> struct RemovePtr<BytesPtr> { using type = RiffBytes; };
		template<> struct RemovePtr<ConstBytesPtr> { using type = const RiffBytes; };

		class RiffFourCC : public RiffData
		{
		private:
			virtual Byte& GetByte(const RiffSize position) override
			{
				if (position >= 4)
				{
					throw std::length_error("RiffFourCCs only have four bytes.");
				}
				char& c(data[position]);
				return *reinterpret_cast<Byte*>(&c);
			}

			virtual Byte GetByte(const RiffSize position) const override
			{
				if (position >= 4)
				{
					throw std::length_error("RiffFourCCs only have four bytes.");
				}
				const char c(data[position]);
				return *reinterpret_cast<const Byte*>(&c);
			}

		public:
			explicit RiffFourCC(std::ifstream& filein)
			{
				static char buf[5];
				filein.read(buf, 4);
				data = buf;
			}

		private:
			virtual void Serialize(std::ofstream& fileout) const override
			{
				char buf[5] = { data[0], data[1], data[2], data[3], '\0' };
				fileout.write(buf, 4);
			}

			virtual RiffSize GetSize() const override
			{
				return 4;
			}

		private:
			FourCC data;
		};

		using FourCCPtr = SharedPtr<RiffFourCC>;
		using ConstFourCCPtr = SharedPtr<const RiffFourCC>;
		template<> struct RemovePtr<FourCCPtr> { using type = RiffFourCC; };
		template<> struct RemovePtr<ConstFourCCPtr> { using type = const RiffFourCC; };

		class RiffChunk;
		using ChunkPtr = SharedPtr<RiffChunk>;
		using ConstChunkPtr = SharedPtr<const RiffChunk>;
		template<> struct RemovePtr<ChunkPtr> { using type = RiffChunk; };
		template<> struct RemovePtr<ConstChunkPtr> { using type = const RiffChunk; };

		class RiffChunk : public RiffData
		{
		private:
			virtual Byte& GetByte(const RiffSize position) override
			{
				if (position < 4)
				{
					char& c(ckid[position]);
					return *reinterpret_cast<Byte*>(&c);
				}
				else if (position < 8)
				{
					return reinterpret_cast<Byte*>(&cksz)[position - 4];
				}
				RiffSize currentpos = position - 8;
				RiffSize idx = 0;
				RiffSize currentsize = ckdata[idx]->GetSize();
				while (currentpos >= currentsize)
				{
					currentpos -= currentsize;
					++idx;
					currentsize = ckdata[idx]->GetSize();
				}
				return ckdata[idx]->GetByte(currentpos);
			}

			virtual Byte GetByte(const RiffSize position) const override
			{
				if (position < 4)
				{
					const char c(ckid[position]);
					return *reinterpret_cast<const Byte*>(&c);
				}
				else if (position < 8)
				{
					return reinterpret_cast<const Byte*>(&cksz)[position - 4];
				}
				RiffSize currentpos = position - 8;
				RiffSize idx = 0;
				RiffSize currentsize = ckdata[idx]->GetSize();
				while (currentpos >= currentsize)
				{
					currentpos -= currentsize;
					++idx;
					currentsize = ckdata[idx]->GetSize();
				}
				return ckdata[idx]->GetByte(currentpos);
			}

		public:
			explicit RiffChunk(std::ifstream& filein)
			{
				static char buf[5];
				filein.read(buf, 4);
				ckid = buf;
				filein.read(buf, 4);
				cksz = Uint32FromCharBufLE(buf);
				if (ckid == "LIST")
				{
					ckdata.emplace_back(MakePtr<FourCCPtr>(filein));
					uint32_t bytesread = 4;
					while (bytesread < cksz && filein)
					{
						DataPtr dataptr = MakePtr<ChunkPtr>(filein);
						const RiffSize addedchunksize = dataptr->GetSize();
						if (MAX_SIZE - addedchunksize < bytesread)
						{
							throw std::length_error("RiffChunks cannot exceed 2^32 - 9 bytes in size!");
						}
						ckdata.emplace_back(std::move(dataptr));
						bytesread += addedchunksize;
						if (addedchunksize % 2)
						{
							bytesread++;
						}
					}
				}
				else
				{
					DataPtr dataptr = MakePtr<BytesPtr>(filein, cksz);
					const uint32_t bytesread = dataptr->GetSize();
					ckdata.emplace_back(std::move(dataptr));
					if (bytesread != cksz)
					{
						throw std::logic_error(
							"Didn't read the correct number of bytes, or file has incorrect size!");
					}
				}
			}

			explicit RiffChunk(const ChunkID& ckidInit, std::vector<DataPtr>&& ckdataInit)
				: ckid(ckidInit), cksz(
					[&ckdataInit]()
					{
						RiffSize sz(0);
						for (const ConstDataPtr ckdatum : ckdataInit)
						{
							sz += ckdatum->GetSize();
						}
						return sz;
					}()), ckdata(std::move(ckdataInit))
			{
			}

			RiffChunk& operator=(std::vector<DataPtr>&& newdata) noexcept
			{
				RiffSize newsize = 0;
				for (const ConstDataPtr newdatum : newdata)
				{
					newsize += newdatum->GetSize();
				}
				cksz = newsize;
				ckdata = std::move(newdata);
				return *this;
			}

			const ChunkID& GetChunkID() const noexcept
			{
				return ckid;
			}

			void PrintSubchunks() const
			{
				for (const ConstDataPtr data : ckdata)
				{
					if (data->IsChunk())
					{
						const RiffChunk& subchunk(*static_cast<const RiffChunk*>(data.get()));
						std::cout << "Subchunk ID: \"" << std::string(subchunk.GetChunkID()) << "\"\n";
						std::cout << "Subchunk size: " << data->GetSize() << "\n";
					}
				}
			}

			size_t GetNumDatas() const noexcept
			{
				return ckdata.size();
			}

			RiffSize GetChunkSize() const noexcept
			{
				return cksz;
			}

			ConstDataPtr operator[](RiffSize idx) const
			{
				ConstDataPtr dataptr(ckdata[idx]);
				return dataptr;
			}

			DataPtr& operator[](RiffSize idx)
			{
				return ckdata[idx];
			}

		private:
			virtual void Serialize(std::ofstream& o) const override
			{
				o << ckid[0] << ckid[1] << ckid[2] << ckid[3];
				SerializeUint32LE(o, cksz);
				uint32_t bytesserialized = 8;
				for (const ConstDataPtr dataptr : ckdata)
				{
					dataptr->Serialize(o);
					bytesserialized += dataptr->GetSize();
					if (bytesserialized % 2)
					{
						bytesserialized++;
					}
				}
			}

			virtual RiffSize GetSize() const override
			{
				if (MAX_SIZE - 8 <= cksz)
				{
					throw std::length_error("RiffChunk cannot hold more than 2^32 - 9 bytes of data!");
				}
				return cksz + 8;
			}

			virtual bool IsChunk() const noexcept override
			{
				return true;
			}

		private:
			ChunkID ckid;
			RiffSize cksz;
			std::vector<DataPtr> ckdata;
		};

		class RiffFile
		{
		protected:
			RiffFile()
				: filesize(0), bNeedsValidate(true), bIsValid(false)
			{
			}

			RiffFile(const RiffFile&) = default;

			RiffFile(RiffFile&&) noexcept = default;

			explicit RiffFile(const std::string filenameInit, ChunkPtr (*chunkptrFactory)(std::ifstream&))
				: filename(filenameInit), bNeedsValidate(true), bIsValid(false)
			{
				static char buf[5];
				std::ifstream filein(filename, std::ios_base::binary | std::ios_base::in);
				filein.read(buf, 4);
				if (buf[0] != 'R' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'F')
				{
					throw std::runtime_error((std::string(
						"First four bytes of a riff file must spell \"RIFF\", but they spell \"") + buf + "\"!").c_str());
				}
				filein.read(buf, 4);
				filesize = Uint32FromCharBufLE(buf);
				filein.read(buf, 4);
				riffid = buf;
				while (filein)
				{
					if (filein.peek() != std::char_traits<char>::eof())
					{
						chunks.emplace_back(chunkptrFactory(filein));
					}
				}
			}

		public:
			virtual ~RiffFile() noexcept
			{
			}

			void Save() const
			{
				if (filename.empty())
				{
					throw std::logic_error("RiffFile::Save(): Filename empty; can't save!");
				}

				Serialize();
			}

			void SaveAs(const std::string& saveasfilename)
			{
				if (saveasfilename.empty())
				{
					throw std::logic_error("RiffFile::SaveAs(): New filename empty; can't save as!");
				}

				filename = saveasfilename;
				bNeedsValidate = true;
				Serialize();
			}

			bool IsValid() const
			{
				if (bNeedsValidate)
				{
					bIsValid = !filename.empty() && Validate(filesize, riffid, chunks);
					bNeedsValidate = false;
				}
				return bIsValid;
			}

			const std::string& GetName() const noexcept
			{
				return filename;
			}

			RiffSize GetSize() const noexcept
			{
				return filesize;
			}

			RiffSize CalcSize() const
			{
				RiffSize calcsize = 4;
				for (const ConstDataPtr chunk : chunks)
				{
					const RiffSize sz(chunk->GetSize());
					calcsize += sz + (sz % 2);
				}
				return calcsize;
			}

			const FourCC& GetRiffID() const noexcept
			{
				return riffid;
			}

		private:
			template<typename T>
			T GetChunkInner(const ChunkID& ckid) const
			{
				T chunk(nullptr);
				auto it = std::find_if(chunks.begin(), chunks.end(),
					[&ckid](const ConstChunkPtr chunk) { return chunk->GetChunkID() == ckid; });
				if (it != chunks.end())
				{
					chunk = *it;
				}
				return chunk;
			}

		public:
			ConstChunkPtr GetChunk(const ChunkID& ckid) const
			{
				ConstChunkPtr chunk(GetChunkInner<ConstChunkPtr>(ckid));
				return chunk;
			}

			void PrintChunks() const
			{
				for (const ConstChunkPtr chunk : chunks)
				{
					std::cout << "Chunk ID: \"" << std::string(chunk->GetChunkID()) << "\"\n";
					std::cout << "Chunk size: " << static_cast<const RiffData*>(chunk.get())->GetSize() << "\n";
					chunk->PrintSubchunks();
				}
			}

		protected:
			void SetName(const std::string& newfilename)
			{
				filename = newfilename;
				bNeedsValidate = true;
			}

			void SetRiffID(const FourCC& newriffid)
			{
				riffid = newriffid;
				bNeedsValidate = true;
			}

			void SetChunk(const ChunkID& ckid, std::vector<DataPtr>&& data)
			{
				auto it = std::find_if(chunks.begin(), chunks.end(),
					[&ckid](const ConstChunkPtr chunk) { return chunk->GetChunkID() == ckid; });
				if (it != chunks.end())
				{
					**it = std::move(data);
				}
				else
				{
					chunks.emplace_back(MakePtr<ChunkPtr>(ckid, std::move(data)));
				}
				bNeedsValidate = true;
			}

			ChunkPtr GetChunkToEdit(const ChunkID& ckid)
			{
				ChunkPtr chunk(GetChunkInner<ChunkPtr>(ckid));
				if (chunk)
				{
					bNeedsValidate = true;
				}
				return chunk;
			}

			void UpdateSize()
			{
				filesize = CalcSize();
				bNeedsValidate = true;
			}

		private:
			void Serialize() const
			{
				if (!IsValid())
				{
					throw std::logic_error("RiffFile::Serialize(): Tried to serialize invalid RiffFile!");
				}

				std::ofstream fileout(filename, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
				fileout.write("RIFF", 4);
				SerializeUint32LE(fileout, filesize);
				SerializeUint32LE(fileout, static_cast<uint32_t>(riffid));
				for (const ConstDataPtr chunk : chunks)
				{
					chunk->Serialize(fileout);
				}
			}

			virtual bool Validate(const RiffSize, const FourCC&, const std::vector<ChunkPtr>&) const = 0;

		private:
			std::string filename;
			RiffSize filesize;
			FourCC riffid;
			std::vector<ChunkPtr> chunks;
			mutable bool bNeedsValidate;
			mutable bool bIsValid;
		};

		class GenericRiffFile : public RiffFile
		{
		public:
			GenericRiffFile() = default;
			GenericRiffFile(const GenericRiffFile&) = default;
			GenericRiffFile(GenericRiffFile&&) noexcept = default;

			explicit GenericRiffFile(std::string filenameInit)
				: RiffFile(std::move(filenameInit), &MakePtr<ChunkPtr>)
			{
			}

		private:
			virtual bool Validate(
				const RiffSize filesize,
				const FourCC& riffid,
				const std::vector<ChunkPtr>& chunks) const override
			{
				return filesize == CalcSize();
			}
		};
	}
}

