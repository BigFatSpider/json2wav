// Copyright Dan Price 2026.

#pragma once

#include <cmath>
#include <cstdint>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifndef ENABLE_QSIN_LOGGING
#define ENABLE_QSIN_LOGGING 0
#endif

#if defined(ENABLE_QSIN_LOGGING) && ENABLE_QSIN_LOGGING
#include <iostream>
#define QSINLOG(msg) std::cout << "qsin log: " << msg << '\n'
#else
#define QSINLOG(msg)
#endif

#define ALBUMBOT_FAST_TRIG_SIMD 0

namespace json2wav
{
	template<typename FloatType> inline constexpr FloatType Tau() noexcept
	{
		return static_cast<FloatType>(2.0l * 3.14159265358979323846l);
	}

	template<typename FloatType> inline constexpr FloatType QuarterTau() noexcept
	{
		return Tau<FloatType>() * 0.25;
	}

	template<typename FloatType> inline constexpr FloatType HalfTau() noexcept
	{
		return Tau<FloatType>() * 0.5;
	}

	template<typename FloatType> inline constexpr FloatType ThreeQuarterTau() noexcept
	{
		return HalfTau<FloatType>() + QuarterTau<FloatType>();
	}

	template<typename FloatType> inline constexpr FloatType TauInv() noexcept
	{
		return static_cast<FloatType>(1.0) / Tau<FloatType>();
	}

	template<typename FloatType> inline constexpr FloatType HalfTauInv() noexcept
	{
		return static_cast<FloatType>(1.0) / HalfTau<FloatType>();
	}

	template<typename FloatType> struct vTau
	{
		static constexpr const FloatType value = Tau<FloatType>();
	};

	template<typename FloatType> struct vQuarterTau
	{
		static constexpr const FloatType value = QuarterTau<FloatType>();
	};

	template<typename FloatType> struct vHalfTau
	{
		static constexpr const FloatType value = HalfTau<FloatType>();
	};

	template<typename FloatType> struct vThreeQuarterTau
	{
		static constexpr const FloatType value = ThreeQuarterTau<FloatType>();
	};

	template<typename FloatType> struct vTauInv
	{
		static constexpr const FloatType value = TauInv<FloatType>();
	};

	template<typename FloatType> struct vHalfTauInv
	{
		static constexpr const FloatType value = HalfTauInv<FloatType>();
	};

	template<typename Type> struct RemoveReference { typedef Type type; };
	template<typename Type> struct RemoveReference<Type&> { typedef Type type; };
	template<typename Type> struct RemoveReference<Type&&> { typedef Type type; };
	template<typename Type> using RemoveReference_t = typename RemoveReference<Type>::type;

	template<typename Type> struct RemoveCV { typedef Type type; };
	template<typename Type> struct RemoveCV<const Type> { typedef Type type; };
	template<typename Type> struct RemoveCV<volatile Type> { typedef Type type; };
	template<typename Type> struct RemoveCV<const volatile Type> { typedef Type type; };
	template<typename Type> using RemoveCV_t = typename RemoveCV<Type>::type;

	namespace detail
	{
		template<typename FloatType> struct MatchUIntInternal { typedef char type; };
		template<> struct MatchUIntInternal<float> { typedef uint32_t type; };
		template<> struct MatchUIntInternal<double> { typedef uint64_t type; };
		template<typename FloatType> struct MatchIntInternal { typedef char type; };
		template<> struct MatchIntInternal<float> { typedef int32_t type; };
		template<> struct MatchIntInternal<double> { typedef int64_t type; };
	}

	template<typename FloatType> struct MatchUInt
	{
		typedef typename detail::MatchUIntInternal<RemoveCV_t<RemoveReference_t<FloatType>>>::type type;
	};
	template<typename FloatType> using MatchUInt_t = typename MatchUInt<FloatType>::type;

	template<typename FloatType> struct MatchInt
	{
		typedef typename detail::MatchIntInternal<RemoveCV_t<RemoveReference_t<FloatType>>>::type type;
	};
	template<typename FloatType> using MatchInt_t = typename MatchInt<FloatType>::type;

