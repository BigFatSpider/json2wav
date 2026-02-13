// Copyright Dan Price. All rights reserved.

#pragma once

#include <cstddef>

#define ALBUMBOT_DEBUGNEW

void* operator new(std::size_t sz);
void operator delete(void* ptr) noexcept;

namespace json2wav
{
	double QueryAllocTime();
	double QueryDeallocTime();
	void PrintAllocTimes(const char* const desc);
}

