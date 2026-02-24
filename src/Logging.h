// Copyright Dan Price 2026.

#pragma once

#include <mutex>
#include <string>
#include <fstream>

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
		static std::string ToString(const T& Snippet)
		{
			std::string StdString(Snippet);
			return StdString;
		}
	};
	template<> struct IsArithmetic<true>
	{
		template<typename T>
		static std::string ToString(const T& Snippet)
		{
			return std::to_string(Snippet);
		}
	};

	template<typename T>
	inline std::string ToString(const T& Snippet)
	{
		return IsArithmetic<std::is_arithmetic_v<T>>::ToString(Snippet);
	}

	template<typename T, typename... Ts>
	struct VarArgs
	{
		static std::string ConcatStrings(const T& Arg, const Ts&... Args)
		{
			return ToString(Arg) + VarArgs<Ts...>::ConcatStrings(Args...);
		}
	};

	template<typename T>
	struct VarArgs<T>
	{
		static std::string ConcatStrings(const T& Arg)
		{
			return ToString(Arg);
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
		static void Log(const Ts&... Snippets)
		{
			LogMemoryToFile(VarArgs<Ts...>::ConcatStrings(Snippets...));
		}
	};
	template<> struct LogMemoryIfActive<false>
	{
		template<typename... Ts>
		static void Log(const Ts&... Snippets) {}
	};

	template<EMemoryLogChannel Channel, typename... Ts>
	inline void LogMemory(const Ts&... Snippets)
	{
		LogMemoryIfActive<IsMemoryLogChannelActive<Channel>>::Log(MemoryLogChannelName<Channel>, ": ", Snippets...);
	}
}

#define LOG_MEMORY(Channel, ...) ::json2wav::LogMemory<EMemoryLogChannel::Channel>(__VA_ARGS__)