	namespace detail
	{
		template<typename Type> struct SignBitInternal;
		template<> struct SignBitInternal<float>
		{
			static constexpr const float fvalue = -0.0f;
			static constexpr const MatchUInt_t<float> value = 0x80000000;
			static constexpr const MatchUInt_t<float> allbits = 0xffffffff;
		};
		template<> struct SignBitInternal<double>
		{
			static constexpr const double fvalue = -0.0;
			static constexpr const MatchUInt_t<double> value = 0x8000000000000000;
			static constexpr const MatchUInt_t<double> allbits = 0xffffffffffffffff;
		};
	}

	template<typename FloatType> struct SignBit
	{
		static constexpr const MatchUInt_t<FloatType> value = detail::SignBitInternal<RemoveCV_t<RemoveReference_t<FloatType>>>::value;
		static constexpr const FloatType fvalue = detail::SignBitInternal<RemoveCV_t<RemoveReference_t<FloatType>>>::fvalue;
	};

	namespace detail
	{
		template<typename FloatType> struct IsFloatTypeInternal { static constexpr const bool value = false; };
		template<> struct IsFloatTypeInternal<float> { static constexpr const bool value = true; };
		template<> struct IsFloatTypeInternal<double> { static constexpr const bool value = true; };
		template<> struct IsFloatTypeInternal<long double> { static constexpr const bool value = true; };
	}

	template<typename FloatType> struct IsFloatType
	{
		static constexpr const bool value = detail::IsFloatTypeInternal<RemoveCV_t<RemoveReference_t<FloatType>>>::value;
	};

	namespace detail
	{
		template<size_t numBytes> inline void CopyBytes(const unsigned char* from, unsigned char* to);
		template<> inline void CopyBytes<4>(const unsigned char* from, unsigned char* to)
		{
			*to = *from;
			*(to + 1) = *(from + 1);
			*(to + 2) = *(from + 2);
			*(to + 3) = *(from + 3);
		}
		template<> inline void CopyBytes<8>(const unsigned char* from, unsigned char* to)
		{
			*to = *from;
			*(to + 1) = *(from + 1);
			*(to + 2) = *(from + 2);
			*(to + 3) = *(from + 3);
			*(to + 4) = *(from + 4);
			*(to + 5) = *(from + 5);
			*(to + 6) = *(from + 6);
			*(to + 7) = *(from + 7);
		}
	}

	// Cast from one type to another while preserving bits, aka type punning
	template<typename ToType, typename FromType> inline ToType BitCast(const FromType from)
	{
		static_assert(sizeof(FromType) == sizeof(ToType), "Cannot BitCast between types of unequal size");

		ToType to;
		const unsigned char* frombyte = reinterpret_cast<const unsigned char*>(&from);
		unsigned char* tobyte = reinterpret_cast<unsigned char*>(&to);
		detail::CopyBytes<sizeof(ToType)>(frombyte, tobyte);

		return to;
	}

	template<typename FloatType> inline MatchUInt_t<FloatType> FloatToBits(const FloatType x)
	{
		return BitCast<MatchUInt_t<FloatType>>(x);
	}

	template<typename FloatType> inline FloatType BitsToFloat(const MatchUInt_t<FloatType> x)
	{
		return BitCast<FloatType>(x);
	}

	namespace detail
	{
		template<typename FloatType> struct Float32Internal { typedef char type; };
		template<> struct Float32Internal<float> { typedef float type; };
		template<> struct Float32Internal<double> { typedef float type; };
		template<> struct Float32Internal<const float> { typedef const float type; };
		template<> struct Float32Internal<const double> { typedef const float type; };
		template<> struct Float32Internal<volatile float> { typedef volatile float type; };
		template<> struct Float32Internal<volatile double> { typedef volatile float type; };
		template<> struct Float32Internal<const volatile float> { typedef const volatile float type; };
		template<> struct Float32Internal<const volatile double> { typedef const volatile float type; };

