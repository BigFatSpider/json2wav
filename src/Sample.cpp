// Copyright Dan Price 2026.

#include "Sample.h"
#include <exception>
//#include <utility>
//#include <unordered_map>

static constexpr const size_t MaxBytes = 4ull * 1024ull * 1024ull * 1024ull;

#define ALBUMBOT_USE_MALLOC 1

#if ALBUMBOT_USE_MALLOC
#define ALBUMBOT_MAX_BYTES_PTR std::malloc(MaxBytes)
#else
static char thebytes[MaxBytes] = { 0 };
#define ALBUMBOT_MAX_BYTES_PTR thebytes
#endif

namespace
{

	class MemoryPool
	{
	private:
		struct MemSizePair
		{
			void* mem;
			size_t size;
			MemSizePair() noexcept : mem(nullptr), size(0) {}
			MemSizePair(const MemSizePair& other) noexcept : mem(other.mem), size(other.size) {}
			MemSizePair(MemSizePair&& other) noexcept : mem(other.mem), size(other.size) {}
			explicit MemSizePair(void* const mem_init, const size_t size_init) noexcept : mem(mem_init), size(size_init) {}
			MemSizePair& operator=(const MemSizePair& other) noexcept
			{
				mem = other.mem;
				size = other.size;
				return *this;
			}
			MemSizePair& operator=(MemSizePair&& other) noexcept
			{
				mem = other.mem;
				size = other.size;
				return *this;
			}
			MemSizePair& assign(void* const mem_ass, const size_t size_ass) noexcept
			{
				mem = mem_ass;
				size = size_ass;
				return *this;
			}
			~MemSizePair() noexcept {}
		};

		class MemSizeArr
		{
		};

	private:
		MemoryPool() : memory(ALBUMBOT_MAX_BYTES_PTR), totalmemsize(MaxBytes), blocksizes_end(0), freeblocks_end(0), alignment(8)
		{
			if (!memory)
			{
				std::terminate();
			}

			//freeblocks.push_back(std::make_pair(memory, totalmemsize));
			if (!push_freeblock(memory, totalmemsize))
			{
				std::terminate();
			}
		}
		~MemoryPool() noexcept
		{
			std::free(memory);
		}

	public:
		static MemoryPool& Get() noexcept
		{
			static MemoryPool singleton;
			return singleton;
		}

	public:
		void* GetMemory(const size_t memsizerequest)
		{
			if (memsizerequest > 0)
			{
				const size_t memsize = memsizerequest + (alignment - (((memsizerequest - 1) % alignment) + 1));
				//for (auto freeblock = freeblocks.begin(); freeblock != freeblocks.end(); ++freeblock)
				for (size_t freeblocks_idx = 0; freeblocks_idx < freeblocks_end; ++freeblocks_idx)
				{
					//if (freeblock->second >= memsize)
					if (freeblocks[freeblocks_idx].size >= memsize)
					{
						//void* const mem = freeblock->first;
						void* const mem = freeblocks[freeblocks_idx].mem;
						//blocksizes.emplace(mem, memsize);
						push_blocksize(mem, memsize);
						void* const newblock = static_cast<void*>(static_cast<unsigned char*>(mem) + memsize);
						//const size_t newblocksize = freeblock->second - memsize;
						const size_t newblocksize = freeblocks[freeblocks_idx].size - memsize;

						if (newblocksize == 0)
						{
							//freeblocks.erase(freeblock);
							remove_freeblock(freeblocks_idx);
						}
						else
						{
							//freeblock->first = newblock;
							//freeblock->second = newblocksize;
							freeblocks[freeblocks_idx].assign(newblock, newblocksize);
						}

						return mem;
					}
				}

				throw json2wav::OutOfSampleMemory();
			}

			return nullptr;
		}

		void FreeMemory(void* const mem) noexcept
		{
			if (!mem)
			{
				return;
			}

			//auto blocksize = blocksizes.find(mem);
			const size_t blocksizes_idx = find_blocksize(mem);
			//if (blocksize != blocksizes.end())
			if (blocksizes_idx < blocksizes_end)
			{
				//void* const nextblock = static_cast<void*>(static_cast<unsigned char*>(mem) + blocksize->second);
				void* const nextblock = static_cast<void*>(static_cast<unsigned char*>(mem) + blocksizes[blocksizes_idx].size);
				//auto nextblockfree = std::find_if(freeblocks.begin(), freeblocks.end(),
				//	[nextblock](const std::pair<void*, size_t>& freeblock)
				//	{ return freeblock.first == nextblock; });
				const size_t nextblockfree_idx = find_freeblock(nextblock);

				//if (nextblockfree != freeblocks.end())
				if (nextblockfree_idx < freeblocks_end)
				{
					//nextblockfree->first = mem;
					//nextblockfree->second += blocksize->second;
					freeblocks[nextblockfree_idx].mem = mem;
					//freeblocks[nextblockfree_idx].size += blocksize->second;
					freeblocks[nextblockfree_idx].size += blocksizes[blocksizes_idx].size;
				}
				else
				{
					//freeblocks.emplace_back(*blocksize);
					//push_freeblock(blocksize->first, blocksize->second);
					push_freeblock(blocksizes[blocksizes_idx]);
				}

				//blocksizes.erase(blocksize);
				remove_blocksize(blocksizes_idx);
			}
		}

