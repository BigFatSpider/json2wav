// Copyright Dan Price 2026.

#pragma once

#include "Memory.h"
#include <mutex>
#include <utility>

namespace json2wav
{
	template<typename T>
	class ThreadSafeStatic
	{
	private:
		static Vector<UniquePtr<T>>& GetStatics()
		{
			static Vector<UniquePtr<T>> statics;
			return statics;
		}

		static Vector<size_t>& GetRecycleStack()
		{
			static Vector<size_t> recycle;
			return recycle;
		}

		static std::mutex& GetMutex()
		{
			static std::mutex mtx;
			return mtx;
		}

		template<typename... Ts>
		static size_t GetIdxNoLock(Ts&&... params)
		{
			auto& recycle(GetRecycleStack());
			if (recycle.size() > 0)
			{
				const size_t idx(recycle.back());
				recycle.pop_back();
				return idx;
			}
			auto& statics(GetStatics());
			const size_t idx(statics.size());
			statics.emplace_back(MakeUnique<T>(std::forward<Ts>(params)...));
			return idx;
		}

		static void RecycleIdx(const size_t idx)
		{
			std::scoped_lock lock(GetMutex());
			auto& recycle(GetRecycleStack());
			recycle.emplace_back(idx);
		}

		template<typename... Ts>
		static T& InitStaticRef(size_t& idx, Ts&&... params)
		{
			std::scoped_lock lock(GetMutex());
			idx = GetIdxNoLock(std::forward<Ts>(params)...);
			return *GetStatics()[idx];
		}

	public:
		template<typename... Ts>
		ThreadSafeStatic(Ts&&... params)
			: idx_(0), staticRef(InitStaticRef(idx_, std::forward<Ts>(params)...))
		{
		}

		~ThreadSafeStatic() noexcept
		{
			RecycleIdx(idx_);
		}

		operator T()
		{
			return staticRef;
		}

		template<typename... Ts>
		auto operator()(Ts&&... params)
		{
			return staticRef(std::forward<Ts>(params)...);
		}

	private:
		size_t idx_;
		T& staticRef;
	};
}