		template<typename FloatType> struct Float64Internal { typedef char type; };
		template<> struct Float64Internal<float> { typedef double type; };
		template<> struct Float64Internal<double> { typedef double type; };
		template<> struct Float64Internal<const float> { typedef const double type; };
		template<> struct Float64Internal<const double> { typedef const double type; };
		template<> struct Float64Internal<volatile float> { typedef volatile double type; };
		template<> struct Float64Internal<volatile double> { typedef volatile double type; };
		template<> struct Float64Internal<const volatile float> { typedef const volatile double type; };
		template<> struct Float64Internal<const volatile double> { typedef const volatile double type; };
	}

	template<typename FloatType> struct Float32
	{
		typedef typename detail::Float32Internal<FloatType>::type type;
	};
	template<typename FloatType> struct Float32<FloatType&>
	{
		typedef typename detail::Float32Internal<RemoveReference_t<FloatType>>::type& type;
	};
	template<typename FloatType> struct Float32<FloatType&&>
	{
		typedef typename detail::Float32Internal<RemoveReference_t<FloatType>>::type&& type;
	};
	template<typename FloatType> using Float32_t = typename Float32<FloatType>::type;

	template<typename FloatType> struct Float64
	{
		typedef typename detail::Float64Internal<FloatType>::type type;
	};
	template<typename FloatType> struct Float64<FloatType&>
	{
		typedef typename detail::Float64Internal<RemoveReference_t<FloatType>>::type& type;
	};
	template<typename FloatType> struct Float64<FloatType&&>
	{
		typedef typename detail::Float64Internal<RemoveReference_t<FloatType>>::type&& type;
	};
	template<typename FloatType> using Float64_t = typename Float64<FloatType>::type;

	template<typename FloatType> inline Float32_t<FloatType> To32Bit(const FloatType x) noexcept
	{
		return static_cast<Float32_t<FloatType>>(x);
	}

	template<typename FloatType> inline Float64_t<FloatType> To64Bit(const FloatType x) noexcept
	{
		return static_cast<Float64_t<FloatType>>(x);
	}

	template<uint64_t inputval> struct BitNum
	{
		static_assert((inputval & (inputval - 1)) == 0, "json2wav::BitNum only takes powers of two");

		static constexpr const uint64_t value =
			((0xaaaaaaaaaaaaaaaaull & inputval) ? 0x01ull : 0ull) | // 0b10101010...
			((0xccccccccccccccccull & inputval) ? 0x02ull : 0ull) | // 0b11001100...
			((0xf0f0f0f0f0f0f0f0ull & inputval) ? 0x04ull : 0ull) | // 0b11110000...
			((0xff00ff00ff00ff00ull & inputval) ? 0x08ull : 0ull) | // 0b1111111100000000...
			((0xffff0000ffff0000ull & inputval) ? 0x10ull : 0ull) | // etc.
			((0xffffffff00000000ull & inputval) ? 0x20ull : 0ull);
	};

	namespace detail
	{
		template<typename FloatType> inline constexpr MatchUInt_t<FloatType> GetFloatExpMask();
		template<> inline constexpr MatchUInt_t<float> GetFloatExpMask<float>() { return 0x7f800000; }
		template<> inline constexpr MatchUInt_t<double> GetFloatExpMask<double>() { return 0x7ff0000000000000; }

		template<uint64_t inputval, uint64_t n = 0> struct PropLMBRight // Propagate LMB to the right
		{
			static constexpr const uint64_t value = PropLMBRight<inputval | (inputval >> (2 << n)), n + 1>::value;
		};
		template<uint64_t inputval> struct PropLMBRight<inputval, 6>
		{
			static constexpr const uint64_t value = inputval;
		};

		template<uint64_t inputval> struct LMB // Left-Most Bit
		{
			static constexpr const uint64_t value = (PropLMBRight<inputval>::value + 1) >> 1;
		};

		template<uint64_t inputval> struct LogFloor
		{
			static constexpr const uint64_t value = BitNum<LMB<inputval>::value>::value;
		};

		template<typename T> struct MSB_num
		{
			static constexpr const int value = 8 * sizeof(T) - 1;
		};
	}

