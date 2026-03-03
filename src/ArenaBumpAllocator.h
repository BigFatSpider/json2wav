// Copyright Dan Price 2026.

#pragma once

#include "MemoryCommon.h"
#include "Logging.h"
#include "Macros.h"
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <string>
#include <new>
#include <cstdint>
#include <cstddef>
#include <cstdlib>

namespace json2wav
{
	struct ArenaBumpAllocatorTracking
	{
		ArenaBumpAllocatorTracking()
			: StartByte(InvalidSize()), NumBytes(0), AlignBytes(0), NumReferences(0), Serial(InvalidUint32())
		{
		}
		ArenaBumpAllocatorTracking(size_t StartByteInit, uint32_t NumBytesInit, uint32_t AlignBytesInit)
			: StartByte(StartByteInit), NumBytes(NumBytesInit), AlignBytes(AlignBytesInit), NumReferences(0), Serial(InvalidUint32())
		{
		}
		size_t StartByte = InvalidSize();
		uint32_t NumBytes = 0;
		uint32_t AlignBytes = 0;
		std::atomic<uint32_t> NumReferences = 0;
		uint32_t Serial = InvalidUint32();
	};

	struct AllocationRecycle
	{
		uint32_t NumBytes = 0;
		uint32_t AlignBytes = 0;
		uint32_t TrackingIndex = InvalidUint32();
	};

	struct IndexSerial
	{
		uint32_t Index = InvalidUint32();
		uint32_t Serial = InvalidUint32();
	};

	class ArenaBumpAllocator
	{
	private:
		static std::byte** GetBlocks()
		{
			static std::byte* Blocks[NumBlocks] = { 0 };
			return &Blocks[0];
		}

		DEFINE_STATIC_PROPERTY(size_t, NextByte, 0);
		DEFINE_STATIC_PROPERTY(std::byte*, TrackingBlock, nullptr);
		DEFINE_STATIC_PROPERTY(std::byte*, RecycleBlock, nullptr);
		DEFINE_STATIC_PROPERTY(std::atomic<uint32_t>, TrackingCount, 0);
		DEFINE_STATIC_PROPERTY(std::atomic<uint32_t>, RecycleCount, 0);
		DEFINE_STATIC_PROPERTY(std::mutex, NextByteMutex);
		DEFINE_STATIC_PROPERTY(std::mutex, BlocksMutex);
		DEFINE_STATIC_PROPERTY(std::mutex, RecycleMutex);
		DEFINE_STATIC_PROPERTY(std::shared_mutex, AllocationMutex);

		static uint32_t NextSerial()
		{
			static std::atomic<uint32_t> Serial = 0;
			return Serial.fetch_add(1);
		}

		static void RemoveRecycleStackIndexUnsafe(AllocationRecycle* RecycleStack, uint32_t RecycleStackLength, uint32_t RecycleStackIndex)
		{
			// The recycle mutex must be locked during this function's execution

			if (RecycleStack && RecycleStackIndex < RecycleStackLength)
			{
				for (uint32_t NextStackIndex = RecycleStackIndex + 1; NextStackIndex < RecycleStackLength; ++NextStackIndex)
				{
					const uint32_t CurrentStackIndex = NextStackIndex - 1;
					RecycleStack[CurrentStackIndex] = RecycleStack[NextStackIndex];
				}
				RecycleStack[RecycleStackLength - 1].~AllocationRecycle();
				const uint32_t RecycleStackSize = GetRecycleCount().fetch_sub(1);
				LOG_MEMORY(Tracking, "Recycle stack size is ", RecycleStackSize - 1);
			}
		}

