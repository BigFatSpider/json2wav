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
#include <cstdlib>
#include <cstddef>

namespace json2wav
{
	template<typename T>
	inline void CallDestructor(void* Object)
	{
		reinterpret_cast<T*>(Object)->~T();
	}

	struct DestructorData
	{
		explicit DestructorData(void (*DestructorInit)(void*), void* ObjectInit)
			: Destructor(DestructorInit), Object(ObjectInit)
		{
		}
		void (*Destructor)(void*);
		void* Object;
	};

	struct AllocationTransaction
	{
		explicit AllocationTransaction(std::shared_mutex& Mutex, std::byte* StorageInit = nullptr)
			: Lock(Mutex), Storage(StorageInit), Object(nullptr)
		{
		}
		std::shared_lock<std::shared_mutex> Lock;
		std::byte* Storage;
		void* Object;
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

		static size_t ReserveBytes(size_t NumBytes, size_t AlignBytes)
		{
			size_t StartByte = 0;

			{
				std::unique_lock<std::mutex> Lock(GetNextByteMutex());

				const size_t NextByte = GetNextByte();
				StartByte = NextByte + ((AlignBytes - (NextByte & (AlignBytes - 1))) & (AlignBytes - 1));
				const size_t LastByte = StartByte + NumBytes - 1;
				const size_t StartBlock = StartByte >> BlockSizeLog2;
				const size_t LastBlock = LastByte >> BlockSizeLog2;
				if (StartBlock != LastBlock)
				{
					// Start at the beginning of the next block
					StartByte = LastBlock << BlockSizeLog2;
				}
				GetNextByte() = StartByte + NumBytes;
			}

			return StartByte;
		}

		static std::byte** GetBlocks()
		{
			static std::byte* Blocks[NumBlocks] = { 0 };
			return &Blocks[0];
		}

		static std::byte*& GetDestructorBlock()
		{
			static std::byte* Block = nullptr;
			return Block;
		}

		static std::atomic<size_t>& GetDestructorCount()
		{
			static std::atomic<size_t> DestructorCount = 0;
			return DestructorCount;
		}

		static std::mutex& GetBlocksMutex()
		{
			static std::mutex BlocksMutex;
			return BlocksMutex;
		}

		static std::shared_mutex& GetAllocationMutex()
		{
			static std::shared_mutex AllocationMutex;
			return AllocationMutex;
		}

	public:
		static constexpr size_t BlockSizeLog2 = 20ull;
		static constexpr size_t NumBlocksLog2 = 12ull;
		static constexpr size_t BlockSize = 1ull << BlockSizeLog2;
		static constexpr size_t NumBlocks = 1ull << NumBlocksLog2;
		static constexpr size_t DestructorBlockSize = BlockSize << 4ull;
		static constexpr size_t MaxByte = 1ull << (BlockSizeLog2 + NumBlocksLog2);
		static constexpr size_t MaxObjects = DestructorBlockSize / sizeof(DestructorData);

		static AllocationTransaction Allocate(size_t NumBytes, size_t AlignBytes)
		{
			AllocationTransaction Allocation(GetAllocationMutex());

			if (AlignBytes & (AlignBytes - 1))
			{
				return Allocation;
			}

			if (NumBytes & (AlignBytes - 1))
			{
				return Allocation;
			}

			const size_t StartByte = ReserveBytes(NumBytes, AlignBytes);
			const size_t BlockIndex = StartByte >> BlockSizeLog2;
			const size_t ByteIndex = StartByte & (BlockSize - 1);
			if (BlockIndex >= NumBlocks)
			{
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
					NextBlock = reinterpret_cast<std::byte*>(std::aligned_alloc(BlockSize, BlockSize));
					GetBlocks()[BlockIndex + 1] = NextBlock;
				}
			}

			if (Block)
			{
				Allocation.Storage = Block + ByteIndex;
			}

			return Allocation;
		}

		template<typename T>
		static bool AddDestructor(const AllocationTransaction& Allocation)
		{
			if (!Allocation.Object)
			{
				return false;
			}

			const size_t DestructorIndex = GetDestructorCount().fetch_add(1);
			if (DestructorIndex >= MaxObjects)
			{
				return false;
			}

			std::byte* DestructorBlock = GetDestructorBlock();
			if (!DestructorBlock)
			{
				std::unique_lock<std::mutex> Lock(GetBlocksMutex());

				std::byte*& DestructorBlockRef = GetDestructorBlock();
				if (!DestructorBlockRef)
				{
					DestructorBlockRef = reinterpret_cast<std::byte*>(std::aligned_alloc(DestructorBlockSize, DestructorBlockSize));
					DestructorBlock = DestructorBlockRef;
				}
			}

			if (!DestructorBlock)
			{
				return false;
			}

			std::byte* DestructorStorage = DestructorBlock + DestructorIndex * sizeof(DestructorData);
			new(DestructorStorage) DestructorData(&CallDestructor<T>, Allocation.Object);
			return true;
		}