	template<typename FloatType>
	inline FloatType floor(const FloatType x) noexcept
	{
		return std::floor(x);
	}

	template<typename FloatType>
	inline FloatType fmod(const FloatType x) noexcept
	{
		return x - floor(x);
	}

	template<typename FloatType>
	inline FloatType fabs(const FloatType x) noexcept
	{
		return BitsToFloat<FloatType>(FloatToBits(x) & ~SignBit<FloatType>::value);
	}

	namespace detail
	{
		template<typename FloatType, int NumMultiplies> inline FloatType FastQSinInternal(const FloatType x) noexcept;
		template<typename FloatType, int NumMultiplies> inline FloatType FastQCosInternal(const FloatType x) noexcept;
#ifdef __AVX2__
		template<int NumMultiplies> inline __m128 FastQCosInternal_v4f32(__m128 x) noexcept;
#endif

		template<> inline double FastQSinInternal<double, 3>(const double x) noexcept
		{
			// Scaled to second derivative
			static constexpr const double a = -0.13887990377380047;
			static constexpr const double b = 0.9792921997447733;

			const double x2 = x*x;
			return x*(a*x2 + b); // Degree 3 with 2 coefficients and 3 multiplies
		}

		template<> inline float FastQSinInternal<float, 3>(const float x) noexcept
		{
			// Scaled to second derivative
			static constexpr const float a = -0.13887990377380047;
			static constexpr const float b = 0.9792921997447733;

			const float x2 = x*x;
			return x*(a*x2 + b); // Degree 3 with 2 coefficients and 3 multiplies
		}
		
		template<> inline double FastQCosInternal<double, 3>(const double x) noexcept
		{
			// Minmax
			static constexpr const double a = 0.03679173081657126;
			static constexpr const double b = -0.4955810032988461;
			static constexpr const double c = 0.999403273394012;

			const double x2 = x*x;
			return (a*x2 + b)*x2 + c;
		}
		
		template<> inline float FastQCosInternal<float, 3>(const float x) noexcept
		{
			// Minmax
			static constexpr const float a = 0.03679173081657126;
			static constexpr const float b = -0.4955810032988461;
			static constexpr const float c = 0.999403273394012;

			const float x2 = x*x;
			return (a*x2 + b)*x2 + c;
		}

		template<> inline double FastQSinInternal<double, 4>(const double x) noexcept
		{
			// Scaled to second derivative
			static constexpr const double a = 0.007444224939077393;
			static constexpr const double b = -0.16544111784093282;
			static constexpr const double c = 0.9995084195105735;

			const double x2 = x*x;
			return x*(x2*(a*x2 + b) + c); // Degree 5 with 3 coefficients and 4 multiplies
		}

		template<> inline float FastQSinInternal<float, 4>(const float x) noexcept
		{
			// Scaled to second derivative
			static constexpr const float a = 0.007444224939077393;
			static constexpr const float b = -0.16544111784093282;
			static constexpr const float c = 0.9995084195105735;

			const float x2 = x*x;
			return x*(x2*(a*x2 + b) + c); // Degree 5 with 3 coefficients and 4 multiplies
		}

		template<> inline double FastQCosInternal<double, 4>(const double x) noexcept
		{
			// Minmax
			static constexpr const double a = -0.001285748702521207;
			static constexpr const double b = 0.04154325789396029;
			static constexpr const double c = -0.49996088903013497;
			static constexpr const double d = 0.9999986665780344;

			const double x2 = x*x;
			return ((a*x2 + b)*x2 + c)*x2 + d;
		}

		template<> inline float FastQCosInternal<float, 4>(const float x) noexcept
		{
			// Minmax
			static constexpr const float a = -0.001285748702521207;
			static constexpr const float b = 0.04154325789396029;
			static constexpr const float c = -0.49996088903013497;
			static constexpr const float d = 0.9999986665780344;

			const float x2 = x*x;
			return ((a*x2 + b)*x2 + c)*x2 + d;
		}

