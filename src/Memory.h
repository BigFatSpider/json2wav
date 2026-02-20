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

	template<typename... Ts>
	inline void LogMemory(const Ts&... Snippets)
	{
		LogMemoryToFile(VarArgs<Ts...>::ConcatStrings(Snippets...));
	}

	inline void MemoryError(const std::string& Message)
	{
		LogMemory(Message);
		throw std::runtime_error(Message.c_str());
	}

	inline constexpr size_t GetAlignmentIndex(size_t Alignment)
	{
		switch (Alignment)
		{
		case 1: return 0;
		case 2: return 1;
		case 4: return 2;
		case 8: return 3;
		case 16: return 4;
		case 32: return 5;
		default: return 6;
		}
	}

	inline constexpr size_t InvalidSize() { return static_cast<size_t>(-1); }
	inline constexpr uint32_t InvalidUint32() { return static_cast<uint32_t>(-1); }

	template<typename T>
	inline void CallDestructor(void* Object)
	{
		reinterpret_cast<T*>(Object)->~T();
	}

	struct AllocationTracking
	{
		AllocationTracking()
			: StartByte(InvalidSize()), NumBytes(0), AlignBytes(0), NumReferences(0), Serial(InvalidUint32())
		{
		}
		AllocationTracking(const AllocationTracking& Other)
			: StartByte(Other.StartByte), NumBytes(Other.NumBytes), AlignBytes(Other.AlignBytes), NumReferences(Other.NumReferences.load()), Serial(Other.Serial)
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

	struct AllocationTransaction
	{
		explicit AllocationTransaction(std::shared_mutex& Mutex, std::byte* StorageInit = nullptr)
			: Lock(Mutex), Storage(StorageInit), Object(nullptr), TrackingIndex(InvalidUint32())
		{
		}
		std::shared_lock<std::shared_mutex> Lock;
		std::byte* Storage;
		void* Object;
		AllocationTracking Tracking;
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
					GetRecycleCount().fetch_sub(1);
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
				AllocationTracking* TrackingList = reinterpret_cast<AllocationTracking*>(TrackingBlock);
				const uint32_t RecycleStackLength = GetRecycleCount().load();
				const uint32_t TrackingCount = GetTrackingCount().load();

				for (uint32_t FromTop = 0; FromTop < RecycleStackLength; ++FromTop)
				{
					const uint32_t RecycleStackIndex = RecycleStackLength - 1 - FromTop;
					AllocationRecycle& RecycleFrame = RecycleStack[RecycleStackIndex];
					const bool bAllocationMatches = RecycleFrame.NumBytes == NumBytes && RecycleFrame.AlignBytes == AlignBytes;
					if (bAllocationMatches && RecycleFrame.TrackingIndex < TrackingCount)
					{
						TrackingIndex = RecycleFrame.TrackingIndex;
						RemoveRecycleStackIndexUnsafe(RecycleStackIndex);
						LogMemory("Reusing ", AlignBytes, " byte aligned ", NumBytes, " byte allocation at byte number ", TrackingList[TrackingIndex].StartByte, " with tracking index ", TrackingIndex);
						return TrackingList[TrackingIndex].StartByte;
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
				LogMemory("Allocating ", NumBytes, " bytes with ", AlignBytes, " byte alignment to byte number ", StartByte);
				GetNextByte() = StartByte + NumBytes;
			}

			return StartByte;
		}

	public:
		static constexpr size_t BlockSizeLog2 = 24ull;
		static constexpr size_t NumBlocksLog2 = 8ull;
		static constexpr size_t BlockSize = 1ull << BlockSizeLog2;
		static constexpr size_t NumBlocks = 1ull << NumBlocksLog2;
		static constexpr size_t TrackingBlockSize = BlockSize << 2ull;
		static constexpr size_t RecycleBlockSize = TrackingBlockSize >> 1ull;
		static constexpr size_t MaxByte = 1ull << (BlockSizeLog2 + NumBlocksLog2);
		static constexpr size_t MaxObjects = TrackingBlockSize / sizeof(AllocationTracking);

		static AllocationTransaction Allocate(uint32_t NumBytes, uint32_t AlignBytes)
		{
			AllocationTransaction Allocation(GetAllocationMutex());

			LogMemory("Allocating ", NumBytes, " bytes with ", AlignBytes, " byte alignment");

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
					LogMemory("Allocating ", BlockSize, " bytes with ", BlockSize, " byte alignment for block ", BlockIndex);
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
					LogMemory("Allocating ", BlockSize, " bytes with ", BlockSize, " byte alignment for block ", BlockIndex + 1);
					NextBlock = reinterpret_cast<std::byte*>(std::aligned_alloc(BlockSize, BlockSize));
					GetBlocks()[BlockIndex + 1] = NextBlock;
				}
			}

			if (Block)
			{
				Allocation.Storage = Block + ByteIndex;
				Allocation.Tracking.StartByte = StartByte;
				Allocation.Tracking.NumBytes = NumBytes;
				Allocation.Tracking.AlignBytes = AlignBytes;
			}

			return Allocation;
		}

		template<typename T>
		static IndexSerial TrackAllocation(const AllocationTransaction& Allocation)
		{
			LogMemory("Tracking allocation at byte number ", Allocation.Tracking.StartByte);

			if (!Allocation.Object)
			{
				MemoryError("ArenaBumpAllocator::TrackAllocation requires an object to track");
				return IndexSerial();
			}

			std::byte* TrackingBlock = GetTrackingBlock();
			if (!TrackingBlock)
			{
				std::unique_lock<std::mutex> Lock(GetBlocksMutex());

				TrackingBlock = GetTrackingBlock();
				if (!TrackingBlock)
				{
					LogMemory("Allocating ", TrackingBlockSize, " bytes with ", TrackingBlockSize, " byte alignment for tracking block");
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
				if (TrackingIndex >= MaxObjects)
				{
					MemoryError("ArenaBumpAllocator::TrackAllocation is tracking too many objects");
					return IndexSerial();
				}

				LogMemory("Adding tracking at index ", TrackingIndex);
				std::byte* TrackingStorage = TrackingBlock + TrackingIndex * sizeof(AllocationTracking);
				Tracking = new(TrackingStorage) AllocationTracking(Allocation.Tracking);
			}
			else
			{
				LogMemory("Reusing tracking at index ", TrackingIndex);
				std::byte* TrackingStorage = TrackingBlock + TrackingIndex * sizeof(AllocationTracking);
				Tracking = reinterpret_cast<AllocationTracking*>(TrackingStorage);
			}

			Tracking->Serial = NextSerial();
			return {TrackingIndex, Tracking->Serial};
		}

		static void RecycleAllocation(uint32_t TrackingIndex, uint32_t ObjectSerial)
		{
			LogMemory("Recycling allocation with tracking index ", TrackingIndex);

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

			LogMemory("Recycling ", Tracking.AlignBytes, " byte aligned ", Tracking.NumBytes, " byte allocation with tracking index ", TrackingIndex);

			std::unique_lock<std::mutex> Lock(GetRecycleMutex());

			std::byte* RecycleBlock = GetRecycleBlock();
			if (!RecycleBlock)
			{
				LogMemory("Allocating ", RecycleBlockSize, " bytes with ", RecycleBlockSize, " byte alignment for recycle block");
				RecycleBlock = reinterpret_cast<std::byte*>(std::aligned_alloc(RecycleBlockSize, RecycleBlockSize));
				if (!RecycleBlock)
				{
					MemoryError("ArenaBumpAllocator::RecycleAllocation could not allocate the recycle block");
					return;
				}

				GetRecycleBlock() = RecycleBlock;
			}

			const uint32_t RecycleIndex = GetRecycleCount().fetch_add(1);
			std::byte* RecycleStorage = RecycleBlock + RecycleIndex * sizeof(AllocationRecycle);
			new(RecycleStorage) AllocationRecycle{Tracking.NumBytes, Tracking.AlignBytes, TrackingIndex};
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

	//template<typename T>
	//using SharedPtr = std::shared_ptr<T>;

	//template<typename T>
	//using WeakPtr = std::weak_ptr<T>;

	//template<typename T>
	//using UniquePtr = std::unique_ptr<T>;

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
		friend class SharedPtr;

		template<typename T>
		friend class WeakPtr;

		template<typename T>
		friend class UniquePtr;
	};

	template<typename T>
	class SharedPtr;

	template<typename T>
	class UniquePtr;

	template<typename T>
	struct PtrOps
	{
		template<typename... Ts>
		static SharedPtr<T> MakeShared(Ts&&... Params);

		template<typename... Ts>
		static UniquePtr<T> MakeUnique(Ts&&... Params);

		template<typename FromType>
		static SharedPtr<T> SharedPtrCast(const SharedPtr<FromType>& FromPointer);
	};

	/**
	 * Copyable smart pointer that defers deallocation until the end of the program.
	 */
	template<typename T>
	class SharedPtr
	{
	private:
		void DecrementReference()
		{
			const uint32_t DecrementResult = ArenaBumpAllocator::DecrementNumReferences(Index, Serial);
			if (DecrementResult	== 1)
			{
				LogMemory("Destructing object at byte number ", ArenaBumpAllocator::GetStartByte(Index, Serial));
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
		SharedPtr(const SharedPtr& Other)
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			PtrCount::Increment();
			IncrementReference();
		}

		SharedPtr(SharedPtr&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			Other.Pointer = nullptr;
			Other.Index = InvalidUint32();
			Other.Serial = InvalidUint32();
		}

		template<typename U>
		SharedPtr(const SharedPtr<U>& Other)
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			PtrCount::Increment();
			IncrementReference();
		}

		template<typename U>
		SharedPtr(SharedPtr<U>&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			Other.Pointer = nullptr;
			Other.Index = InvalidUint32();
			Other.Serial = InvalidUint32();
		}

		SharedPtr(std::nullptr_t) noexcept
			: Pointer(nullptr), Index(InvalidUint32()), Serial(InvalidUint32())
		{
		}

		SharedPtr() noexcept
			: SharedPtr(nullptr)
		{
		}

		~SharedPtr() noexcept
		{
			if (Pointer)
			{
				DecrementReference();
				PtrCount::Decrement();
			}
		}

		SharedPtr& operator=(const SharedPtr& Other)
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
		
		SharedPtr& operator=(SharedPtr&& Other) noexcept
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
		SharedPtr& operator=(const SharedPtr<U>& Other)
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
		SharedPtr& operator=(SharedPtr<U>&& Other) noexcept
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

		SharedPtr& operator=(std::nullptr_t) noexcept
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
		friend class SharedPtr;

		template<typename U>
		friend class WeakPtr;

		template<typename U>
		friend struct PtrOps;

	private:
		T* Pointer;
		uint32_t Index;
		uint32_t Serial;
	};

	template<typename T>
	template<typename... Ts>
	inline SharedPtr<T> PtrOps<T>::MakeShared(Ts&&... Params)
	{
		SharedPtr<T> SharedPointer(nullptr);

		{
			AllocationTransaction Allocation = ArenaBumpAllocator::Allocate(sizeof(T), alignof(T));
			if (Allocation.Storage)
			{
				LogMemory("Constructing object at byte number ", Allocation.Tracking.StartByte);
				T* Object = new(Allocation.Storage) T(std::forward<Ts>(Params)...);
				Allocation.Object = Object;
				const IndexSerial Tracking = ArenaBumpAllocator::TrackAllocation<T>(Allocation);
				if (Tracking.Index < ArenaBumpAllocator::MaxObjects)
				{
					SharedPointer.Set(*Object, Tracking.Index, Tracking.Serial);
					if (ArenaBumpAllocator::GetNumReferences(Tracking.Index, Tracking.Serial) != 1)
					{
						MemoryError("SharedPtr reference count not initialized correctly");
					}
				}
				else
				{
					Object->~T();
				}
			}
		}

		return SharedPointer;
	}

	template<typename T, typename... Ts>
	inline SharedPtr<T> MakeShared(Ts&&... Params)
	{
		return PtrOps<T>::MakeShared(std::forward<Ts>(Params)...);
	}

	template<typename T>
	template<typename FromType>
	inline SharedPtr<T> PtrOps<T>::SharedPtrCast(const SharedPtr<FromType>& FromPointer)
	{
		SharedPtr<T> ToPointer(nullptr);

		if (FromPointer)
		{
			ToPointer.Set(static_cast<T&>(*FromPointer), FromPointer.Index, FromPointer.Serial);
		}

		return ToPointer;
	}

	template<typename ToType, typename FromType>
	inline SharedPtr<ToType> SharedPtrCast(const SharedPtr<FromType>& FromPointer)
	{
		return PtrOps<ToType>::SharedPtrCast(FromPointer);
	}

	/**
	 * Copyable smart pointer that doesn't prevent deallocation.
	 */
	template<typename T>
	class WeakPtr
	{
	public:
		template<typename U>
		WeakPtr(const WeakPtr<U>& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
		}

		template<typename U>
		WeakPtr(WeakPtr<U>&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
			Other.Pointer = nullptr;
			Other.Index = InvalidUint32();
			Other.Serial = InvalidUint32();
		}

		template<typename U>
		WeakPtr(const SharedPtr<U>& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
		}

		template<typename U>
		WeakPtr(SharedPtr<U>&& Other) noexcept
			: Pointer(Other.Pointer), Index(Other.Index), Serial(Other.Serial)
		{
		}

		WeakPtr(std::nullptr_t) noexcept
			: Pointer(nullptr), Index(InvalidUint32()), Serial(InvalidUint32())
		{
		}

		WeakPtr() noexcept
			: WeakPtr(nullptr)
		{
		}

		~WeakPtr() noexcept
		{
		}

		template<typename U>
		WeakPtr& operator=(const WeakPtr<U>& Other)
		{
			Pointer = Other.Pointer;
			Index = Other.Index;
			Serial = Other.Serial;
			return *this;
		}

		template<typename U>
		WeakPtr& operator=(WeakPtr<U>&& Other) noexcept
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

		WeakPtr& operator=(std::nullptr_t) noexcept
		{
			Pointer = nullptr;
			Index = InvalidUint32();
			Serial = InvalidUint32();
			return *this;
		}

		SharedPtr<T> lock() const
		{
			SharedPtr<T> SharedPointer(nullptr);

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
		friend class WeakPtr;

	private:
		T* Pointer;
		uint32_t Index;
		uint32_t Serial;
	};

	/**
	 * Uncopyable smart pointer that defers deallocation until the end of the program.
	 */
	template<typename T>
	class UniquePtr
	{
	private:
		UniquePtr& operator=(T& Reference)
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
		template<typename U>
		UniquePtr(const UniquePtr<U>& Other) = delete;

		template<typename U>
		UniquePtr(UniquePtr<U>&& Other) noexcept
			: Pointer(Other.Pointer)
		{
			Other.Pointer = nullptr;
		}

		UniquePtr(std::nullptr_t) noexcept
			: Pointer(nullptr)
		{
		}

		UniquePtr() noexcept
			: UniquePtr(nullptr)
		{
		}

		~UniquePtr() noexcept
		{
			if (Pointer)
			{
				Pointer->~T();
				PtrCount::Decrement();
			}
		}

		template<typename U>
		UniquePtr& operator=(const UniquePtr<U>& Other) = delete;

		template<typename U>
		UniquePtr& operator=(UniquePtr<U>&& Other)
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

		template<typename U>
		UniquePtr& operator=(std::nullptr_t)
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
		friend class UniquePtr;

		friend struct PtrOps<T>;

	private:
		T* Pointer;
	};

	template<typename T>
	template<typename... Ts>
	inline UniquePtr<T> PtrOps<T>::MakeUnique(Ts&&... Params)
	{
		UniquePtr<T> UniquePointer(nullptr);

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
	inline UniquePtr<T> MakeUnique(Ts&&... Params)
	{
		return PtrOps<T>::MakeUnique(std::forward<Ts>(Params)...);
	}

	/*template<typename T, typename... Ts>
	inline SharedPtr<T> MakeShared(Ts&&... Params)
	{
		return std::make_shared<T>(std::forward<Ts>(Params)...);
	}

	template<typename T, typename... Ts>
	inline UniquePtr<T> MakeUnique(Ts&&... Params)
	{
		return std::make_unique<T>(std::forward<Ts>(Params)...);
	}

	template<typename ToType, typename FromType>
	inline SharedPtr<ToType> SharedPtrCast(const SharedPtr<FromType>& Ptr)
	{
		return std::static_pointer_cast<ToType>(Ptr);
	}*/
}

template<typename LeftType, typename RightType>
inline bool operator==(const json2wav::SharedPtr<LeftType>& LeftPointer, const json2wav::SharedPtr<RightType>& RightPointer)
{
	return LeftPointer.get() == RightPointer.get();
}