		static void TearDown()
		{
			std::unique_lock<std::shared_mutex> AllocationLock(GetAllocationMutex());
			std::unique_lock<std::mutex> BlocksLock(GetBlocksMutex());

			// Destroy everything in reverse order
			if (DestructorData* Destructors = reinterpret_cast<DestructorData*>(GetDestructorBlock()))
			{
				const size_t NumDestructors = GetDestructorCount().load();
				for (size_t DestructorReverseIndex = 0; DestructorReverseIndex < NumDestructors; ++DestructorReverseIndex)
				{
					const size_t DestructorIndex = NumDestructors - 1 - DestructorReverseIndex;
					DestructorData& Destructor = Destructors[DestructorIndex];
					Destructor.Destructor(Destructor.Object);
				}

				std::free(Destructors);
				GetDestructorBlock() = nullptr;
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
	 * Smart pointer that defers destruction until the end of the program.
	 */
	template<typename T>
	class SharedPtr
	{
	private:
		SharedPtr& operator=(T& Reference)
		{
			if (&Reference != Pointer)
			{
				const bool bIncrement = !Pointer;
				Pointer = &Reference;
				if (bIncrement)
				{
					PtrCount::Increment();
				}
			}

			return *this;
		}

	public:
		template<typename U>
		SharedPtr(const SharedPtr<U>& Other)
			: Pointer(Other.Pointer)
		{
			PtrCount::Increment();
		}

		template<typename U>
		SharedPtr(SharedPtr<U>&& Other) noexcept
			: Pointer(Other.Pointer)
		{
			Other.Pointer = nullptr;
		}

		SharedPtr(std::nullptr_t) noexcept
			: Pointer(nullptr)
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
				PtrCount::Decrement();
			}
		}

		template<typename U>
		SharedPtr& operator=(const SharedPtr<U>& Other)
		{
			if (reinterpret_cast<const std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				const bool bIncrement = !*this && Other;
				const bool bDecrement = *this && !Other;

				Pointer = Other.Pointer;

				if (bIncrement)
				{
					PtrCount::Increment();
				}
				else if (bDecrement)
				{
					PtrCount::Decrement();
				}
			}

			return *this;
		}

		template<typename U>
		SharedPtr& operator=(SharedPtr<U>&& Other)
		{
			if (reinterpret_cast<std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				const bool bDecrement = static_cast<bool>(*this);

				Pointer = Other.Pointer;
				Other.Pointer = nullptr;

				if (bDecrement)
				{
					PtrCount::Decrement();
				}
			}

			return *this;
		}

		SharedPtr& operator=(std::nullptr_t)
		{
			const bool bDecrement = static_cast<bool>(*this);

			Pointer = nullptr;

			if (bDecrement)
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

		friend struct PtrOps<T>;

	private:
		T* Pointer;
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
				T* Object = new(Allocation.Storage) T(std::forward<Ts>(Params)...);
				Allocation.Object = Object;
				if (ArenaBumpAllocator::AddDestructor<T>(Allocation))
				{
					SharedPointer = *Object;
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
			ToPointer = static_cast<T&>(*FromPointer);
		}

		return ToPointer;
	}

	template<typename ToType, typename FromType>
	inline SharedPtr<ToType> SharedPtrCast(const SharedPtr<FromType>& FromPointer)
	{
		return PtrOps<ToType>::SharedPtrCast(FromPointer);
	}

	/**
	 * Smart pointer that doesn't prevent destruction.
	 */
	template<typename T>
	class WeakPtr
	{
	public:
		template<typename U>
		WeakPtr(const WeakPtr<U>& Other) noexcept
			: Pointer(Other.Pointer)
		{
		}

		template<typename U>
		WeakPtr(WeakPtr<U>&& Other) noexcept
			: Pointer(Other.Pointer)
		{
			Other.Pointer = nullptr;
		}

		template<typename U>
		WeakPtr(const SharedPtr<U>& Other)
			: Pointer(Other.get())
		{
		}

		template<typename U>
		WeakPtr(SharedPtr<U>&& Other) noexcept
			: Pointer(Other.get())
		{
			Other = nullptr;
		}

		WeakPtr(std::nullptr_t) noexcept
			: Pointer(nullptr)
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
			if (reinterpret_cast<const std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				Pointer = Other.Pointer;
			}

			return *this;
		}

		template<typename U>
		WeakPtr& operator=(WeakPtr<U>&& Other)
		{
			if (reinterpret_cast<std::byte*>(&Other) != reinterpret_cast<std::byte*>(this))
			{
				Pointer = Other.Pointer;
				Other.Pointer = nullptr;
			}

			return *this;
		}

		WeakPtr& operator=(std::nullptr_t)
		{
			Pointer = nullptr;
			return *this;
		}

		SharedPtr<T> lock() const
		{
			SharedPtr<T> SharedPointer(nullptr);

			if (Pointer)
			{
				if (PtrCount::GetCount().fetch_add(1) > 0)
				{
					SharedPointer = *Pointer;
				}
				PtrCount::GetCount().fetch_sub(1);
			}

			return SharedPointer;
		}

		template<typename U>
		friend class WeakPtr;

	private:
		T* Pointer;
	};

	/**
	 * Smart pointer that destroys its object on destruction.
	 */
	template<typename T>
	class UniquePtr
	{
	private:
		UniquePtr& operator=(T& Reference)
		{
			if (&Reference != Pointer)
			{
				const bool bIncrement = !Pointer;
				Pointer = &Reference;
				if (bIncrement)
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
				const bool bDecrement = static_cast<bool>(*this);

				if (bDecrement)
				{
					Pointer->~T();
				}

				Pointer = Other.Pointer;
				Other.Pointer = nullptr;

				if (bDecrement)
				{
					PtrCount::Decrement();
				}
			}

			return *this;
		}

		template<typename U>
		UniquePtr& operator=(std::nullptr_t)
		{
			const bool bDecrement = static_cast<bool>(*this);

			if (bDecrement)
			{
				Pointer->~T();
			}

			Pointer = nullptr;

			if (bDecrement)
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