		template<> inline double FastQSinInternal<double, 5>(const double x) noexcept
		{
			// Scaled to second derivative... or not?
			//static constexpr const double a = -0.00018291002245065702;
			//static constexpr const double b = 0.008302616718014468;
			//static constexpr const double c = -0.16664217516238153;
			//static constexpr const double d = 0.9999935825924967;

			// Scaled to second derivative
			static constexpr const double a = -0.00018603054211531987;
			static constexpr const double b = 0.008316106083806889;
			static constexpr const double c = -0.1666587129389504;
			static constexpr const double d = 0.9999991392712565;

			const double x2 = x*x;
			return x*(x2*(x2*(a*x2 + b) + c) + d); // Degree 7 with 4 coefficients and 5 multiplies
		}

		template<> inline float FastQSinInternal<float, 5>(const float x) noexcept
		{
			// Scaled to second derivative... or not?
			//static constexpr const float a = -0.00018291002245065702;
			//static constexpr const float b = 0.008302616718014468;
			//static constexpr const float c = -0.16664217516238153;
			//static constexpr const float d = 0.9999935825924967;

			// Scaled to second derivative
			static constexpr const float a = -0.00018603054211531987;
			static constexpr const float b = 0.008316106083806889;
			static constexpr const float c = -0.1666587129389504;
			static constexpr const float d = 0.9999991392712565;

			const float x2 = x*x;
			return x*(x2*(x2*(a*x2 + b) + c) + d); // Degree 7 with 4 coefficients and 5 multiplies
		}

		template<> inline double FastQCosInternal<double, 5>(const double x) noexcept
		{
			// Minmax
			static constexpr const double a = 0.0000239135878299366;
			static constexpr const double b = -0.001388047510451876;
			static constexpr const double c = 0.04166641365506613;
			static constexpr const double d = -0.49999998164605974;
			static constexpr const double e = 0.9999999998917916;

			const double x2 = x*x;
			return (((a*x2 + b)*x2 + c)*x2 + d)*x2 + e;
		}

		template<> inline float FastQCosInternal<float, 5>(const float x) noexcept
		{
			// Minmax
			static constexpr const float a = 0.0000239135878299366;
			static constexpr const float b = -0.001388047510451876;
			static constexpr const float c = 0.04166641365506613;
			static constexpr const float d = -0.49999998164605974;
			static constexpr const float e = 0.9999999998917916;

			const float x2 = x*x;
			return (((a*x2 + b)*x2 + c)*x2 + d)*x2 + e;
		}

#ifdef __AVX2__
		template<> inline __m128 FastQCosInternal_v4f32<5>(__m128 x) noexcept
		{
			// Minmax
			static constexpr const float a = 0.0000239135878299366;
			static constexpr const float b = -0.001388047510451876;
			static constexpr const float c = 0.04166641365506613;
			static constexpr const float d = -0.49999998164605974;
			static constexpr const float e = 0.9999999998917916;

			__m128 x2 = _mm_mul_ps(x, x);
			x = _mm_fmadd_ps(_mm_set1_ps(a), x2, _mm_set1_ps(b));
			x = _mm_fmadd_ps(x, x2, _mm_set1_ps(c));
			x = _mm_fmadd_ps(x, x2, _mm_set1_ps(d));
			return _mm_fmadd_ps(x, x2, _mm_set1_ps(e));
		}
#endif