		static uint32_t ReuseAllocation(uint32_t NumBytes, uint32_t AlignBytes)
		{
			uint32_t TrackingIndex = InvalidUint32();

			std::unique_lock<std::mutex> Lock(GetRecycleMutex());

			std::byte* RecycleBlock = GetRecycleBlock();
			if (RecycleBlock)
			{
				AllocationRecycle* RecycleStack = reinterpret_cast<AllocationRecycle*>(RecycleBlock);
				const uint32_t RecycleStackLength = GetRecycleCount().load();

				for (uint32_t FromTop = 0; FromTop < RecycleStackLength; ++FromTop)
				{
					const uint32_t RecycleStackIndex = RecycleStackLength - 1 - FromTop;
					AllocationRecycle& RecycleFrame = RecycleStack[RecycleStackIndex];
					const bool bAllocationMatches = RecycleFrame.NumBytes == NumBytes && RecycleFrame.AlignBytes == AlignBytes;
					if (bAllocationMatches)
					{
						TrackingIndex = RecycleFrame.TrackingIndex;
						RemoveRecycleStackIndexUnsafe(RecycleStack, RecycleStackLength, RecycleStackIndex);
						LOG_MEMORY(Allocation, "Reusing ", AlignBytes, " byte aligned ", NumBytes, " byte allocation with tracking index ", TrackingIndex);
						break;
					}
				}
			}

			return TrackingIndex;
		}

		static size_t ReserveBytes(uint32_t NumBytes, uint32_t AlignBytes)
		{
			std::unique_lock<std::mutex> Lock(GetNextByteMutex());

			const size_t Alignment = AlignBytes;
			const size_t NextByte = GetNextByte();
			size_t StartByte = NextByte + ((Alignment - (NextByte & (Alignment - 1))) & (Alignment - 1));
			const size_t LastByte = StartByte + NumBytes - 1;
			const size_t StartBlock = StartByte >> BlockSizeLog2;
			const size_t LastBlock = LastByte >> BlockSizeLog2;
			if (StartBlock != LastBlock)
			{
				// Start at the beginning of the next block
				StartByte = LastBlock << BlockSizeLog2;
			}
			LOG_MEMORY(Allocation, "Allocating ", NumBytes, " bytes with ", AlignBytes, " byte alignment to byte number ", StartByte);
			GetNextByte() = StartByte + NumBytes;

			return StartByte;
		}

	public:
		static constexpr size_t BlockSizeLog2 = 24ull;
		static constexpr size_t NumBlocksLog2 = 8ull;
		static constexpr size_t BlockSize = 1ull << BlockSizeLog2;
		static constexpr size_t NumBlocks = 1ull << NumBlocksLog2;
		static constexpr size_t TrackingBlockSize = BlockSize << 6ull;
		static constexpr size_t RecycleBlockSize = TrackingBlockSize;
		static constexpr size_t MaxByte = 1ull << (BlockSizeLog2 + NumBlocksLog2);
		static constexpr size_t MaxObjects = TrackingBlockSize / sizeof(ArenaBumpAllocatorTracking);

