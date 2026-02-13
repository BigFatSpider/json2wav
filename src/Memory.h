// Copyright Dan Price. All rights reserved.

#pragma once

#include <vector>
#include <array>
#include <memory>
#include <utility>

namespace json2wav
{
	template<typename T>
	using Vector = std::vector<T>;

	template<typename T, size_t n>
	using Array = std::array<T, n>;

	template<typename T>
	using SharedPtr = std::shared_ptr<T>;

	template<typename T>
	using WeakPtr = std::weak_ptr<T>;

	template<typename T>
	using UniquePtr = std::unique_ptr<T>;

	template<typename T, typename... Ts>
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
	}
}