		template<> inline double FastQSinInternal<double, 6>(const double x) noexcept
		{
			// Scaled to second derivative... or not?
			//static constexpr const double a = 2.5757524296267892e-06;
			//static constexpr const double b = -0.00019792713205486696;
			//static constexpr const double c = 0.00833274827499629;
			//static constexpr const double d = -0.1666663723913334;
			//static constexpr const double e = 0.9999999573287034;

			// Scaled to second derivative
			static constexpr const double a = 2.586151293336854e-06;
			static constexpr const double b = -0.0001979765656985496;
			static constexpr const double c = 0.008332814670389913;
			static constexpr const double d = -0.16666638726299862;
			static constexpr const double e = 0.9999999469515957;

#if defined(ALBUMBOT_FAST_TRIG_SIMD) && ALBUMBOT_FAST_TRIG_SIMD
			// Parallel algorithm
			// a*x9 + b*x7 + c*x5 + d*x3 + e*x
			//
			// 1. x*x, a*x, b*x, c*x
			// 2. x2*x2, bx*x2, d*x2
			// 3. bx3+cx, dx2+e
			// 4. ax*x4, bx3scx*x4, dx2se*x
			// 5. ax5*x4
			// 6. ax9+bx7scx5
			// 7. ax9sbx7scx5+dx3sex
			//
			const double x2 = x*x;
			const double ax = a*x;
			const double bx = b*x;
			const double cx = c*x;
			const double x4 = x2*x2;
			const double bx3 = bx*x2;
			const double dx2 = d*x2;
			const double bx3scx = bx3+cx;
			const double dx2se = dx2+e;
			const double ax5 = ax*x4;
			const double bx7scx5 = bx3scx*x4;
			const double dx3sex = dx2se*x;
			const double ax9 = ax5*x4;
			const double ax9sbx7scx5 = ax9+bx7scx5;
			return ax9sbx7scx5+dx3sex;
#else
			// Serial algorithm
			const double x2 = x*x;
			return x*(x2*(x2*(x2*(a*x2 + b) + c) + d) + e); // Degree 9 with 5 coefficients and 6 multiplies
#endif
		}

		template<> inline float FastQSinInternal<float, 6>(const float x) noexcept
		{
			// Scaled to second derivative... or not?
			//static constexpr const float a = 2.5757524296267892e-06;
			//static constexpr const float b = -0.00019792713205486696;
			//static constexpr const float c = 0.00833274827499629;
			//static constexpr const float d = -0.1666663723913334;
			//static constexpr const float e = 0.9999999573287034;

			// Scaled to second derivative
			static constexpr const float a = 2.586151293336854e-06;
			static constexpr const float b = -0.0001979765656985496;
			static constexpr const float c = 0.008332814670389913;
			static constexpr const float d = -0.16666638726299862;
			static constexpr const float e = 0.9999999469515957;

#if defined(ALBUMBOT_FAST_TRIG_SIMD) && ALBUMBOT_FAST_TRIG_SIMD
			// Parallel algorithm
			// a*x9 + b*x7 + c*x5 + d*x3 + e*x
			//
			// 1. x*x, a*x, b*x, c*x
			// 2. x2*x2, bx*x2, d*x2
			// 3. bx3+cx, dx2+e
			// 4. ax*x4, bx3scx*x4, dx2se*x
			// 5. ax5*x4
			// 6. ax9+bx7scx5
			// 7. ax9sbx7scx5+dx3sex
			//
			const float x2 = x*x;
			const float ax = a*x;
			const float bx = b*x;
			const float cx = c*x;
			const float x4 = x2*x2;
			const float bx3 = bx*x2;
			const float dx2 = d*x2;
			const float bx3scx = bx3+cx;
			const float dx2se = dx2+e;
			const float ax5 = ax*x4;
			const float bx7scx5 = bx3scx*x4;
			const float dx3sex = dx2se*x;
			const float ax9 = ax5*x4;
			const float ax9sbx7scx5 = ax9+bx7scx5;
			return ax9sbx7scx5+dx3sex;
#else
			// Serial algorithm
			const float x2 = x*x;
			return x*(x2*(x2*(x2*(a*x2 + b) + c) + d) + e); // Degree 9 with 5 coefficients and 6 multiplies
#endif
		}

		template<> inline double FastQSinInternal<double, 7>(const double x) noexcept
		{
			// Scaled to second derivative
			static constexpr const double a = -2.378150790269759e-08;
			static constexpr const double b = 2.7517220540920924e-06;
			static constexpr const double c = -0.00019840637004080698;
			static constexpr const double d = 0.008333328140526583;
			static constexpr const double e = -0.16666666460250168;
			static constexpr const double f = 0.999999999697273;

			const double x2 = x*x;
			return x*(x2*(x2*(x2*(x2*(a*x2 + b) + c) + d) + e) + f); // Degree 11 with 6 coefficients and 7 multiplies
		}

