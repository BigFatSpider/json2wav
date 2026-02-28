// Copyright Dan Price 2026.

#pragma once

#include "ArenaBumpAllocator.h"
#include "CachedArenaAllocator.h"
#include "MemoryCommon.h"
#include "Logging.h"
#include <vector>
#include <array>
#include <memory>
#include <utility>
#include <atomic>
#include <new>
#include <cstdint>
#include <cstddef>

namespace json2wav
{
	using ArenaAllocator = CachedArenaAllocator;

	struct AllocationRecycleStackInfo
	{
		uint32_t NumBytes = 0;
		uint32_t AlignBytes = 0;
		uint32_t StackSize = 0;
		uint32_t StackMax = 0;
	};

	struct AllocationRecycleNode
	{
		std::byte* Block = nullptr;
		AllocationRecycleNode* Next = nullptr;
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
				ArenaAllocator::TearDown();
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
			const uint32_t DecrementResult = ArenaAllocator::DecrementNumReferences(Index, Serial);
			if (DecrementResult	== 1)
			{
				if constexpr (ArenaAllocator::HasGetStartByte())
				{
					LOG_MEMORY(Allocation, "Destructing object at byte number ", ArenaAllocator::GetStartByte(Index, Serial));
				}
				Pointer->~T();
				ArenaAllocator::RecycleAllocation(static_cast<uint32_t>(sizeof(T)), static_cast<uint32_t>(alignof(T)), Index, Serial);
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
			const uint32_t IncrementResult = ArenaAllocator::IncrementNumReferences(Index, Serial);
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
			AllocationTransaction Allocation = ArenaAllocator::Allocate(static_cast<uint32_t>(sizeof(T)), static_cast<uint32_t>(alignof(T)));
			if (Allocation.Storage && Allocation.TrackingIndex < ArenaAllocator::MaxObjects)
			{
				const uint32_t TrackingIndex = Allocation.TrackingIndex;
				const uint32_t ObjectSerial = Allocation.ObjectSerial;
				LOG_MEMORY(Allocation, "Constructing object at index ", TrackingIndex, " with serial ", ObjectSerial);
				T* Object = new(Allocation.Storage) T(std::forward<Ts>(Params)...);
				SharedPointer.Set(*Object, Allocation.TrackingIndex, Allocation.ObjectSerial);
				if (ArenaAllocator::GetNumReferences(Allocation.TrackingIndex, Allocation.ObjectSerial) != 1)
				{
					MemoryError("SharedPtr reference count not initialized correctly");
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
			AllocationTransaction Allocation = ArenaAllocator::Allocate(static_cast<uint32_t>(sizeof(T)), static_cast<uint32_t>(alignof(T)));
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

