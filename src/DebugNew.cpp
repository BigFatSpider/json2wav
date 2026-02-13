// Copyright Dan Price. All rights reserved.

#include "DebugNew.h"
#include <chrono>
#include <atomic>
#include <iostream>
#include <cstdlib>
#include <cstdint>

namespace
{
	std::atomic<uint64_t>& GetAllocTime()
	{
		static std::atomic<uint64_t> timens = 0.0f;
		return timens;
	}

	std::atomic<uint64_t>& GetDeallocTime()
	{
		static std::atomic<uint64_t> timens = 0.0f;
		return timens;
	}
}

void* operator new(std::size_t sz)
{
	auto start = std::chrono::high_resolution_clock::now();
	void* ptr = std::malloc(sz);
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::nanoseconds diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
	GetAllocTime() += diff.count();
	return ptr;
}

void operator delete(void* ptr) noexcept
{
	auto start = std::chrono::high_resolution_clock::now();
	std::free(ptr);
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::nanoseconds diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
	GetDeallocTime() += diff.count();
}

namespace json2wav
{
	double QueryAllocTime()
	{
		const uint64_t timens = GetAllocTime().exchange(0);
		return static_cast<double>(timens)*0.000000001;
	}

	double QueryDeallocTime()
	{
		const uint64_t timens = GetDeallocTime().exchange(0);
		return static_cast<double>(timens)*0.000000001;
	}

	void PrintAllocTimes(const char* const desc)
	{
		const double allocTime = QueryAllocTime();
		const double deallocTime = QueryDeallocTime();
		std::cout << allocTime << " seconds spent allocating and\n";
		std::cout << deallocTime << " seconds spent deallocating " << desc << "\nsince previous query.\n";
	}
}

