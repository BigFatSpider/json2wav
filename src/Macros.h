// Copyright Dan Price 2026.

#pragma once

#define OPEN_PARENTHESIS (
#define CLOSE_PARENTHESIS )

#define DEFINE_STATIC_PROPERTY(Type, Name, ...) \
	static Type& Get##Name() \
	{ \
		static Type Name __VA_OPT__(OPEN_PARENTHESIS) __VA_ARGS__ __VA_OPT__(CLOSE_PARENTHESIS); \
		return Name; \
	}

