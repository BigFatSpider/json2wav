// Copyright Dan Price 2026.

#pragma once

#include <vector>
#include <array>
#include <memory>
#include <utility>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <new>
#include <string>
#include <stdexcept>
#include <fstream>
#include <type_traits>
#include <algorithm>
#include <cstdlib>
#include <cstddef>

namespace json2wav
{
	inline void LogMemoryToFile(const std::string& Message)
	{
		static std::mutex MemoryLogMutex;
		std::scoped_lock<std::mutex> Lock(MemoryLogMutex);
		static std::ios::openmode OpenMode = std::ios::out;
		std::ofstream LogFile("json2wav-memory.log", OpenMode);
		OpenMode |= std::ios::app;
		LogFile << Message << '\n';
	}

	template<bool bIsArithmetic> struct IsArithmetic;
	template<> struct IsArithmetic<false>
	{
		template<typename T>
		static std::string ToString(const T& Snippet)
		{
			std::string StdString(Snippet);
			return StdString;
		}
	};
	template<> struct IsArithmetic<true>
	{
		template<typename T>
		static std::string ToString(const T& Snippet)
		{
			return std::to_string(Snippet);
		}
	};

	template<typename T>
	inline std::string ToString(const T& Snippet)
	{
		return IsArithmetic<std::is_arithmetic_v<T>>::ToString(Snippet);
	}

	template<typename T, typename... Ts>
	struct VarArgs
	{
		static std::string ConcatStrings(const T& Arg, const Ts&... Args)
		{
			return ToString(Arg) + VarArgs<Ts...>::ConcatStrings(Args...);
		}
	};

	template<typename T>
	struct VarArgs<T>
	{
		static std::string ConcatStrings(const T& Arg)
		{
			return ToString(Arg);
		}
	};

	enum class EMemoryLogChannel
	{
		Allocation,
		Tracking,
		Error
	};

	template<EMemoryLogChannel Channel> struct MemoryLogChannelProperties;
	template<> struct MemoryLogChannelProperties<EMemoryLogChannel::Allocation>
	{
		static constexpr bool IsActive = false;
		static constexpr const char* Name = "Allocation";
	};
	template<> struct MemoryLogChannelProperties<EMemoryLogChannel::Tracking>
	{
		static constexpr bool IsActive = false;
		static constexpr const char* Name = "Tracking";
	};
	template<> struct MemoryLogChannelProperties<EMemoryLogChannel::Error>
	{
		static constexpr bool IsActive = true;
		static constexpr const char* Name = "Error";
	};
	template<EMemoryLogChannel Channel>
	constexpr bool IsMemoryLogChannelActive = MemoryLogChannelProperties<Channel>::IsActive;
	template<EMemoryLogChannel Channel>
	constexpr const char* MemoryLogChannelName = MemoryLogChannelProperties<Channel>::Name;

	template<bool bChannelActive> struct LogMemoryIfActive;
	template<> struct LogMemoryIfActive<true>
	{
		template<typename... Ts>
		static void Log(const Ts&... Snippets)
		{
			LogMemoryToFile(VarArgs<Ts...>::ConcatStrings(Snippets...));
		}
	};
	template<> struct LogMemoryIfActive<false>
	{
		template<typename... Ts>
		static void Log(const Ts&... Snippets) {}
	};

	template<EMemoryLogChannel Channel, typename... Ts>
	inline void LogMemory(const Ts&... Snippets)
	{
		LogMemoryIfActive<IsMemoryLogChannelActive<Channel>>::Log(MemoryLogChannelName<Channel>, ": ", Snippets...);
	}

	inline void MemoryError(const std::string& Message)
	{
		LogMemory<EMemoryLogChannel::Error>(Message);
		throw std::runtime_error(Message.c_str());
	}

	inline constexpr size_t InvalidSize() { return static_cast<size_t>(-1); }
	inline constexpr uint32_t InvalidUint32() { return static_cast<uint32_t>(-1); }

	struct AllocationTracking
	{
		AllocationTracking()
			: StartByte(InvalidSize()), NumBytes(0), AlignBytes(0), NumReferences(0), Serial(InvalidUint32())
		{
		}
		AllocationTracking(size_t StartByteInit, uint32_t NumBytesInit, uint32_t AlignBytesInit)
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
		size_t StartByte = InvalidSize();
		uint32_t NumBytes = 0;
		uint32_t AlignBytes = 0;
		uint32_t TrackingIndex = InvalidUint32();
	};

	struct IndexSerial
	{
		uint32_t Index = InvalidUint32();
		uint32_t Serial = InvalidUint32();
	};

