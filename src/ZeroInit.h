// Copyright Dan Price 2026.

#pragma once

namespace json2wav
{
	template<typename T> class zeroinit_t
	{
	public:
		zeroinit_t() : val(0) {}
		zeroinit_t(const zeroinit_t& other) : val(other.val) {}
		zeroinit_t(zeroinit_t&& other) noexcept : val(static_cast<T&&>(other.val)) {}
		T& operator=(const zeroinit_t& other) { return val = other.val; }
		T& operator=(zeroinit_t&& other) noexcept { return val = static_cast<T&&>(other.val); }
		~zeroinit_t() noexcept {}

		zeroinit_t(const T& valInit) : val(valInit) {}
		zeroinit_t(T&& valInit) noexcept : val(static_cast<T&&>(valInit)) {}
		T& operator=(const T& otherVal) { return val = otherVal; }
		T& operator=(T&& otherVal) noexcept { return val = static_cast<T&&>(otherVal); }

		T& operator*() noexcept { return val; }
		const T& operator*() const noexcept { return val; }
		T* operator->() noexcept { return &val; }
		const T* operator->() const noexcept { return &val; }

	private:
		T val;
	};
}

