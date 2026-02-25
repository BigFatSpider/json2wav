// Copyright Dan Price 2026.

#pragma once

#include "Logging.h"
#include <string>
#include <stdexcept>
#include <cstdint>

namespace json2wav
{
	inline void MemoryError(const std::string& Message)
	{
		LOG_MEMORY(Error, Message);
		throw std::runtime_error(Message.c_str());
	}

	inline constexpr size_t InvalidSize() { return static_cast<size_t>(-1); }
	inline constexpr uint32_t InvalidUint32() { return static_cast<uint32_t>(-1); }
}