	struct AllocationTransaction
	{
		explicit AllocationTransaction(std::shared_mutex& Mutex, std::byte* StorageInit = nullptr)
			: Lock(Mutex), Storage(StorageInit), StartByte(InvalidSize()), NumBytes(0), AlignBytes(0), TrackingIndex(InvalidUint32())
		{
		}
		std::shared_lock<std::shared_mutex> Lock;
		std::byte* Storage;
		size_t StartByte;
		uint32_t NumBytes;
		uint32_t AlignBytes;
		uint32_t TrackingIndex;
	};

	class ArenaBumpAllocator
	{
	private:
		static size_t& GetNextByte()
		{
			static size_t NextByte = 0;
			return NextByte;
		}

		static std::mutex& GetNextByteMutex()
		{
			static std::mutex NextByteMutex;
			return NextByteMutex;
		}

		static std::byte** GetBlocks()
		{
			static std::byte* Blocks[NumBlocks] = { 0 };
			return &Blocks[0];
		}

		static std::byte*& GetTrackingBlock()
		{
			static std::byte* Block = nullptr;
			return Block;
		}

		static std::atomic<uint32_t>& GetTrackingCount()
		{
			static std::atomic<uint32_t> Count = 0;
			return Count;
		}

		static std::byte*& GetRecycleBlock()
		{
			static std::byte* Block = nullptr;
			return Block;
		}

		static std::atomic<uint32_t>& GetRecycleCount()
		{
			static std::atomic<uint32_t> Count = 0;
			return Count;
		}

		static std::mutex& GetBlocksMutex()
		{
			static std::mutex Mutex;
			return Mutex;
		}

		static std::mutex& GetRecycleMutex()
		{
			static std::mutex Mutex;
			return Mutex;
		}

		static std::shared_mutex& GetAllocationMutex()
		{
			static std::shared_mutex Mutex;
			return Mutex;
		}

		static uint32_t NextSerial()
		{
			static std::atomic<uint32_t> Serial = 0;
			return Serial.fetch_add(1);
		}

		static void RemoveRecycleStackIndexUnsafe(uint32_t RecycleStackIndex)
		{
			// The recycle mutex must be locked during this function's execution

			if (AllocationRecycle* RecycleStack = reinterpret_cast<AllocationRecycle*>(GetRecycleBlock()))
			{
				const uint32_t RecycleStackLength = GetRecycleCount().load();
				if (RecycleStackIndex < RecycleStackLength)
				{
					for (uint32_t NextStackIndex = RecycleStackIndex + 1; NextStackIndex < RecycleStackLength; ++NextStackIndex)
					{
						const uint32_t CurrentStackIndex = NextStackIndex - 1;
						RecycleStack[CurrentStackIndex] = RecycleStack[NextStackIndex];
					}
					RecycleStack[RecycleStackLength - 1].~AllocationRecycle();
					const uint32_t RecycleStackSize = GetRecycleCount().fetch_sub(1);
					LogMemory<EMemoryLogChannel::Tracking>("Recycle stack size is ", RecycleStackSize - 1);
				}
			}
		}

		static size_t ReuseAllocation(uint32_t NumBytes, uint32_t AlignBytes, uint32_t& TrackingIndex)
		{
			std::unique_lock<std::mutex> Lock(GetRecycleMutex());

			std::byte* RecycleBlock = GetRecycleBlock();
			std::byte* TrackingBlock = GetTrackingBlock();
			if (RecycleBlock && TrackingBlock)
			{
				AllocationRecycle* RecycleStack = reinterpret_cast<AllocationRecycle*>(RecycleBlock);
				//AllocationTracking* TrackingList = reinterpret_cast<AllocationTracking*>(TrackingBlock);
				const uint32_t RecycleStackLength = GetRecycleCount().load();
				const uint32_t TrackingCount = GetTrackingCount().load();

				for (uint32_t FromTop = 0; FromTop < RecycleStackLength; ++FromTop)
				{
					const uint32_t RecycleStackIndex = RecycleStackLength - 1 - FromTop;
					AllocationRecycle& RecycleFrame = RecycleStack[RecycleStackIndex];
					const bool bAllocationMatches = RecycleFrame.NumBytes == NumBytes && RecycleFrame.AlignBytes == AlignBytes;
					if (bAllocationMatches && RecycleFrame.TrackingIndex < TrackingCount)
					{
						const size_t StartByte = RecycleFrame.StartByte;
						TrackingIndex = RecycleFrame.TrackingIndex;
						RemoveRecycleStackIndexUnsafe(RecycleStackIndex);
						LogMemory<EMemoryLogChannel::Allocation>("Reusing ", AlignBytes, " byte aligned ", NumBytes, " byte allocation at byte number ", StartByte, " with tracking index ", TrackingIndex);
						return StartByte;
					}
				}
			}

			return InvalidSize();
		}

