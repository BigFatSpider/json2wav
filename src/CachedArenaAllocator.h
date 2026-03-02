// Copyright Dan Price 2026.

#pragma once

#include "MemoryCommon.h"
#include "Macros.h"
#include <mutex>
#include <bit>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace json2wav
{
	template<std::endian Endian>
	inline uint32_t ReadUint32Endianed(std::byte* Memory);

	template<>
	inline uint32_t ReadUint32Endianed<std::endian::little>(std::byte* Memory)
	{
		const uint32_t Byte0 = std::to_integer<uint32_t>(Memory[0]);
		const uint32_t Byte1 = std::to_integer<uint32_t>(Memory[1]);
		const uint32_t Byte2 = std::to_integer<uint32_t>(Memory[2]);
		const uint32_t Byte3 = std::to_integer<uint32_t>(Memory[3]);
		return Byte0 | (Byte1 << 8) | (Byte2 << 16) | (Byte3 << 24);
	}

	template<>
	inline uint32_t ReadUint32Endianed<std::endian::big>(std::byte* Memory)
	{
		const uint32_t Byte0 = std::to_integer<uint32_t>(Memory[0]);
		const uint32_t Byte1 = std::to_integer<uint32_t>(Memory[1]);
		const uint32_t Byte2 = std::to_integer<uint32_t>(Memory[2]);
		const uint32_t Byte3 = std::to_integer<uint32_t>(Memory[3]);
		return (Byte0 << 24) | (Byte1 << 16) | (Byte2 << 8) | Byte3;
	}

	inline uint32_t ReadUint32(std::byte* Memory)
	{
		return ReadUint32Endianed<std::endian::native>(Memory);
	}

	inline void WriteUint32(std::byte* Memory, uint32_t Value)
	{
		const std::byte* ValueMemory = reinterpret_cast<const std::byte*>(&Value);
		Memory[0] = ValueMemory[0];
		Memory[1] = ValueMemory[1];
		Memory[2] = ValueMemory[2];
		Memory[3] = ValueMemory[3];
	}

	class BytePtr
	{
	public:
		BytePtr(const BytePtr&) = delete;
		BytePtr& operator=(const BytePtr&) = delete;

		BytePtr(std::byte* PointerInit = nullptr) noexcept : Pointer(PointerInit) {}

		BytePtr(BytePtr&& Other) noexcept
			: Pointer(Other.Pointer)
		{
			Other.Pointer = nullptr;
		}

		BytePtr& operator=(BytePtr&& Other) noexcept
		{
			if (&Other != this)
			{
				Free();
				Pointer = Other.Pointer;
				Other.Pointer = nullptr;
			}
			return *this;
		}

		~BytePtr() noexcept
		{
			Free();
		}

		void Free() noexcept
		{
			if (Pointer)
			{
				std::free(Pointer);
				Pointer = nullptr;
			}
		}

		std::byte* Release() noexcept
		{
			std::byte* ReleasedPointer = Pointer;
			Pointer = nullptr;
			return ReleasedPointer;
		}

		std::byte* Get() const noexcept
		{
			return Pointer;
		}

		operator std::byte*() const noexcept
		{
			return Pointer;
		}

		explicit operator bool() const noexcept
		{
			return Pointer != nullptr;
		}

		std::byte& operator*() const
		{
			return *Pointer;
		}

		std::byte* operator->() const
		{
			return Pointer;
		}

	private:
		std::byte* Pointer;
	};

	struct AllocationCacheItem
	{
		AllocationCacheItem(uint32_t NumBytesInit = 0, uint32_t AlignBytesInit = 0, uint32_t ArenaIndexInit = InvalidUint32())
			: NumBytes(NumBytesInit), AlignBytes(AlignBytesInit), ArenaIndex(ArenaIndexInit)
		{
		}
		AllocationCacheItem(const AllocationCacheItem&) noexcept = default;
		AllocationCacheItem& operator=(const AllocationCacheItem&) noexcept = default;
		uint32_t NumBytes = 0;
		uint32_t AlignBytes = 0;
		uint32_t ArenaIndex = InvalidUint32();
	};

	constexpr uint64_t AtomicU32Size = sizeof(std::atomic<uint32_t>);
	constexpr uint64_t AtomicU32Align = alignof(std::atomic<uint32_t>);
	constexpr uint64_t AtomicPtrSize = sizeof(std::atomic<std::byte*>);

	struct ArenaHeader
	{
		static constexpr uint64_t BlockSizeLog2 = 24ull;
		static constexpr uint64_t BlockSize = 1ull << BlockSizeLog2;
		static constexpr uint64_t BlockMask = BlockSize - 1ull;
		static constexpr uint64_t HeaderSizeLog2 = 12ull;
		static constexpr uint64_t HeaderSize = 1ull << HeaderSizeLog2;
		static constexpr uint64_t NextIndexOffset = AtomicU32Align > 8ull ? AtomicU32Align : 8ull;
		static constexpr uint64_t BlockArraySize = (HeaderSize - NextIndexOffset - 2ull * AtomicU32Size) / AtomicPtrSize;
		static constexpr uint64_t MaxBlocks = BlockArraySize + 1ull;

		const uint32_t NumBytes;
		const uint32_t AlignBytes;
		std::atomic<uint32_t> NextIndex;
		std::atomic<uint32_t> RecycleStackTop;
		std::atomic<std::byte*> BlockArray[BlockArraySize];

		ArenaHeader(uint32_t NumBytesInit, uint32_t AlignBytesInit)
			: NumBytes(NumBytesInit), AlignBytes(AlignBytesInit), NextIndex(0), RecycleStackTop(InvalidUint32())
		{
			for (uint64_t BlockArrayIndex = 0; BlockArrayIndex < BlockArraySize; ++BlockArrayIndex)
			{
				BlockArray[BlockArrayIndex].store(nullptr);
			}
		}

		~ArenaHeader() noexcept
		{
			for (uint64_t ReverseIndex = 0; ReverseIndex < BlockArraySize; ++ReverseIndex)
			{
				const uint64_t BlockArrayIndex = BlockArraySize - 1ull - ReverseIndex;
				std::byte* Pointer = BlockArray[BlockArrayIndex].load();
				if (Pointer && BlockArray[BlockArrayIndex].compare_exchange_strong(Pointer, nullptr))
				{
					std::free(Pointer);
				}
			}
		}
	};

	inline constexpr uint64_t GetArenaStartOffset(uint64_t AlignBytes)
	{
		const uint64_t AlignMask = AlignBytes - 1;
		return sizeof(ArenaHeader) + ((AlignBytes - (sizeof(ArenaHeader) & AlignMask)) & AlignMask);
	}

	inline uint64_t GetArenaStartOffset(ArenaHeader& Arena)
	{
		return GetArenaStartOffset(Arena.AlignBytes);
	}

	inline constexpr uint64_t GetBlockSize(uint64_t NumBytes)
	{
		return (ArenaHeader::BlockSize / NumBytes) * NumBytes;
	}

	inline uint64_t GetBlockSize(ArenaHeader& Arena)
	{
		return GetBlockSize(Arena.NumBytes);
	}

	inline std::byte* GetArenaStartByte(ArenaHeader& Arena)
	{
		return reinterpret_cast<std::byte*>(&Arena) + GetArenaStartOffset(Arena);
	}

	struct ArenaLocation
	{
		uint32_t Block = InvalidUint32();
		uint32_t Byte = InvalidUint32();
	};

	inline ArenaLocation GetArenaLocation(ArenaHeader& Arena, uint32_t Index)
	{
		ArenaLocation Location;

		if (Index != InvalidUint32())
		{
			const uint64_t NumBytes = Arena.NumBytes;
			const uint64_t BlockSize = GetBlockSize(Arena);

			const uint64_t Start = static_cast<uint64_t>(Index) * NumBytes;
			const uint64_t StartBlock = Start / BlockSize;
			const uint64_t StartByte = (StartBlock == 0) * GetArenaStartOffset(Arena) + (Start % BlockSize);

			Location.Block = static_cast<uint32_t>(StartBlock & 0xffffffffull);
			Location.Byte = static_cast<uint32_t>(StartByte & 0xffffffffull);
		}

		return Location;
	}

	inline std::byte* GetArenaBlock(ArenaHeader& Arena, const ArenaLocation& Location)
	{
		if (Location.Block == 0)
		{
			return reinterpret_cast<std::byte*>(&Arena);
		}

		const uint32_t BlockArrayIndex = Location.Block - 1;
		if (BlockArrayIndex < ArenaHeader::BlockArraySize)
		{
			return Arena.BlockArray[BlockArrayIndex];
		}

		return nullptr;
	}

	inline std::byte* GetArenaAllocationStorage(ArenaHeader& Arena, uint32_t Index)
	{
		ArenaLocation Location = GetArenaLocation(Arena, Index);
		if (std::byte* Block = GetArenaBlock(Arena, Location))
		{
			return Block + Location.Byte;
		}
		return nullptr;
	}

	struct ArenaAllocation
	{
		std::byte* Storage = nullptr;
		uint32_t Index = InvalidUint32();
	};

	inline ArenaAllocation AllocateStorage(ArenaHeader& Arena)
	{
		ArenaAllocation Allocation;

		uint32_t RecycleStackTop = Arena.RecycleStackTop.load();
		while (std::byte* AllocationStorage = GetArenaAllocationStorage(Arena, RecycleStackTop))
		{
			const uint32_t RecycleStackNext = ReadUint32(AllocationStorage);
			if (Arena.RecycleStackTop.compare_exchange_strong(RecycleStackTop, RecycleStackNext))
			{
				Allocation.Storage = AllocationStorage;
				Allocation.Index = RecycleStackTop;
				return Allocation;
			}
			RecycleStackTop = Arena.RecycleStackTop.load();
		}

		const uint32_t NextIndex = Arena.NextIndex.fetch_add(1);
		ArenaLocation Location = GetArenaLocation(Arena, NextIndex);
		const uint64_t BlockSize = GetBlockSize(Arena) + (Location.Block == 0) * GetArenaStartOffset(Arena);
		if (Location.Block >= ArenaHeader::MaxBlocks || Location.Byte >= BlockSize)
		{
			MemoryError("AllocateStorage couldn't get a location to allocate storage");
			return Allocation;
		}

		std::byte* Block = GetArenaBlock(Arena, Location);
		if (!Block)
		{
			if (Location.Block == 0)
			{
				MemoryError("AllocateStorage couldn't get the first block???");
				return Allocation;
			}

			const uint64_t Alignment = Arena.AlignBytes > 8ull ? Arena.AlignBytes : 8ull;
			const uint64_t AlignmentMask = Alignment - 1ull;
			const uint64_t AlignmentPadding = (Alignment - (BlockSize & AlignmentMask)) & AlignmentMask;
			Block = static_cast<std::byte*>(std::aligned_alloc(Alignment, BlockSize + AlignmentPadding));
			if (!Block)
			{
				MemoryError("AllocateStorage couldn't allocate a new block");
				return Allocation;
			}

			std::byte* NullStdBytePtr = nullptr;
			if (!Arena.BlockArray[Location.Block - 1].compare_exchange_strong(NullStdBytePtr, Block))
			{
				std::free(Block);
				Block = Arena.BlockArray[Location.Block - 1].load();
				if (!Block)
				{
					MemoryError("AllocateStorage CAX failed to find null, but then load returned null");
					return Allocation;
				}
			}
		}

		Allocation.Storage = Block + Location.Byte;
		Allocation.Index = NextIndex;
		return Allocation;
	}

	inline void RecycleStorage(ArenaHeader& Arena, uint32_t Index)
	{
		if (std::byte* Allocation = GetArenaAllocationStorage(Arena, Index))
		{
			uint32_t RecycleStackTop = InvalidUint32();
			do
			{
				RecycleStackTop = Arena.RecycleStackTop.load();
				WriteUint32(Allocation, RecycleStackTop);
			} while (!Arena.RecycleStackTop.compare_exchange_weak(RecycleStackTop, Index));
		}
	}

	struct CachedArenaAllocatorTracking
	{
		CachedArenaAllocatorTracking(uint32_t NumReferencesInit = 0, uint32_t SerialInit = InvalidUint32(), uint32_t AllocationIndexInit = InvalidUint32())
			: NumReferences(NumReferencesInit), Serial(SerialInit), AllocationIndex(AllocationIndexInit)
		{
		}
		std::atomic<uint32_t> NumReferences;
		uint32_t Serial;
		uint32_t AllocationIndex;
	};

	class CachedArenaAllocator
	{
	private:
		DEFINE_STATIC_PROPERTY(std::shared_mutex, AllocationMutex);
		DEFINE_STATIC_PROPERTY(std::mutex, ArenaHeadersMutex);
		DEFINE_STATIC_PROPERTY(std::mutex, TrackingArenaMutex);
		DEFINE_STATIC_PROPERTY(std::atomic<ArenaHeader*>*, ArenaHeaders, nullptr);
		DEFINE_STATIC_PROPERTY(std::atomic<uint32_t>, NumArenas, 0);
		DEFINE_STATIC_PROPERTY(std::atomic<uint32_t>, NextSerial, 0);
		DEFINE_STATIC_PROPERTY(ArenaHeader*, TrackingArena, nullptr);

		DEFINE_THREADLOCAL_PROPERTY(uint32_t, ThreadCacheLength, 0);
		DEFINE_THREADLOCAL_PROPERTY(uint32_t, ThreadCacheMax, 0);
		DEFINE_THREADLOCAL_PROPERTY(BytePtr, ThreadCacheBytePtr, nullptr);

		static std::byte* GetThreadCacheStorage() noexcept
		{
			return GetThreadCacheBytePtr().Get();
		}

		static AllocationCacheItem* GetThreadCache() noexcept
		{
			if (GetThreadCacheLength() > 0)
			{
				return reinterpret_cast<AllocationCacheItem*>(GetThreadCacheStorage());
			}
			return nullptr;
		}

		static uint32_t IncrementThreadCacheLength()
		{
			const uint32_t LastIndex = GetThreadCacheLength()++;

			if (LastIndex >= GetThreadCacheMax())
			{
				const size_t OldStorageSize = static_cast<size_t>(GetThreadCacheMax()) * sizeof(AllocationCacheItem);
				GetThreadCacheMax() = GetThreadCacheMax() ? GetThreadCacheMax() << 1 : ThreadCacheInitialMax;
				const size_t NewStorageSize = static_cast<size_t>(GetThreadCacheMax()) * sizeof(AllocationCacheItem);

				BytePtr NewStorage = static_cast<std::byte*>(std::aligned_alloc(ThreadCacheInitialMax, NewStorageSize));
				if (!NewStorage)
				{
					MemoryError("CachedArenaAllocator::IncrementThreadCacheLength couldn't allocate ", NewStorageSize, " bytes with alignment ", ThreadCacheInitialMax);
					return InvalidUint32();
				}

				if (BytePtr OldStorage = std::move(GetThreadCacheBytePtr()))
				{
					std::memcpy(NewStorage, OldStorage, OldStorageSize);
				}

				GetThreadCacheBytePtr() = std::move(NewStorage);
			}

			return LastIndex;
		}

		static void InitArenaHeaders()
		{
			if (!GetArenaHeaders())
			{
				std::scoped_lock<std::mutex> Lock(GetArenaHeadersMutex());
				if (!GetArenaHeaders())
				{
					std::byte* ArenaHeadersStorage = static_cast<std::byte*>(std::aligned_alloc(alignof(std::atomic<ArenaHeader*>), MaxArenas * sizeof(std::atomic<ArenaHeader*>)));
					if (!ArenaHeadersStorage)
					{
						MemoryError("CachedArenaAllocator::InitArenaHeaders couldn't allocate storage for arena headers");
						return;
					}
					for (size_t ArenaIndex = 0; ArenaIndex < MaxArenas; ++ArenaIndex)
					{
						std::byte* ArenaHeaderStorage = ArenaHeadersStorage + ArenaIndex * sizeof(std::atomic<ArenaHeader*>);
						new(ArenaHeaderStorage) std::atomic<ArenaHeader*>(nullptr);
					}
					GetArenaHeaders() = reinterpret_cast<std::atomic<ArenaHeader*>*>(ArenaHeadersStorage);
				}
			}
		}

		static uint32_t FindArenaIndex(uint32_t NumBytes, uint32_t AlignBytes)
		{
			// NumArenas will never decrease until tear down
			const uint32_t NumArenas = GetNumArenas().load();
			for (uint32_t ArenaIndex = 0; ArenaIndex < NumArenas; ++ArenaIndex)
			{
				const ArenaHeader& Arena = *GetArenaHeaders()[ArenaIndex].load();
				if (Arena.NumBytes == NumBytes && Arena.AlignBytes == AlignBytes)
				{
					return ArenaIndex;
				}
			}

			if (NumArenas >= MaxArenas)
			{
				MemoryError("CachedArenaAllocator::FindArenaIndex arenas maxed out");
				return InvalidUint32();
			}

			// Need a new arena
			InitArenaHeaders();
			const uint64_t Alignment = AlignBytes > alignof(ArenaHeader) ? AlignBytes : alignof(ArenaHeader);
			const uint64_t AlignmentMask = Alignment - 1ull;
			const uint64_t HeaderBlockSize = GetBlockSize(NumBytes) + GetArenaStartOffset(AlignBytes);
			const uint64_t AlignmentPadding = (Alignment - (HeaderBlockSize & AlignmentMask)) & AlignmentMask;
			std::byte* ArenaHeaderBlock = static_cast<std::byte*>(std::aligned_alloc(Alignment, HeaderBlockSize + AlignmentPadding));
			if (!ArenaHeaderBlock)
			{
				MemoryError("CachedArenaAllocator::FindArenaIndex couldn't allocate an arena");
				return InvalidUint32();
			}

			ArenaHeader* Arena = new(ArenaHeaderBlock) ArenaHeader(NumBytes, AlignBytes);
			uint32_t ArenaIndex = NumArenas;
			ArenaHeader* NullArenaHeaderPtr = nullptr;
			while (!GetArenaHeaders()[ArenaIndex].compare_exchange_strong(NullArenaHeaderPtr, Arena))
			{
				ArenaHeader* AddedArena = GetArenaHeaders()[ArenaIndex].load();
				if (AddedArena->NumBytes == NumBytes && AddedArena->AlignBytes == AlignBytes)
				{
					Arena->~ArenaHeader();
					std::free(ArenaHeaderBlock);
					return ArenaIndex;
				}
				if (++ArenaIndex == MaxArenas)
				{
					Arena->~ArenaHeader();
					std::free(ArenaHeaderBlock);
					MemoryError("CachedArenaAllocator::FindArenaIndex ran out of arenas for ", NumBytes, "-byte ", AlignBytes, "-aligned allocations");
					return InvalidUint32();
				}
			}

			GetNumArenas().fetch_add(1);

			return ArenaIndex;
		}

		static uint32_t GetArenaIndex(uint32_t NumBytes, uint32_t AlignBytes)
		{
			uint32_t ThreadCacheIndex = InvalidUint32();
			for (uint32_t ReverseCacheIndex = 0; ReverseCacheIndex < GetThreadCacheLength(); ++ReverseCacheIndex)
			{
				const uint32_t CurrentCacheIndex = GetThreadCacheLength() - 1 - ReverseCacheIndex;
				AllocationCacheItem& CacheItem = GetThreadCache()[CurrentCacheIndex];
				if (CacheItem.NumBytes == NumBytes && CacheItem.AlignBytes == AlignBytes)
				{
					ThreadCacheIndex = CurrentCacheIndex;
					break;
				}
			}
			if (ThreadCacheIndex >= GetThreadCacheLength())
			{
				ThreadCacheIndex = IncrementThreadCacheLength();
				new(GetThreadCacheStorage() + ThreadCacheIndex * sizeof(AllocationCacheItem)) AllocationCacheItem(NumBytes, AlignBytes);
				GetThreadCache()[ThreadCacheIndex].ArenaIndex = FindArenaIndex(NumBytes, AlignBytes);
			}
			else if (ThreadCacheIndex + 1 < GetThreadCacheLength())
			{
				const auto CacheItem = GetThreadCache()[ThreadCacheIndex];
				for (; ThreadCacheIndex + 1 < GetThreadCacheLength(); ++ThreadCacheIndex)
				{
					GetThreadCache()[ThreadCacheIndex] = GetThreadCache()[ThreadCacheIndex + 1];
				}
				GetThreadCache()[ThreadCacheIndex] = CacheItem;
			}
			return GetThreadCache()[ThreadCacheIndex].ArenaIndex;
		}

		static uint32_t NewTrackingIndex(uint32_t AllocationIndex, uint32_t Serial)
		{
			ArenaHeader* TrackingArena = GetTrackingArena();
			if (!TrackingArena)
			{
				std::scoped_lock<std::mutex> Lock(GetTrackingArenaMutex());
				TrackingArena = GetTrackingArena();
				if (!TrackingArena)
				{
					constexpr uint64_t SizeOfTracking = sizeof(CachedArenaAllocatorTracking);
					constexpr uint64_t AlignOfTracking = alignof(CachedArenaAllocatorTracking);
					constexpr uint64_t Alignment = AlignOfTracking > alignof(ArenaHeader) ? AlignOfTracking : alignof(ArenaHeader);
					constexpr uint64_t AlignmentMask = Alignment - 1ull;
					constexpr uint64_t HeaderBlockSize = GetBlockSize(SizeOfTracking) + GetArenaStartOffset(AlignOfTracking);
					constexpr uint64_t AlignmentPadding = (Alignment - (HeaderBlockSize & AlignmentMask)) & AlignmentMask;
					std::byte* TrackingArenaStorage = static_cast<std::byte*>(std::aligned_alloc(Alignment, HeaderBlockSize + AlignmentPadding));
					if (!TrackingArenaStorage)
					{
						MemoryError("CachedArenaAllocator::NewTrackingIndex couldn't allocate storage for the tracking arena");
						return InvalidUint32();
					}
					TrackingArena = new(TrackingArenaStorage) ArenaHeader(SizeOfTracking, AlignOfTracking);
					GetTrackingArena() = TrackingArena;
				}
			}

			ArenaAllocation TrackingAllocation = AllocateStorage(*TrackingArena);
			if (!TrackingAllocation.Storage)
			{
				MemoryError("CachedArenaAllocator::NewTrackingIndex couldn't allocate tracking storage in the tracking arena");
				return InvalidUint32();
			}

			new(TrackingAllocation.Storage) CachedArenaAllocatorTracking(0, Serial, AllocationIndex);
			return TrackingAllocation.Index;
		}

	public:
		static constexpr size_t MaxArenasLog2 = 12ull;
		static constexpr size_t MaxArenas = 1ull << MaxArenasLog2;
		static constexpr size_t ThreadCacheInitialMaxLog2 = 12ull;
		static constexpr size_t ThreadCacheInitialMax = 1ull << ThreadCacheInitialMaxLog2;
		static constexpr size_t MaxObjects = (ArenaHeader::MaxBlocks * ArenaHeader::BlockSize - ArenaHeader::HeaderSize) / sizeof(CachedArenaAllocatorTracking);

		static AllocationTransaction Allocate(uint32_t NumBytes, uint32_t AlignBytes)
		{
			AllocationTransaction AllocationInfo(GetAllocationMutex());

			const uint32_t AlignMask = AlignBytes - 1;
			if (AlignBytes & AlignMask)
			{
				MemoryError("CachedArenaAllocator::Allocate requires power-of-2 alignment. Alignment was ", AlignBytes, ".");
				return AllocationInfo;
			}

			// Minimum size is 4 bytes, so that recycled allocations can store the next index in the free list
			NumBytes = NumBytes < 4ull ? 4ull : NumBytes;

			// Ensure allocation is a multiple of the alignment
			NumBytes = NumBytes + ((AlignBytes - (NumBytes & AlignMask)) & AlignMask);

			AllocationInfo.NumBytes = NumBytes;
			AllocationInfo.AlignBytes = AlignBytes;

			// Find an arena
			const uint32_t ArenaIndex = GetArenaIndex(NumBytes, AlignBytes);
			ArenaHeader* Arena = ArenaIndex < MaxArenas ? GetArenaHeaders()[ArenaIndex].load() : nullptr;
			if (!Arena)
			{
				MemoryError("CachedArenaAllocator::Allocate couldn't get an arena");
				return AllocationInfo;
			}

			// Allocate storage
			ArenaAllocation Allocation = AllocateStorage(*Arena);
			if (!Allocation.Storage)
			{
				MemoryError("CachedArenaAllocator::Allocate couldn't allocate storage");
				return AllocationInfo;
			}
			else if (Allocation.Index == InvalidUint32())
			{
				MemoryError("CachedArenaAllocator::Allocate allocated storage but got an invalid index");
				return AllocationInfo;
			}
			AllocationInfo.Storage = Allocation.Storage;

			// Increment serial
			AllocationInfo.ObjectSerial = GetNextSerial().fetch_add(1);

			// Add tracking
			AllocationInfo.TrackingIndex = NewTrackingIndex(Allocation.Index, AllocationInfo.ObjectSerial);

			return AllocationInfo;
		}

		static void RecycleAllocation(uint32_t NumBytes, uint32_t AlignBytes, uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			const uint32_t ArenaIndex = GetArenaIndex(NumBytes, AlignBytes);
			ArenaHeader* Arena = ArenaIndex < MaxArenas ? GetArenaHeaders()[ArenaIndex].load() : nullptr;
			if (!Arena)
			{
				MemoryError("CachedArenaAllocator::RecycleAllocation couldn't get an arena");
				return;
			}

			ArenaHeader* TrackingArena = GetTrackingArena();
			if (!TrackingArena)
			{
				MemoryError("CachedArenaAllocator::RecycleAllocation couldn't get the tracking arena");
				return;
			}

			CachedArenaAllocatorTracking* Tracking = reinterpret_cast<CachedArenaAllocatorTracking*>(GetArenaAllocationStorage(*TrackingArena, TrackingIndex));
			if (Tracking && Tracking->Serial == ObjectSerial)
			{
				const uint32_t AllocationIndex = Tracking->AllocationIndex;
				Tracking->~CachedArenaAllocatorTracking();
				RecycleStorage(*TrackingArena, TrackingIndex);
				RecycleStorage(*Arena, AllocationIndex);
			}
		}

		static uint32_t IncrementNumReferences(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			if (ArenaHeader* TrackingArena = GetTrackingArena())
			{
				CachedArenaAllocatorTracking* Tracking = reinterpret_cast<CachedArenaAllocatorTracking*>(GetArenaAllocationStorage(*TrackingArena, TrackingIndex));
				if (Tracking && Tracking->Serial == ObjectSerial)
				{
					return Tracking->NumReferences++;
				}
			}
			return InvalidUint32();
		}

		static uint32_t DecrementNumReferences(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			if (ArenaHeader* TrackingArena = GetTrackingArena())
			{
				CachedArenaAllocatorTracking* Tracking = reinterpret_cast<CachedArenaAllocatorTracking*>(GetArenaAllocationStorage(*TrackingArena, TrackingIndex));
				if (Tracking && Tracking->Serial == ObjectSerial)
				{
					return Tracking->NumReferences--;
				}
			}
			return InvalidUint32();
		}

		static uint32_t GetNumReferences(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			if (ArenaHeader* TrackingArena = GetTrackingArena())
			{
				CachedArenaAllocatorTracking* Tracking = reinterpret_cast<CachedArenaAllocatorTracking*>(GetArenaAllocationStorage(*TrackingArena, TrackingIndex));
				if (Tracking && Tracking->Serial == ObjectSerial)
				{
					return Tracking->NumReferences;
				}
			}
			return InvalidUint32();
		}

		static constexpr bool HasGetStartByte() { return false; }
		static size_t GetStartByte(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			return InvalidSize();
		}

		static void TearDown()
		{
			std::unique_lock<std::shared_mutex> AllocationLock(GetAllocationMutex());
			std::unique_lock<std::mutex> ArenaHeadersLock(GetArenaHeadersMutex());
			std::unique_lock<std::mutex> TrackingArenaLock(GetTrackingArenaMutex());

			if (ArenaHeader* TrackingArena = GetTrackingArena())
			{
				TrackingArena->~ArenaHeader();
				std::free(TrackingArena);
				GetTrackingArena() = nullptr;
			}

			if (std::atomic<ArenaHeader*>* ArenaHeaders = GetArenaHeaders())
			{
				const uint32_t NumArenas = GetNumArenas().load();
				for (uint32_t ReverseIndex = 0; ReverseIndex < NumArenas; ++ReverseIndex)
				{
					const uint32_t ArenaIndex = NumArenas - 1 - ReverseIndex;
					ArenaHeader* Arena = ArenaHeaders[ArenaIndex].load();
					if (Arena && ArenaHeaders[ArenaIndex].compare_exchange_strong(Arena, nullptr))
					{
						Arena->~ArenaHeader();
						std::free(Arena);
					}
				}
				for (uint32_t ReverseIndex = 0; ReverseIndex < MaxArenas; ++ReverseIndex)
				{
					const uint32_t ArenaIndex = MaxArenas - 1 - ReverseIndex;
					ArenaHeaders[ArenaIndex].~atomic<ArenaHeader*>();
				}
				std::free(ArenaHeaders);
				GetArenaHeaders() = nullptr;
			}
		}
	};
}

