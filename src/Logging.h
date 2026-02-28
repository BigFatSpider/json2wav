// Copyright Dan Price 2026.

#pragma once

#include <mutex>
#include <string>
#include <fstream>
#include <type_traits>
#include <utility>

namespace json2wav
{
	inline void LogMemoryToFile(const std::string& Message)
	{
		static std::mutex MemoryLogMutex;
		std::scoped_lock<std::mutex> Lock(MemoryLogMutex);
		static std::ios::openmode OpenMode = std::ios::out;
		std::ofstream LogFile("json2wav-memory.log", OpenMode);
		OpenMode |= std::ios::app;
		LogFile << Message << '\n';
	}

	template<bool bIsArithmetic> struct IsArithmetic;
	template<> struct IsArithmetic<false>
	{
		template<typename T>
		static std::string ToString(T&& Snippet)
		{
			std::string StdString(std::forward<T>(Snippet));
			return StdString;
		}
	};
	template<> struct IsArithmetic<true>
	{
		template<typename T>
		static std::string ToString(T&& Snippet)
		{
			return std::to_string(std::forward<T>(Snippet));
		}
	};

	template<typename T>
	inline std::string ToString(T&& Snippet)
	{
		return IsArithmetic<std::is_arithmetic_v<std::remove_reference_t<T>>>::ToString(std::forward<T>(Snippet));
	}

	template<typename T, typename... Ts>
	struct VarArgs
	{
		static std::string ConcatStrings(T&& Arg, Ts&&... Args)
		{
			return ToString(std::forward<T>(Arg)) + VarArgs<Ts...>::ConcatStrings(std::forward<Ts>(Args)...);
		}
	};

	template<typename T>
	struct VarArgs<T>
	{
		static std::string ConcatStrings(T&& Arg)
		{
			return ToString(std::forward<T>(Arg));
		}
	};

	enum class EMemoryLogChannel
	{
		Allocation,
		Tracking,
		Error
	};

	template<EMemoryLogChannel Channel> struct MemoryLogChannelProperties;
	template<> struct MemoryLogChannelProperties<EMemoryLogChannel::Allocation>
	{
		static constexpr bool IsActive = false;
		static constexpr const char* Name = "Allocation";
	};
	template<> struct MemoryLogChannelProperties<EMemoryLogChannel::Tracking>
	{
		static constexpr bool IsActive = false;
		static constexpr const char* Name = "Tracking";
	};
	template<> struct MemoryLogChannelProperties<EMemoryLogChannel::Error>
	{
		static constexpr bool IsActive = true;
		static constexpr const char* Name = "Error";
	};
	template<EMemoryLogChannel Channel>
	constexpr bool IsMemoryLogChannelActive = MemoryLogChannelProperties<Channel>::IsActive;
	template<EMemoryLogChannel Channel>
	constexpr const char* MemoryLogChannelName = MemoryLogChannelProperties<Channel>::Name;

	template<bool bChannelActive> struct LogMemoryIfActive;
	template<> struct LogMemoryIfActive<true>
	{
		template<typename... Ts>
		static void Log(Ts&&... Snippets)
		{
			LogMemoryToFile(VarArgs<Ts...>::ConcatStrings(std::forward<Ts>(Snippets)...));
		}
	};
	template<> struct LogMemoryIfActive<false>
	{
		template<typename... Ts>
		static void Log(Ts&&... Snippets) {}
	};

	template<EMemoryLogChannel Channel, typename... Ts>
	inline void LogMemory(Ts&&... Snippets)
	{
		LogMemoryIfActive<IsMemoryLogChannelActive<Channel>>::Log(MemoryLogChannelName<Channel>, ": ", std::forward<Ts>(Snippets)...);
	}
}

#define LOG_MEMORY(Channel, ...) ::json2wav::LogMemory<EMemoryLogChannel::Channel>(__VA_ARGS__)