		static AllocationTransaction Allocate(uint32_t NumBytes, uint32_t AlignBytes)
		{
			AllocationTransaction Allocation(GetAllocationMutex());

			LOG_MEMORY(Allocation, "Allocating ", NumBytes, " bytes with ", AlignBytes, " byte alignment");

			if (NumBytes > BlockSize)
			{
				std::string requirements("allocations to be " + std::to_string(BlockSize) + " bytes or less, but allocation requested was " + std::to_string(NumBytes) + " bytes");
				MemoryError(("ArenaBumpAllocator::Allocate requires " + requirements).c_str());
				return Allocation;
			}

			// Verify power-of-2 alignment
			if (AlignBytes & (AlignBytes - 1))
			{
				MemoryError("ArenaBumpAllocator::Allocate requires power-of-2 alignment");
				return Allocation;
			}

			// Verify size is a multiple of alignment
			if (NumBytes & (AlignBytes - 1))
			{
				MemoryError("ArenaBumpAllocator::Allocate requires size to be a multiple of alignment");
				return Allocation;
			}

			size_t StartByte = InvalidSize();
			const uint32_t TrackingIndex = ReuseAllocation(NumBytes, AlignBytes);
			if (TrackingIndex < GetTrackingCount().load())
			{
				if (std::byte* TrackingBlock = GetTrackingBlock())
				{
					ArenaBumpAllocatorTracking& Tracking = *reinterpret_cast<ArenaBumpAllocatorTracking*>(TrackingBlock + TrackingIndex * sizeof(ArenaBumpAllocatorTracking));
					StartByte = Tracking.StartByte;
					Allocation.TrackingIndex = TrackingIndex;
				}
			}
			if (StartByte == InvalidSize())
			{
				StartByte = ReserveBytes(NumBytes, AlignBytes);
			}

			const size_t BlockIndex = StartByte >> BlockSizeLog2;
			const size_t ByteIndex = StartByte & (BlockSize - 1);
			if (BlockIndex >= NumBlocks)
			{
				MemoryError("ArenaBumpAllocator::Allocate has allocated too many blocks");
				std::unique_lock<std::mutex> Lock(GetNextByteMutex());
				GetNextByte() = MaxByte;
				return Allocation;
			}

			std::byte* Block = GetBlocks()[BlockIndex];
			std::byte* NextBlock = (BlockIndex + 1 < NumBlocks) ? GetBlocks()[BlockIndex + 1] : nullptr;

			if (!Block)
			{
				std::unique_lock<std::mutex> Lock(GetBlocksMutex());

				Block = GetBlocks()[BlockIndex];
				if (!Block)
				{
					LOG_MEMORY(Allocation, "Allocating ", BlockSize, " bytes with ", BlockSize, " byte alignment for block ", BlockIndex);
					Block = reinterpret_cast<std::byte*>(std::aligned_alloc(BlockSize, BlockSize));
					GetBlocks()[BlockIndex] = Block;
				}
			}

			if (!NextBlock && BlockIndex + 1 < NumBlocks)
			{
				std::unique_lock<std::mutex> Lock(GetBlocksMutex());

				NextBlock = GetBlocks()[BlockIndex + 1];
				if (!NextBlock)
				{
					LOG_MEMORY(Allocation, "Allocating ", BlockSize, " bytes with ", BlockSize, " byte alignment for block ", BlockIndex + 1);
					NextBlock = reinterpret_cast<std::byte*>(std::aligned_alloc(BlockSize, BlockSize));
					GetBlocks()[BlockIndex + 1] = NextBlock;
				}
			}

			if (Block)
			{
				Allocation.Storage = Block + ByteIndex;
				Allocation.StartByte = StartByte;
				Allocation.NumBytes = NumBytes;
				Allocation.AlignBytes = AlignBytes;

				const IndexSerial Tracking = TrackAllocation(Allocation);
				Allocation.TrackingIndex = Tracking.Index;
				Allocation.ObjectSerial = Tracking.Serial;
			}

			return Allocation;
		}

		static IndexSerial TrackAllocation(const AllocationTransaction& Allocation)
		{
			LOG_MEMORY(Allocation, "Tracking allocation at byte number ", Allocation.StartByte);

			if (Allocation.StartByte == InvalidSize())
			{
				MemoryError("ArenaBumpAllocator::TrackAllocation requires an allocation to track");
				return IndexSerial();
			}

			std::byte* TrackingBlock = GetTrackingBlock();
			if (!TrackingBlock)
			{
				std::unique_lock<std::mutex> Lock(GetBlocksMutex());

				TrackingBlock = GetTrackingBlock();
				if (!TrackingBlock)
				{
					LOG_MEMORY(Allocation, "Allocating ", TrackingBlockSize, " bytes with ", TrackingBlockSize, " byte alignment for tracking block");
					TrackingBlock = reinterpret_cast<std::byte*>(std::aligned_alloc(TrackingBlockSize, TrackingBlockSize));
				}

				GetTrackingBlock() = TrackingBlock;
			}

			if (!TrackingBlock)
			{
				MemoryError("ArenaBumpAllocator::TrackAllocation could not allocate the tracking block");
				return IndexSerial();
			}

			ArenaBumpAllocatorTracking* Tracking = nullptr;
			uint32_t TrackingIndex = Allocation.TrackingIndex;
			if (TrackingIndex == InvalidUint32())
			{
				TrackingIndex = GetTrackingCount().fetch_add(1);
				LOG_MEMORY(Tracking, "Tracking list size is ", TrackingIndex + 1);
				if (TrackingIndex >= MaxObjects)
				{
					MemoryError("ArenaBumpAllocator::TrackAllocation is tracking too many objects");
					return IndexSerial();
				}

				LOG_MEMORY(Allocation, "Adding tracking at index ", TrackingIndex);
				std::byte* TrackingStorage = TrackingBlock + TrackingIndex * sizeof(ArenaBumpAllocatorTracking);
				Tracking = new(TrackingStorage) ArenaBumpAllocatorTracking(Allocation.StartByte, Allocation.NumBytes, Allocation.AlignBytes);
			}
			else
			{
				LOG_MEMORY(Allocation, "Reusing tracking at index ", TrackingIndex);
				std::byte* TrackingStorage = TrackingBlock + TrackingIndex * sizeof(ArenaBumpAllocatorTracking);
				Tracking = reinterpret_cast<ArenaBumpAllocatorTracking*>(TrackingStorage);
			}

			Tracking->Serial = NextSerial();
			return {TrackingIndex, Tracking->Serial};
		}