		template<> inline float FastQSinInternal<float, 7>(const float x) noexcept
		{
			// Scaled to second derivative
			static constexpr const float a = -2.378150790269759e-08;
			static constexpr const float b = 2.7517220540920924e-06;
			static constexpr const float c = -0.00019840637004080698;
			static constexpr const float d = 0.008333328140526583;
			static constexpr const float e = -0.16666666460250168;
			static constexpr const float f = 0.999999999697273;

			const float x2 = x*x;
			return x*(x2*(x2*(x2*(x2*(a*x2 + b) + c) + d) + e) + f); // Degree 11 with 6 coefficients and 7 multiplies
		}

		template<typename FloatType, int NumMultiplies> inline FloatType FastSinInternal(const double x) noexcept
		{
			const double x_cyc = x * vTauInv<double>::value;
			const FloatType x_norm = To32Bit((fmod(x_cyc) - 0.5) * vTau<double>::value);
			const MatchUInt_t<FloatType> x_norm_bits = FloatToBits(x_norm);
			const MatchUInt_t<FloatType> sign_bit = x_norm_bits & SignBit<FloatType>::value;
			const MatchUInt_t<FloatType> not_sign_bit = sign_bit ^ SignBit<FloatType>::value;
			const MatchUInt_t<FloatType> x_norm_pos_bits = x_norm_bits & ~sign_bit;
			const FloatType x_norm_pos = BitsToFloat<FloatType>(x_norm_pos_bits);
			const MatchUInt_t<FloatType> qsin_input_sign_bit =
				FloatToBits(vQuarterTau<FloatType>::value - x_norm_pos) & SignBit<FloatType>::value;
			const FloatType qsin_input_offset =
				vQuarterTau<FloatType>::value +
					BitsToFloat<FloatType>((qsin_input_sign_bit ^ SignBit<FloatType>::value) |
						FloatToBits(vQuarterTau<FloatType>::value));

			QSINLOG("FastSinInternal with degree " << NumMultiplies << " called:");
			QSINLOG("x_cyc: " << x_cyc);
			QSINLOG("x_norm: " << x_norm);
			QSINLOG("x_norm_bits: " << std::hex << x_norm_bits);
			QSINLOG("sign_bit: " << std::hex << sign_bit);
			QSINLOG("not_sign_bit: " << std::hex << not_sign_bit);
			QSINLOG("x_norm_pos_bits: " << std::hex << x_norm_pos_bits);
			QSINLOG("x_norm_pos: " << x_norm_pos);
			QSINLOG("qsin_input_sign_bit: " << std::hex << qsin_input_sign_bit);

			return BitsToFloat<FloatType>(not_sign_bit |
				FloatToBits(FastQSinInternal<FloatType, NumMultiplies>(qsin_input_offset +
					BitsToFloat<FloatType>(qsin_input_sign_bit | x_norm_pos_bits))));
		}