	private:
		bool push_blocksize(void* const mem, const size_t size)
		{
			return push_memsizepair(blocksizes, blocksizes_end, blocksizes_size, mem, size);
		}

		bool push_blocksize(const MemSizePair& memsize)
		{
			return push_memsizepair(blocksizes, blocksizes_end, blocksizes_size, memsize);
		}

		bool push_freeblock(void* const mem, const size_t size)
		{
			return push_memsizepair(freeblocks, freeblocks_end, freeblocks_size, mem, size);
		}

		bool push_freeblock(const MemSizePair& memsize)
		{
			return push_memsizepair(freeblocks, freeblocks_end, freeblocks_size, memsize);
		}

		bool push_memsizepair(
			MemSizePair* const memsizearr,
			size_t& memsizearr_end,
			const size_t memsizearr_size,
			void* const mem,
			const size_t size)
		{
			if (memsizearr_end < memsizearr_size)
			{
				memsizearr[memsizearr_end++].assign(mem, size);
				return true;
			}
			return false;
		}

		bool push_memsizepair(
			MemSizePair* const memsizearr,
			size_t& memsizearr_end,
			const size_t memsizearr_size,
			const MemSizePair& memsize)
		{
			if (memsizearr_end < memsizearr_size)
			{
				memsizearr[memsizearr_end++] = memsize;
				return true;
			}
			return false;
		}

		bool remove_blocksize(const size_t idx)
		{
			return remove_memsizepair(blocksizes, blocksizes_end, blocksizes_size, idx);
		}

		bool remove_freeblock(const size_t idx)
		{
			return remove_memsizepair(freeblocks, freeblocks_end, freeblocks_size, idx);
		}

		bool remove_memsizepair(
			MemSizePair* const memsizearr,
			size_t& memsizearr_end,
			const size_t memsizearr_size,
			const size_t idx)
		{
			if (idx < memsizearr_end)
			{
				memsizearr[idx] = memsizearr[--memsizearr_end];
				return true;
			}
			return false;
		}

		size_t find_blocksize(void* const mem)
		{
			return find_memsizepair(blocksizes, blocksizes_end, mem);
		}

		size_t find_freeblock(void* const mem)
		{
			return find_memsizepair(freeblocks, freeblocks_end, mem);
		}

		size_t find_memsizepair(
			MemSizePair* const memsizearr,
			const size_t memsizearr_end,
			void* const mem)
		{
			for (size_t idx = 0; idx < memsizearr_end; ++idx)
			{
				if (memsizearr[idx].mem == mem)
				{
					return idx;
				}
			}
			return memsizearr_end;
		}

	private:
		void* const memory;
		const size_t totalmemsize;
		//std::unordered_map<void*, size_t> blocksizes;
		static constexpr const size_t blocksizes_size = MaxBytes / json2wav::sampleChunkSize;
		MemSizePair blocksizes[blocksizes_size];
		size_t blocksizes_end;
		//std::vector<std::pair<void*, size_t>> freeblocks;
		static constexpr const size_t freeblocks_size = MaxBytes / json2wav::sampleChunkSize;
		MemSizePair freeblocks[freeblocks_size];
		size_t freeblocks_end;
		const size_t alignment;
	};

	MemoryPool* const volatile pmempool = &MemoryPool::Get();
	MemoryPool& mempool = MemoryPool::Get();
}

namespace json2wav
{
	Sample* SampleBuf::salloc(const size_t numsamples)
	{
		return static_cast<Sample*>(mempool.GetMemory(numsamples * sizeof(Sample)));
	}

	void SampleBuf::sfree(Sample* const mem) noexcept
	{
		mempool.FreeMemory(mem);
	}

	Sample** SampleBuf::challoc(const size_t numchannels)
	{
		return static_cast<Sample**>(mempool.GetMemory(numchannels * sizeof(Sample*)));
	}

	void SampleBuf::chfree(Sample** const mem) noexcept
	{
		mempool.FreeMemory(mem);
	}

	std::mutex SampleBuf::allocmtx;
}