		static void RecycleAllocation(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			LOG_MEMORY(Allocation, "Recycling allocation with tracking index ", TrackingIndex);

			ArenaBumpAllocatorTracking* TrackingList = reinterpret_cast<ArenaBumpAllocatorTracking*>(GetTrackingBlock());
			if (!TrackingList)
			{
				MemoryError("ArenaBumpAllocator::RecycleAllocation needs a tracking list to recycle tracked allocations");
				return;
			}

			const uint32_t TrackingCount = GetTrackingCount().load();
			if (TrackingIndex >= TrackingCount)
			{
				MemoryError("ArenaBumpAllocator::RecycleAllocation needs a valid tracking index to recycle");
				return;
			}

			ArenaBumpAllocatorTracking& Tracking = TrackingList[TrackingIndex];
			if (Tracking.Serial != ObjectSerial)
			{
				MemoryError("ArenaBumpAllocator::RecycleAllocation needs serial numbers to match");
				return;
			}

			LOG_MEMORY(Allocation, "Recycling ", Tracking.AlignBytes, " byte aligned ", Tracking.NumBytes, " byte allocation with tracking index ", TrackingIndex);

			std::unique_lock<std::mutex> Lock(GetRecycleMutex());

			std::byte* RecycleBlock = GetRecycleBlock();
			if (!RecycleBlock)
			{
				LOG_MEMORY(Allocation, "Allocating ", RecycleBlockSize, " bytes with ", RecycleBlockSize, " byte alignment for recycle block");
				RecycleBlock = reinterpret_cast<std::byte*>(std::aligned_alloc(RecycleBlockSize, RecycleBlockSize));
				if (!RecycleBlock)
				{
					MemoryError("ArenaBumpAllocator::RecycleAllocation could not allocate the recycle block");
					return;
				}

				GetRecycleBlock() = RecycleBlock;
			}

			const uint32_t RecycleIndex = GetRecycleCount().fetch_add(1);
			if (RecycleIndex >= MaxObjects)
			{
				GetRecycleCount().fetch_sub(1);
				MemoryError("ArenaBumpAllocator::RecycleAllocation is recycling too many objects");
				return;
			}

			LOG_MEMORY(Tracking, "Recycle stack size is ", RecycleIndex + 1);
			std::byte* RecycleStorage = RecycleBlock + RecycleIndex * sizeof(AllocationRecycle);
			new(RecycleStorage) AllocationRecycle{Tracking.NumBytes, Tracking.AlignBytes, TrackingIndex};
		}

		static uint32_t IncrementNumReferences(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			ArenaBumpAllocatorTracking* TrackingList = reinterpret_cast<ArenaBumpAllocatorTracking*>(GetTrackingBlock());
			const uint32_t TrackingCount = GetTrackingCount().load();
			if (TrackingList && TrackingIndex < TrackingCount)
			{
				ArenaBumpAllocatorTracking& Tracking = TrackingList[TrackingIndex];
				if (Tracking.Serial == ObjectSerial)
				{
					return Tracking.NumReferences++;
				}
			}
			return InvalidUint32();
		}