		template<typename FloatType, int NumMultiplies> inline FloatType FastCosInternal(const double x) noexcept
		{
			const double x_cyc = x * vTauInv<double>::value;
			const FloatType x_norm = To32Bit((fmod(x_cyc) - 0.5) * vTau<double>::value);
			const FloatType x_norm_abs = fabs(x_norm);

			const FloatType x_tri = vHalfTau<FloatType>::value - x_norm_abs;
			static constexpr const FloatType offset[2] = { FloatType(0.0l), vHalfTau<FloatType>::value };
			static constexpr const size_t sign_bit_shift = sizeof(FloatType) * 8 - 1;
			const MatchUInt_t<FloatType> cos_sign = SignBit<FloatType>::value & FloatToBits(vQuarterTau<FloatType>::value - x_tri);
			const FloatType cos_in = x_tri - offset[cos_sign >> sign_bit_shift];

			QSINLOG("FastCosInternal with degree " << NumMultiplies << " called:");
			QSINLOG("x_cyc: " << x_cyc);
			QSINLOG("x_norm: " << x_norm);
			QSINLOG("x_tri: " << x_tri);
			QSINLOG("cos_in: " << cos_in);

			return BitsToFloat<FloatType>(cos_sign | FloatToBits(detail::FastQCosInternal<FloatType, NumMultiplies>(cos_in)));
		}

#ifdef __AVX2__
		template<int NumMultiplies> inline __m128 FastCosInternal(__m256d x) noexcept
		{
			x = _mm256_mul_pd(x, _mm256_set1_pd(vTauInv<double>::value));
			x = _mm256_sub_pd(_mm256_sub_pd(x, _mm256_floor_pd(x)), _mm256_set1_pd(0.5));
			x = _mm256_mul_pd(x, _mm256_set1_pd(vTau<double>::value));
			__m128 x32 = _mm256_cvtpd_ps(x);
			x32 = _mm_andnot_ps(_mm_set1_ps(-0.0f), x32); // abs
			x32 = _mm_sub_ps(_mm_set1_ps(vHalfTau<float>::value), x32);
			static constexpr const int sign_bit_shift = sizeof(float) * 8 - 1;
			__m128 cos_in = x32;
			x32 = _mm_sub_ps(_mm_set1_ps(vQuarterTau<float>::value), x32);
			__m128i cos_sign = _mm_and_si128(_mm_set1_epi32(0x80000000ul), _mm_castps_si128(x32));
			__m128i x32i = _mm_srai_epi32(_mm_castps_si128(x32), sign_bit_shift);
			cos_in = _mm_sub_ps(cos_in, _mm_castsi128_ps(_mm_and_si128(x32i, _mm_castps_si128(_mm_set1_ps(vHalfTau<float>::value)))));
			return _mm_castsi128_ps(_mm_or_si128(cos_sign, _mm_castps_si128(detail::FastQCosInternal_v4f32<NumMultiplies>(cos_in))));
		}
#endif
	}

	template<int NumMultiplies, typename FloatType = float>
	inline FloatType FastSin(const double x) noexcept
	{
		static_assert(sizeof(float) == 4, "FastSin only supports 4-byte floats");
		static_assert(sizeof(double) == 8, "FastSin only supports 8-byte doubles");
		static_assert(IsFloatType<FloatType>::value, "FastSin only operates on floating point types");
		static_assert(NumMultiplies > 0, "FastSin requires 1 or more multiplies");

		return detail::FastSinInternal<FloatType, NumMultiplies>(x);
	}

	template<int NumMultiplies, typename FloatType = float>
	inline FloatType FastCos(const double x) noexcept
	{
		static_assert(sizeof(float) == 4, "FastCos only supports 4-byte floats");
		static_assert(sizeof(double) == 8, "FastCos only supports 8-byte doubles");
		static_assert(IsFloatType<FloatType>::value, "FastCos only operates on floating point types");
		static_assert(NumMultiplies > 0, "FastCos requires 1 or more multiplies");

		return detail::FastCosInternal<FloatType, NumMultiplies>(x);
	}

#ifdef __AVX2__
	template<int NumMultiplies>
	inline __m128 FastCos(__m256d vx) noexcept
	{
		static_assert(NumMultiplies > 0, "FastCos requires 1 or more multiplies");
		return detail::FastCosInternal<NumMultiplies>(vx);
	}
#endif

	template<bool bSine> struct FastSinusoid;
	template<> struct FastSinusoid<false>
	{
		template<int NumMultiplies, typename FloatType = float>
		static inline FloatType call(const double x) { return FastCos<NumMultiplies, FloatType>(x); }
	};
	template<> struct FastSinusoid<true>
	{
		template<int NumMultiplies, typename FloatType = float>
		static inline FloatType call(const double x) { return FastSin<NumMultiplies, FloatType>(x); }
	};

	namespace fast
	{
		template<typename FloatType = float>
		inline FloatType sin(const double x)
		{
			return FastSin<5, FloatType>(x);
		}

		template<typename FloatType = float>
		inline FloatType cos(const double x)
		{
			return FastCos<5, FloatType>(x);
		}

#ifdef __AVX2__
		inline __m128 cos(__m256d vx)
		{
			return FastCos<5>(vx);
		}
#endif
	}
}

