// Copyright Dan Price 2026.

#pragma once

#include "Logging.h"
#include <string>
#include <stdexcept>
#include <shared_mutex>
#include <cstdint>
#include <cstddef>

namespace json2wav
{
	inline void MemoryError(const std::string& Message)
	{
		LOG_MEMORY(Error, Message);
		throw std::runtime_error(Message.c_str());
	}

	inline constexpr size_t InvalidSize() { return static_cast<size_t>(-1); }
	inline constexpr uint32_t InvalidUint32() { return static_cast<uint32_t>(-1); }

	struct AllocationTransaction
	{
		explicit AllocationTransaction(std::shared_mutex& Mutex, std::byte* StorageInit = nullptr)
			: Lock(Mutex), Storage(StorageInit), StartByte(InvalidSize()), NumBytes(0), AlignBytes(0), TrackingIndex(InvalidUint32()), ObjectSerial(InvalidUint32())
		{
		}
		std::shared_lock<std::shared_mutex> Lock;
		std::byte* Storage;
		size_t StartByte;
		uint32_t NumBytes;
		uint32_t AlignBytes;
		uint32_t TrackingIndex;
		uint32_t ObjectSerial;
	};
}

