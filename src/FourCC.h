// Copyright Dan Price 2026.

#include <string>
#include <cstdint>

namespace json2wav::riff
{
	union FourCC
	{
	public:
		FourCC() noexcept
			: data(0)
		{
		}

		FourCC(const FourCC& other) noexcept
			: data(other.data)
		{
		}

		FourCC(FourCC&& other) noexcept
			: data(other.data)
		{
		}

		explicit FourCC(const uint32_t dataInit) noexcept
			: data(dataInit)
		{
		}

		explicit FourCC(const char* charsInit) noexcept
			: chars({charsInit[0], charsInit[1], charsInit[2], charsInit[3]})
		{
		}

		FourCC& operator=(const FourCC& other) noexcept
		{
			data = other.data;
			return *this;
		}

		FourCC& operator=(FourCC&& other) noexcept
		{
			data = other.data;
			return *this;
		}

		FourCC& operator=(const uint32_t dataVal) noexcept
		{
			set(dataVal);
			return *this;
		}

		FourCC& operator=(const char* charVals) noexcept
		{
			set(charVals);
			return *this;
		}

		char& operator[](const unsigned int idx) noexcept
		{
			return (&chars.c0)[idx % 4];
		}

		char operator[](const unsigned int idx) const noexcept
		{
			return get(idx);
		}

		void set(const uint32_t dataVal) noexcept
		{
			data = dataVal;
		}

		void set(const char* charVals) noexcept
		{
			for (int i = 0; i < 4; ++i)
			{
				(*this)[i] = charVals[i];
			}
		}

		void set(const unsigned int idx, const char c) noexcept
		{
			(*this)[idx] = c;
		}

		uint32_t get() const noexcept
		{
			return data;
		}

		char get(const unsigned int idx) const noexcept
		{
			return (&chars.c0)[idx % 4];
		}

		explicit operator uint32_t() const noexcept
		{
			return data;
		}

		explicit operator std::string() const noexcept
		{
			std::string str({chars.c0, chars.c1, chars.c2, chars.c3});
			return str;
		}

		friend bool operator==(const FourCC& lhs, const char* rhs) noexcept
		{
			return lhs.chars.c0 == rhs[0] && lhs.chars.c1 == rhs[1] &&
				lhs.chars.c2 == rhs[2] && lhs.chars.c3 == rhs[3];
		}

		friend bool operator==(const char* lhs, const FourCC& rhs) noexcept
		{
			return rhs == lhs;
		}

		friend bool operator==(const FourCC& lhs, const std::string& rhs) noexcept
		{
			return rhs.size() == 4 && lhs == rhs.c_str();
		}

		friend bool operator==(const std::string& lhs, const FourCC& rhs) noexcept
		{
			return rhs == lhs;
		}

		friend bool operator==(const FourCC& lhs, const uint32_t rhs) noexcept
		{
			return lhs.data == rhs;
		}

		friend bool operator==(const uint32_t lhs, const FourCC& rhs) noexcept
		{
			return rhs == lhs;
		}

		friend bool operator==(const FourCC& lhs, const FourCC& rhs) noexcept
		{
			return lhs.data == rhs.data;
		}

		friend bool operator!=(const FourCC& lhs, const char* rhs) noexcept
		{
			return !(lhs == rhs);
		}

		friend bool operator!=(const char* lhs, const FourCC& rhs) noexcept
		{
			return !(rhs == lhs);
		}

		friend bool operator!=(const FourCC& lhs, const std::string& rhs) noexcept
		{
			return !(lhs == rhs);
		}

		friend bool operator!=(const std::string& lhs, const FourCC& rhs) noexcept
		{
			return !(rhs == lhs);
		}

		friend bool operator!=(const FourCC& lhs, const uint32_t rhs) noexcept
		{
			return !(lhs == rhs);
		}

		friend bool operator!=(const uint32_t lhs, const FourCC& rhs) noexcept
		{
			return !(rhs == lhs);
		}

		friend bool operator!=(const FourCC& lhs, const FourCC& rhs) noexcept
		{
			return !(lhs == rhs);
		}

	private:
		uint32_t data;
		struct
		{
			char c0;
			char c1;
			char c2;
			char c3;
		} chars;
	};
}