		static uint32_t DecrementNumReferences(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			ArenaBumpAllocatorTracking* TrackingList = reinterpret_cast<ArenaBumpAllocatorTracking*>(GetTrackingBlock());
			const uint32_t TrackingCount = GetTrackingCount().load();
			if (TrackingList && TrackingIndex < TrackingCount)
			{
				ArenaBumpAllocatorTracking& Tracking = TrackingList[TrackingIndex];
				if (Tracking.Serial == ObjectSerial)
				{
					return Tracking.NumReferences--;
				}
			}
			return InvalidUint32();
		}

		static uint32_t GetNumReferences(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			ArenaBumpAllocatorTracking* TrackingList = reinterpret_cast<ArenaBumpAllocatorTracking*>(GetTrackingBlock());
			const uint32_t TrackingCount = GetTrackingCount().load();
			if (TrackingList && TrackingIndex < TrackingCount)
			{
				ArenaBumpAllocatorTracking& Tracking = TrackingList[TrackingIndex];
				if (Tracking.Serial == ObjectSerial)
				{
					return Tracking.NumReferences;
				}
			}
			return InvalidUint32();
		}

		static constexpr bool HasGetStartByte() { return true; }
		static size_t GetStartByte(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			ArenaBumpAllocatorTracking* TrackingList = reinterpret_cast<ArenaBumpAllocatorTracking*>(GetTrackingBlock());
			const uint32_t TrackingCount = GetTrackingCount().load();
			if (TrackingList && TrackingIndex < TrackingCount)
			{
				ArenaBumpAllocatorTracking& Tracking = TrackingList[TrackingIndex];
				if (Tracking.Serial == ObjectSerial)
				{
					return Tracking.StartByte;
				}
			}
			return InvalidSize();
		}

		static void TearDown()
		{
			std::unique_lock<std::shared_mutex> AllocationLock(GetAllocationMutex());
			std::unique_lock<std::mutex> BlocksLock(GetBlocksMutex());
			std::unique_lock<std::mutex> RecycleLock(GetRecycleMutex());

			// Destroy recycle stack in reverse order
			if (AllocationRecycle* RecycleStack = reinterpret_cast<AllocationRecycle*>(GetRecycleBlock()))
			{
				const uint32_t RecycleCount = GetRecycleCount().load();
				for (uint32_t RecycleReverseIndex = 0; RecycleReverseIndex < RecycleCount; ++RecycleReverseIndex)
				{
					const uint32_t RecycleIndex = RecycleCount - 1 - RecycleReverseIndex;
					AllocationRecycle& Recycle = RecycleStack[RecycleIndex];
					Recycle.~AllocationRecycle();
				}
				std::free(RecycleStack);
				GetRecycleBlock() = nullptr;
				GetRecycleCount().store(0);
			}

			// Destroy tracking in reverse order
			if (ArenaBumpAllocatorTracking* TrackingList = reinterpret_cast<ArenaBumpAllocatorTracking*>(GetTrackingBlock()))
			{
				const uint32_t TrackingCount = GetTrackingCount().load();
				for (uint32_t TrackingReverseIndex = 0; TrackingReverseIndex < TrackingCount; ++TrackingReverseIndex)
				{
					const uint32_t TrackingIndex = TrackingCount - 1 - TrackingReverseIndex;
					ArenaBumpAllocatorTracking& Tracking = TrackingList[TrackingIndex];
					Tracking.~ArenaBumpAllocatorTracking();
				}
				std::free(TrackingList);
				GetTrackingBlock() = nullptr;
				GetTrackingCount().store(0);
			}

			// Deallocate blocks in reverse order
			for (size_t BlockReverseIndex = 0; BlockReverseIndex < NumBlocks; ++BlockReverseIndex)
			{
				const size_t BlockIndex = NumBlocks - 1 - BlockReverseIndex;
				if (std::byte* Block = GetBlocks()[BlockIndex])
				{
					std::free(Block);
					GetBlocks()[BlockIndex] = nullptr;
				}
			}
		}
	};
}

