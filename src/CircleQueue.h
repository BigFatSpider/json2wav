// Copyright Dan Price 2026.

#pragma once

#include <stdexcept>
#include <utility>
#include <new>

namespace json2wav
{
	template<typename T, size_t n>
	class StaticCircleQueue
	{
	public:
		StaticCircleQueue() : data(reinterpret_cast<T*>(raw_data)), start(0), finish(0) {}
		StaticCircleQueue(const StaticCircleQueue& other)
			: data(reinterpret_cast<T*>(raw_data)), start(other.start), finish(other.finish)
		{
			for (size_t i = start; i != finish; i = (i + 1) & mask)
			{
				new(raw_data + i * sizeof(T)) T(other.data[i]);
			}
		}
		StaticCircleQueue(StaticCircleQueue&& other) noexcept
			: data(reinterpret_cast<T*>(raw_data)), start(other.start), finish(other.finish)
		{
			for (size_t i = start; i != finish; i = (i + 1) & mask)
			{
				new(raw_data + i * sizeof(T)) T(std::move(other.data[i]));
			}
		}
		StaticCircleQueue& operator=(const StaticCircleQueue& other)
		{
			unsigned char op[num];
			std::memset(op, 0, num);
			for (size_t i = start; i != finish; i = (i + 1) & mask)
			{
				op[i] = 1;
			}
			for (size_t i = other.start; i != other.finish; i = (i + 1) & mask)
			{
				op[i] |= 2;
			}

			for (size_t i = 0; i < num; ++i)
			{
				switch (op[i])
				{
				default:
				case 0:
					break;
				case 1:
					data[i].~T();
					break;
				case 2:
					new(raw_data + i * sizeof(T)) T(other.data[i]);
					break;
				case 3:
					data[i] = other.data[i];
					break;
				}
			}

			start = other.start;
			finish = other.finish;
		}
		StaticCircleQueue& operator=(StaticCircleQueue&& other) noexcept
		{
			unsigned char op[num];
			std::memset(op, 0, num);
			for (size_t i = start; i != finish; i = (i + 1) & mask)
			{
				op[i] = 1;
			}
			for (size_t i = other.start; i != other.finish; i = (i + 1) & mask)
			{
				op[i] |= 2;
			}

			for (size_t i = 0; i < num; ++i)
			{
				switch (op[i])
				{
				default:
				case 0:
					break;
				case 1:
					data[i].~T();
					break;
				case 2:
					new(raw_data + i * sizeof(T)) T(std::move(other.data[i]));
					break;
				case 3:
					data[i] = std::move(other.data[i]);
					break;
				}
			}

			start = other.start;
			finish = other.finish;
		}
		~StaticCircleQueue() noexcept
		{
			for (size_t i = start; i != finish; i = (i + 1) & mask)
			{
				data[i].~T();
			}
		}

		bool empty() const noexcept
		{
			return start == finish;
		}

		template<typename... ParamTypes>
		void push(ParamTypes&&... params)
		{
			const size_t newFinish = mask & (finish + 1);
			if (newFinish == start)
			{
				throw std::length_error("StaticCircleQueue is full; couldn't push");
			}
			//data[finish] = T(std::forward<ParamTypes>(params)...);
			new(raw_data + finish * sizeof(T)) T(std::forward<ParamTypes>(params)...);
			finish = newFinish;
		}

/*
		void push(const T& item)
		{
			const size_t newFinish = mask & (finish + 1);
			if (newFinish == start)
			{
				throw std::length_error("StaticCircleQueue is full; couldn't push");
			}
			data[finish] = item;
			finish = newFinish;
		}

		void push(T&& item)
		{
			const size_t newFinish = mask & (finish + 1);
			if (newFinish == start)
			{
				throw std::length_error("StaticCircleQueue is full; couldn't push");
			}
			data[finish] = std::move(item);
			finish = newFinish;
		}
*/

		T pop()
		{
			if (empty())
			{
				throw std::logic_error("StaticCircleQueue is empty; couldn't pop");
			}

			T result(std::move(data[start]));
			data[start].~T();
			start = (start + 1) & mask;
			return result;
		}

		void pop_idx()
		{
			if (empty())
			{
				throw std::logic_error("StaticCircleQueue is empty; couldn't pop");
			}

			data[start].~T();
			start = (start + 1) & mask;
		}

		T& peek()
		{
			if (empty())
			{
				throw std::logic_error("StaticCircleQueue is empty; couldn't peek");
			}

			return data[start];
		}

		const T& peek(const size_t idx = 0) const
		{
			if (empty())
			{
				throw std::logic_error("StaticCircleQueue is empty; couldn't peek");
			}

			return data[(start + idx) & mask];
		}

		size_t size() const noexcept
		{
			if (start <= finish)
			{
				return finish - start;
			}
			return finish + num - start;
		}

	private:
		static constexpr const size_t num = 1 << n;
		static constexpr const size_t mask = num - 1;

	private:
		alignas(T) unsigned char raw_data[num * sizeof(T)];
		T* data;
		size_t start;
		size_t finish;
	};
}

