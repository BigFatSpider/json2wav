// Copyright Dan Price. All rights reserved.

#pragma once

#include <map>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace json2wav
{
	inline std::atomic<uint64_t>& GetMapTime()
	{
		static std::atomic<uint64_t> timens = 0;
		return timens;
	}

	inline double QueryMapTime()
	{
		const uint64_t timens = GetMapTime().exchange(0);
		return static_cast<double>(timens)*0.000000001;
	}

	struct ScopedMapTimer
	{
		ScopedMapTimer() : start(std::chrono::high_resolution_clock::now()) {}
		~ScopedMapTimer() noexcept
		{
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::nanoseconds diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
			GetMapTime() += diff.count();
		}
		const std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> start;
	};

	template<typename K, typename V>
	class stdmapwrapper
	{
	public:
		template<typename... ParamTypes>
		decltype(auto) find(ParamTypes&&... params)
		{
			ScopedMapTimer timer;
			return wrapped.find(std::forward<ParamTypes>(params)...);
		}

		template<typename... ParamTypes>
		decltype(auto) find(ParamTypes&&... params) const
		{
			ScopedMapTimer timer;
			return wrapped.find(std::forward<ParamTypes>(params)...);
		}

		decltype(auto) begin()
		{
			ScopedMapTimer timer;
			return wrapped.begin();
		}

		decltype(auto) begin() const
		{
			ScopedMapTimer timer;
			return wrapped.begin();
		}

		decltype(auto) end()
		{
			ScopedMapTimer timer;
			return wrapped.end();
		}

		decltype(auto) end() const
		{
			ScopedMapTimer timer;
			return wrapped.end();
		}

		template<typename... ParamTypes>
		decltype(auto) emplace(ParamTypes&&... params)
		{
			ScopedMapTimer timer;
			return wrapped.emplace(std::forward<ParamTypes>(params)...);
		}

		void clear() noexcept
		{
			ScopedMapTimer timer;
			wrapped.clear();
		}

		template<typename... ParamTypes>
		decltype(auto) lower_bound(ParamTypes&&... params)
		{
			ScopedMapTimer timer;
			return wrapped.lower_bound(std::forward<ParamTypes>(params)...);
		}

		template<typename... ParamTypes>
		decltype(auto) lower_bound(ParamTypes&&... params) const
		{
			ScopedMapTimer timer;
			return wrapped.lower_bound(std::forward<ParamTypes>(params)...);
		}

		template<typename... ParamTypes>
		decltype(auto) upper_bound(ParamTypes&&... params)
		{
			ScopedMapTimer timer;
			return wrapped.upper_bound(std::forward<ParamTypes>(params)...);
		}

		template<typename... ParamTypes>
		decltype(auto) upper_bound(ParamTypes&&... params) const
		{
			ScopedMapTimer timer;
			return wrapped.upper_bound(std::forward<ParamTypes>(params)...);
		}

		template<typename... ParamTypes>
		decltype(auto) equal_range(ParamTypes&&... params)
		{
			ScopedMapTimer timer;
			return wrapped.equal_range(std::forward<ParamTypes>(params)...);
		}

		template<typename... ParamTypes>
		decltype(auto) equal_range(ParamTypes&&... params) const
		{
			ScopedMapTimer timer;
			return wrapped.equal_range(std::forward<ParamTypes>(params)...);
		}

	private:
		std::map<K, V> wrapped;
	};
}