		static size_t ReserveBytes(uint32_t NumBytes, uint32_t AlignBytes, uint32_t& TrackingIndex)
		{
			size_t StartByte = ReuseAllocation(NumBytes, AlignBytes, TrackingIndex);

			if (StartByte == InvalidSize())
			{
				TrackingIndex = InvalidUint32();

				std::unique_lock<std::mutex> Lock(GetNextByteMutex());

				const size_t Alignment = AlignBytes;
				const size_t NextByte = GetNextByte();
				StartByte = NextByte + ((Alignment - (NextByte & (Alignment - 1))) & (Alignment - 1));
				const size_t LastByte = StartByte + NumBytes - 1;
				const size_t StartBlock = StartByte >> BlockSizeLog2;
				const size_t LastBlock = LastByte >> BlockSizeLog2;
				if (StartBlock != LastBlock)
				{
					// Start at the beginning of the next block
					StartByte = LastBlock << BlockSizeLog2;
				}
				LogMemory<EMemoryLogChannel::Allocation>("Allocating ", NumBytes, " bytes with ", AlignBytes, " byte alignment to byte number ", StartByte);
				GetNextByte() = StartByte + NumBytes;
			}

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
		static constexpr size_t MaxObjects = std::min(TrackingBlockSize / sizeof(AllocationTracking), RecycleBlockSize / sizeof(AllocationRecycle));

		static AllocationTransaction Allocate(uint32_t NumBytes, uint32_t AlignBytes)
		{
			AllocationTransaction Allocation(GetAllocationMutex());

			LogMemory<EMemoryLogChannel::Allocation>("Allocating ", NumBytes, " bytes with ", AlignBytes, " byte alignment");

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

			const size_t StartByte = ReserveBytes(NumBytes, AlignBytes, Allocation.TrackingIndex);
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
					LogMemory<EMemoryLogChannel::Allocation>("Allocating ", BlockSize, " bytes with ", BlockSize, " byte alignment for block ", BlockIndex);
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
					LogMemory<EMemoryLogChannel::Allocation>("Allocating ", BlockSize, " bytes with ", BlockSize, " byte alignment for block ", BlockIndex + 1);
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
			}

			return Allocation;
		}

		template<typename T>
		static IndexSerial TrackAllocation(const AllocationTransaction& Allocation)
		{
			LogMemory<EMemoryLogChannel::Allocation>("Tracking allocation at byte number ", Allocation.StartByte);

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
					LogMemory<EMemoryLogChannel::Allocation>("Allocating ", TrackingBlockSize, " bytes with ", TrackingBlockSize, " byte alignment for tracking block");
					TrackingBlock = reinterpret_cast<std::byte*>(std::aligned_alloc(TrackingBlockSize, TrackingBlockSize));
				}

				GetTrackingBlock() = TrackingBlock;
			}

			if (!TrackingBlock)
			{
				MemoryError("ArenaBumpAllocator::TrackAllocation could not allocate the tracking block");
				return IndexSerial();
			}

			AllocationTracking* Tracking = nullptr;
			uint32_t TrackingIndex = Allocation.TrackingIndex;
			if (TrackingIndex == InvalidUint32())
			{
				TrackingIndex = GetTrackingCount().fetch_add(1);
				LogMemory<EMemoryLogChannel::Tracking>("Tracking list size is ", TrackingIndex + 1);
				if (TrackingIndex >= MaxObjects)
				{
					MemoryError("ArenaBumpAllocator::TrackAllocation is tracking too many objects");
					return IndexSerial();
				}

				LogMemory<EMemoryLogChannel::Allocation>("Adding tracking at index ", TrackingIndex);
				std::byte* TrackingStorage = TrackingBlock + TrackingIndex * sizeof(AllocationTracking);
				Tracking = new(TrackingStorage) AllocationTracking(Allocation.StartByte, Allocation.NumBytes, Allocation.AlignBytes);
			}
			else
			{
				LogMemory<EMemoryLogChannel::Allocation>("Reusing tracking at index ", TrackingIndex);
				std::byte* TrackingStorage = TrackingBlock + TrackingIndex * sizeof(AllocationTracking);
				Tracking = reinterpret_cast<AllocationTracking*>(TrackingStorage);
			}

			Tracking->Serial = NextSerial();
			return {TrackingIndex, Tracking->Serial};
		}

		static void RecycleAllocation(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			LogMemory<EMemoryLogChannel::Allocation>("Recycling allocation with tracking index ", TrackingIndex);

			AllocationTracking* TrackingList = reinterpret_cast<AllocationTracking*>(GetTrackingBlock());
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

			AllocationTracking& Tracking = TrackingList[TrackingIndex];
			if (Tracking.Serial != ObjectSerial)
			{
				MemoryError("ArenaBumpAllocator::RecycleAllocation needs serial numbers to match");
				return;
			}

			LogMemory<EMemoryLogChannel::Allocation>("Recycling ", Tracking.AlignBytes, " byte aligned ", Tracking.NumBytes, " byte allocation with tracking index ", TrackingIndex);

			std::unique_lock<std::mutex> Lock(GetRecycleMutex());

			std::byte* RecycleBlock = GetRecycleBlock();
			if (!RecycleBlock)
			{
				LogMemory<EMemoryLogChannel::Allocation>("Allocating ", RecycleBlockSize, " bytes with ", RecycleBlockSize, " byte alignment for recycle block");
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

			LogMemory<EMemoryLogChannel::Tracking>("Recycle stack size is ", RecycleIndex + 1);
			std::byte* RecycleStorage = RecycleBlock + RecycleIndex * sizeof(AllocationRecycle);
			new(RecycleStorage) AllocationRecycle{Tracking.StartByte, Tracking.NumBytes, Tracking.AlignBytes, TrackingIndex};
		}

		static uint32_t IncrementNumReferences(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			AllocationTracking* TrackingList = reinterpret_cast<AllocationTracking*>(GetTrackingBlock());
			const uint32_t TrackingCount = GetTrackingCount().load();
			if (TrackingList && TrackingIndex < TrackingCount)
			{
				AllocationTracking& Tracking = TrackingList[TrackingIndex];
				if (Tracking.Serial == ObjectSerial)
				{
					return Tracking.NumReferences++;
				}
			}
			return InvalidUint32();
		}

		static uint32_t DecrementNumReferences(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			AllocationTracking* TrackingList = reinterpret_cast<AllocationTracking*>(GetTrackingBlock());
			const uint32_t TrackingCount = GetTrackingCount().load();
			if (TrackingList && TrackingIndex < TrackingCount)
			{
				AllocationTracking& Tracking = TrackingList[TrackingIndex];
				if (Tracking.Serial == ObjectSerial)
				{
					return Tracking.NumReferences--;
				}
			}
			return InvalidUint32();
		}

		static uint32_t GetNumReferences(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			AllocationTracking* TrackingList = reinterpret_cast<AllocationTracking*>(GetTrackingBlock());
			const uint32_t TrackingCount = GetTrackingCount().load();
			if (TrackingList && TrackingIndex < TrackingCount)
			{
				AllocationTracking& Tracking = TrackingList[TrackingIndex];
				if (Tracking.Serial == ObjectSerial)
				{
					return Tracking.NumReferences;
				}
			}
			return InvalidUint32();
		}

		static size_t GetStartByte(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			AllocationTracking* TrackingList = reinterpret_cast<AllocationTracking*>(GetTrackingBlock());
			const uint32_t TrackingCount = GetTrackingCount().load();
			if (TrackingList && TrackingIndex < TrackingCount)
			{
				AllocationTracking& Tracking = TrackingList[TrackingIndex];
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
			if (AllocationTracking* TrackingList = reinterpret_cast<AllocationTracking*>(GetTrackingBlock()))
			{
				const uint32_t TrackingCount = GetTrackingCount().load();
				for (uint32_t TrackingReverseIndex = 0; TrackingReverseIndex < TrackingCount; ++TrackingReverseIndex)
				{
					const uint32_t TrackingIndex = TrackingCount - 1 - TrackingReverseIndex;
					AllocationTracking& Tracking = TrackingList[TrackingIndex];
					Tracking.~AllocationTracking();
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

	template<typename T>
	using Vector = std::vector<T>;

	template<typename T, size_t n>
	using Array = std::array<T, n>;

	class PtrCount
	{
	private:
		static std::atomic<uint64_t>& GetCount()
		{
			static std::atomic<uint64_t> Count;
			return Count;
		}

		static void Increment()
		{
			GetCount().fetch_add(1);
		}

		static void Decrement()
		{
			if (GetCount().fetch_sub(1) == 1)
			{
				ArenaBumpAllocator::TearDown();
			}
		}

		template<typename T>
		friend class SharedPtrImpl;

		template<typename T>
		friend class WeakPtrImpl;

		template<typename T>
		friend class UniquePtrImpl;
	};

	template<typename T>
	class SharedPtrImpl;

	template<typename T>
	class UniquePtrImpl;

	template<typename T>
	struct PtrOps
	{
		template<typename... Ts>
		static SharedPtrImpl<T> MakeShared(Ts&&... Params);

		template<typename... Ts>
		static UniquePtrImpl<T> MakeUnique(Ts&&... Params);

		template<typename FromType>
		static SharedPtrImpl<T> SharedPtrCast(const SharedPtrImpl<FromType>& FromPointer);
	};

	/**
	 * Copyable owning smart pointer that defers deallocation until the end of the program.
	 */
	template<typename T>
	class SharedPtrImpl
	{
	private:
		void DecrementReference()
		{
			const uint32_t DecrementResult = ArenaBumpAllocator::DecrementNumReferences(Index, Serial);
			if (DecrementResult	== 1)
			{
				LogMemory<EMemoryLogChannel::Allocation>("Destructing object at byte number ", ArenaBumpAllocator::GetStartByte(Index, Serial));
				Pointer->~T();
				ArenaBumpAllocator::RecycleAllocation(Index, Serial);
				Pointer = nullptr;
				Index = InvalidUint32();
				Serial = InvalidUint32();
			}
			else if (DecrementResult == InvalidUint32())
			{
				Pointer = nullptr;
				Index = InvalidUint32();
				Serial = InvalidUint32();
			}
		}

		void IncrementReference()
		{
			const uint32_t IncrementResult = ArenaBumpAllocator::IncrementNumReferences(Index, Serial);
			if (IncrementResult == InvalidUint32())
			{
				Pointer = nullptr;
				Index = InvalidUint32();
				Serial = InvalidUint32();
			}
		}

		void Set(T& Reference, uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			if (&Reference != Pointer)
			{
				if (Pointer)
				{
					DecrementReference();
				}
				else
				{
					PtrCount::Increment();
				}
				Pointer = &Reference;
				Index = TrackingIndex;
				Serial = ObjectSerial;
				IncrementReference();
			}
		}

	public:
		SharedPtrImpl(const SharedPtrImpl& Other)
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			PtrCount::Increment();
			IncrementReference();
		}

		SharedPtrImpl(SharedPtrImpl&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			Other.Pointer = nullptr;
			Other.Index = InvalidUint32();
			Other.Serial = InvalidUint32();
		}

		template<typename U>
		SharedPtrImpl(const SharedPtrImpl<U>& Other)
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			PtrCount::Increment();
			IncrementReference();
		}

		template<typename U>
		SharedPtrImpl(SharedPtrImpl<U>&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			Other.Pointer = nullptr;
			Other.Index = InvalidUint32();
			Other.Serial = InvalidUint32();
		}

		SharedPtrImpl(std::nullptr_t) noexcept
			: Pointer(nullptr), Index(InvalidUint32()), Serial(InvalidUint32())
		{
		}

		SharedPtrImpl() noexcept
			: SharedPtrImpl(nullptr)
		{
		}

		~SharedPtrImpl() noexcept
		{
			if (Pointer)
			{
				DecrementReference();
				PtrCount::Decrement();
			}
		}

		SharedPtrImpl& operator=(const SharedPtrImpl& Other)
		{
			if (reinterpret_cast<const std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				const bool bGlobalIncrement = !*this && Other;
				const bool bGlobalDecrement = *this && !Other;

				if (bGlobalIncrement)
				{
					PtrCount::Increment();
				}

				if (Pointer)
				{
					DecrementReference();
				}

				Pointer = Other.Pointer;
				Index = Other.Index;
				Serial = Other.Serial;

				if (Pointer)
				{
					IncrementReference();
				}

				if (bGlobalDecrement)
				{
					PtrCount::Decrement();
				}
			}

			return *this;
		}
		
		SharedPtrImpl& operator=(SharedPtrImpl&& Other) noexcept
		{
			if (reinterpret_cast<std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				const bool bGlobalDecrement = static_cast<bool>(*this);

				if (Pointer)
				{
					DecrementReference();
				}

				Pointer = Other.Pointer;
				Index = Other.Index;
				Serial = Other.Serial;

				Other.Pointer = nullptr;
				Other.Index = InvalidUint32();
				Other.Serial = InvalidUint32();

				if (bGlobalDecrement)
				{
					PtrCount::Decrement();
				}
			}

			return *this;
		}

		template<typename U>
		SharedPtrImpl& operator=(const SharedPtrImpl<U>& Other)
		{
			if (reinterpret_cast<const std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				const bool bGlobalIncrement = !*this && Other;
				const bool bGlobalDecrement = *this && !Other;

				if (bGlobalIncrement)
				{
					PtrCount::Increment();
				}

				if (Pointer)
				{
					DecrementReference();
				}

				Pointer = Other.Pointer;
				Index = Other.Index;
				Serial = Other.Serial;

				if (Pointer)
				{
					IncrementReference();
				}

				if (bGlobalDecrement)
				{
					PtrCount::Decrement();
				}
			}

			return *this;
		}

		template<typename U>
		SharedPtrImpl& operator=(SharedPtrImpl<U>&& Other) noexcept
		{
			if (reinterpret_cast<std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				const bool bGlobalDecrement = static_cast<bool>(*this);

				if (Pointer)
				{
					DecrementReference();
				}

				Pointer = Other.Pointer;
				Index = Other.Index;
				Serial = Other.Serial;

				Other.Pointer = nullptr;
				Other.Index = InvalidUint32();
				Other.Serial = InvalidUint32();

				if (bGlobalDecrement)
				{
					PtrCount::Decrement();
				}
			}

			return *this;
		}

		SharedPtrImpl& operator=(std::nullptr_t) noexcept
		{
			const bool bGlobalDecrement = static_cast<bool>(*this);

			if (Pointer)
			{
				DecrementReference();
			}

			Pointer = nullptr;
			Index = InvalidUint32();
			Serial = InvalidUint32();

			if (bGlobalDecrement)
			{
				PtrCount::Decrement();
			}

			return *this;
		}

		T& operator*() const { return *Pointer; }
		T* operator->() const { return Pointer; }
		T* get() const { return Pointer; }

		explicit operator bool() const { return static_cast<bool>(Pointer); }

		template<typename U>
		friend class SharedPtrImpl;

		template<typename U>
		friend class WeakPtrImpl;

		template<typename U>
		friend struct PtrOps;

	private:
		T* Pointer;
		uint32_t Index;
		uint32_t Serial;
	};

	template<typename T>
	template<typename... Ts>
	inline SharedPtrImpl<T> PtrOps<T>::MakeShared(Ts&&... Params)
	{
		SharedPtrImpl<T> SharedPointer(nullptr);

		{
			AllocationTransaction Allocation = ArenaBumpAllocator::Allocate(sizeof(T), alignof(T));
			if (Allocation.Storage)
			{
				LogMemory<EMemoryLogChannel::Allocation>("Constructing object at byte number ", Allocation.StartByte);
				const IndexSerial Tracking = ArenaBumpAllocator::TrackAllocation<T>(Allocation);
				if (Tracking.Index < ArenaBumpAllocator::MaxObjects)
				{
					T* Object = new(Allocation.Storage) T(std::forward<Ts>(Params)...);
					SharedPointer.Set(*Object, Tracking.Index, Tracking.Serial);
					if (ArenaBumpAllocator::GetNumReferences(Tracking.Index, Tracking.Serial) != 1)
					{
						MemoryError("SharedPtr reference count not initialized correctly");
					}
				}
			}
		}

		return SharedPointer;
	}

	template<typename T, typename... Ts>
	inline SharedPtrImpl<T> MakeSharedImpl(Ts&&... Params)
	{
		return PtrOps<T>::MakeShared(std::forward<Ts>(Params)...);
	}

	template<typename T>
	template<typename FromType>
	inline SharedPtrImpl<T> PtrOps<T>::SharedPtrCast(const SharedPtrImpl<FromType>& FromPointer)
	{
		SharedPtrImpl<T> ToPointer(nullptr);

		if (FromPointer)
		{
			ToPointer.Set(*static_cast<T*>(FromPointer.Pointer), FromPointer.Index, FromPointer.Serial);
		}

		return ToPointer;
	}

	template<typename ToType, typename FromType>
	inline SharedPtrImpl<ToType> SharedPtrCastImpl(const SharedPtrImpl<FromType>& FromPointer)
	{
		return PtrOps<ToType>::SharedPtrCast(FromPointer);
	}

	/**
	 * Copyable non-owning smart pointer that doesn't prevent deallocation.
	 */
	template<typename T>
	class WeakPtrImpl
	{
	public:
		WeakPtrImpl(const WeakPtrImpl& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
		}

		template<typename U>
		WeakPtrImpl(const WeakPtrImpl<U>& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
		}

		WeakPtrImpl(WeakPtrImpl&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			Other.Pointer = nullptr;
			Other.Index = InvalidUint32();
			Other.Serial = InvalidUint32();
		}

		template<typename U>
		WeakPtrImpl(WeakPtrImpl<U>&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			Other.Pointer = nullptr;
			Other.Index = InvalidUint32();
			Other.Serial = InvalidUint32();
		}

		WeakPtrImpl(const SharedPtrImpl<T>& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
		}

		template<typename U>
		WeakPtrImpl(const SharedPtrImpl<U>& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
		}

		WeakPtrImpl(SharedPtrImpl<T>&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
		}

		template<typename U>
		WeakPtrImpl(SharedPtrImpl<U>&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
		}

		WeakPtrImpl(std::nullptr_t) noexcept
			: Pointer(nullptr), Index(InvalidUint32()), Serial(InvalidUint32())
		{
		}

		WeakPtrImpl() noexcept
			: WeakPtrImpl(nullptr)
		{
		}

		~WeakPtrImpl() noexcept
		{
		}

		WeakPtrImpl& operator=(const WeakPtrImpl& Other)
		{
			Pointer = Other.Pointer;
			Index = Other.Index;
			Serial = Other.Serial;
			return *this;
		}

		template<typename U>
		WeakPtrImpl& operator=(const WeakPtrImpl<U>& Other)
		{
			Pointer = Other.Pointer;
			Index = Other.Index;
			Serial = Other.Serial;
			return *this;
		}

		WeakPtrImpl& operator=(WeakPtrImpl&& Other) noexcept
		{
			if (reinterpret_cast<std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				Pointer = Other.Pointer;
				Index = Other.Index;
				Serial = Other.Serial;

				Other.Pointer = nullptr;
				Other.Index = InvalidUint32();
				Other.Serial = InvalidUint32();
			}

			return *this;
		}

		template<typename U>
		WeakPtrImpl& operator=(WeakPtrImpl<U>&& Other) noexcept
		{
			if (reinterpret_cast<std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				Pointer = Other.Pointer;
				Index = Other.Index;
				Serial = Other.Serial;

				Other.Pointer = nullptr;
				Other.Index = InvalidUint32();
				Other.Serial = InvalidUint32();
			}

			return *this;
		}

		WeakPtrImpl& operator=(const SharedPtrImpl<T>& Other)
		{
			Pointer = Other.Pointer;
			Index = Other.Index;
			Serial = Other.Serial;
			return *this;
		}

		template<typename U>
		WeakPtrImpl& operator=(const SharedPtrImpl<U>& Other)
		{
			Pointer = Other.Pointer;
			Index = Other.Index;
			Serial = Other.Serial;
			return *this;
		}

		WeakPtrImpl& operator=(SharedPtrImpl<T>&& Other)
		{
			Pointer = Other.Pointer;
			Index = Other.Index;
			Serial = Other.Serial;
			return *this;
		}

		template<typename U>
		WeakPtrImpl& operator=(SharedPtrImpl<U>&& Other)
		{
			Pointer = Other.Pointer;
			Index = Other.Index;
			Serial = Other.Serial;
			return *this;
		}

		WeakPtrImpl& operator=(std::nullptr_t) noexcept
		{
			Pointer = nullptr;
			Index = InvalidUint32();
			Serial = InvalidUint32();
			return *this;
		}

		SharedPtrImpl<T> lock() const
		{
			SharedPtrImpl<T> SharedPointer(nullptr);

			if (Pointer)
			{
				if (PtrCount::GetCount().fetch_add(1) > 0)
				{
					SharedPointer.Set(*Pointer, Index, Serial);
				}
				PtrCount::GetCount().fetch_sub(1);
			}

			return SharedPointer;
		}

		template<typename U>
		friend class WeakPtrImpl;

	private:
		T* Pointer;
		uint32_t Index;
		uint32_t Serial;
	};

	/**
	 * Uncopyable owning smart pointer that defers deallocation until the end of the program.
	 */
	template<typename T>
	class UniquePtrImpl
	{
	private:
		UniquePtrImpl& operator=(T& Reference)
		{
			if (&Reference != Pointer)
			{
				const bool bGlobalIncrement = !Pointer;
				Pointer = &Reference;
				if (bGlobalIncrement)
				{
					PtrCount::Increment();
				}
			}

			return *this;
		}

	public:
		UniquePtrImpl(const UniquePtrImpl& Other) = delete;
		template<typename U>
		UniquePtrImpl(const UniquePtrImpl<U>& Other) = delete;

		UniquePtrImpl(UniquePtrImpl&& Other) noexcept
			: Pointer(Other.Pointer)
		{
			Other.Pointer = nullptr;
		}

		template<typename U>
		UniquePtrImpl(UniquePtrImpl<U>&& Other) noexcept
			: Pointer(Other.Pointer)
		{
			Other.Pointer = nullptr;
		}

		UniquePtrImpl(std::nullptr_t) noexcept
			: Pointer(nullptr)
		{
		}

		UniquePtrImpl() noexcept
			: UniquePtrImpl(nullptr)
		{
		}

		~UniquePtrImpl() noexcept
		{
			if (Pointer)
			{
				Pointer->~T();
				PtrCount::Decrement();
			}
		}

		UniquePtrImpl& operator=(const UniquePtrImpl& Other) = delete;
		template<typename U>
		UniquePtrImpl& operator=(const UniquePtrImpl<U>& Other) = delete;

		template<typename U>
		UniquePtrImpl& operator=(UniquePtrImpl<U>&& Other)
		{
			if (reinterpret_cast<std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				const bool bGlobalDecrement = static_cast<bool>(*this);

				if (Pointer)
				{
					Pointer->~T();
				}

				Pointer = Other.Pointer;
				Other.Pointer = nullptr;

				if (bGlobalDecrement)
				{
					PtrCount::Decrement();
				}
			}

			return *this;
		}

		UniquePtrImpl& operator=(UniquePtrImpl&& Other)
		{
			if (reinterpret_cast<std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				const bool bGlobalDecrement = static_cast<bool>(*this);

				if (Pointer)
				{
					Pointer->~T();
				}

				Pointer = Other.Pointer;
				Other.Pointer = nullptr;

				if (bGlobalDecrement)
				{
					PtrCount::Decrement();
				}
			}

			return *this;
		}

		UniquePtrImpl& operator=(std::nullptr_t)
		{
			const bool bGlobalDecrement = static_cast<bool>(*this);

			if (Pointer)
			{
				Pointer->~T();
			}

			Pointer = nullptr;

			if (bGlobalDecrement)
			{
				PtrCount::Decrement();
			}

			return *this;
		}

		T& operator*() const { return *Pointer; }
		T* operator->() const { return Pointer; }
		T* get() const { return Pointer; }

		explicit operator bool() const { return static_cast<bool>(Pointer); }

		template<typename U>
		friend class UniquePtrImpl;

		friend struct PtrOps<T>;

	private:
		T* Pointer;
	};

	template<typename T>
	template<typename... Ts>
	inline UniquePtrImpl<T> PtrOps<T>::MakeUnique(Ts&&... Params)
	{
		UniquePtrImpl<T> UniquePointer(nullptr);

		{
			AllocationTransaction Allocation = ArenaBumpAllocator::Allocate(sizeof(T), alignof(T));
			if (Allocation.Storage)
			{
				T* Object = new(Allocation.Storage) T(std::forward<Ts>(Params)...);
				UniquePointer = *Object;
			}
		}

		return UniquePointer;
	}

	template<typename T, typename... Ts>
	inline UniquePtrImpl<T> MakeUniqueImpl(Ts&&... Params)
	{
		return PtrOps<T>::MakeUnique(std::forward<Ts>(Params)...);
	}

	template<typename T>
	using SharedPtr = SharedPtrImpl<T>;
	//template<typename T>
	//using SharedPtr = std::shared_ptr<T>;

	template<typename T, typename... Ts>
	inline SharedPtr<T> MakeShared(Ts&&... Params)
	{
		return MakeSharedImpl<T>(std::forward<Ts>(Params)...);
		//return std::make_shared<T>(std::forward<Ts>(Params)...);
	}

	template<typename ToType, typename FromType>
	inline SharedPtr<ToType> SharedPtrCast(const SharedPtr<FromType>& Ptr)
	{
		return SharedPtrCastImpl<ToType>(Ptr);
		//return std::static_pointer_cast<ToType>(Ptr);
	}

	template<typename T>
	using WeakPtr = WeakPtrImpl<T>;
	//template<typename T>
	//using WeakPtr = std::weak_ptr<T>;

	template<typename T>
	using UniquePtr = UniquePtrImpl<T>;
	//template<typename T>
	//using UniquePtr = std::unique_ptr<T>;

	template<typename T, typename... Ts>
	inline UniquePtr<T> MakeUnique(Ts&&... Params)
	{
		return MakeUniqueImpl<T>(std::forward<Ts>(Params)...);
		//return std::make_unique<T>(std::forward<Ts>(Params)...);
	}

}

template<typename LeftType, typename RightType>
inline bool operator==(const json2wav::SharedPtr<LeftType>& LeftPointer, const json2wav::SharedPtr<RightType>& RightPointer)
{
	return LeftPointer.get() == RightPointer.get();
}

