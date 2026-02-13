// Copyright Dan Price. All rights reserved.

#pragma once

#include "Memory.h"
#include <algorithm>
#include <type_traits>
#include <cmath>
#include <cstdint>

namespace json2wav
{
	namespace Utility
	{
		template<typename T, bool bOwner> struct SmartPtr;
		template<typename T> struct SmartPtr<T, false> { using type = WeakPtr<T>; };
		template<typename T> struct SmartPtr<T, true> { using type = SharedPtr<T>; };
		template<typename T, bool bOwner> using SmartPtr_t = typename SmartPtr<T, bOwner>::type;

		template<typename T, bool bOwner, bool bSmartPtr> struct Ptr;
		template<typename T, bool bOwner> struct Ptr<T, bOwner, false> { using type = T*; };
		template<typename T, bool bOwner> struct Ptr<T, bOwner, true> { using type = SmartPtr_t<T, bOwner>; };
		template<typename T, bool bOwner, bool bSmartPtr> using Ptr_t = typename Ptr<T, bOwner, bSmartPtr>::type;

		template<typename T, bool bSmartPtr> struct StrongPtr { using type = Ptr_t<T, true, bSmartPtr>; };
		template<typename T, bool bSmartPtr> using StrongPtr_t = typename StrongPtr<T, bSmartPtr>::type;

		template<typename T, template<typename> typename P> struct LockFtor
		{
			static SharedPtr<T> Lock(P<T> ptr)
			{
				SharedPtr<T> shptr(ptr);
				return shptr;
			}
		};
		template<typename T> struct LockFtor<T, SharedPtr>
		{ static SharedPtr<T> Lock(SharedPtr<T> ptr) { return ptr; } };
		template<typename T> struct LockFtor<T, WeakPtr>
		{ static SharedPtr<T> Lock(WeakPtr<T> ptr) { return ptr.lock(); } };
		template<typename T, template<typename> typename P>
		inline SharedPtr<T> Lock(P<T> ptr) { return LockFtor<T, P>::Lock(ptr); }
		template<typename T> inline T* Lock(T* ptr) { return ptr; }

		template<typename T>
		inline typename Vector<WeakPtr<T>>::iterator Find(
			const SharedPtr<T>& ptr,
			Vector<WeakPtr<T>>& vec)
		{
			return std::find_if(vec.begin(), vec.end(),
				[&ptr](const WeakPtr<T>& wkptr)
				{
					if (const SharedPtr<T> ptrLocked = wkptr.lock())
					{
						return ptrLocked == ptr;
					}
					return false;
				});
		}

		template<typename T>
		inline typename Vector<SharedPtr<T>>::iterator Find(
			const SharedPtr<T>& ptr,
			Vector<SharedPtr<T>>& vec)
		{
			return std::find_if(vec.begin(), vec.end(),
				[&ptr](const SharedPtr<T>& currentPtr)
				{
					return ptr == currentPtr;
				});
		}

		template<typename T>
		inline typename Vector<T*>::iterator Find(
			T* ptr,
			Vector<T*>& vec)
		{
			return std::find_if(vec.begin(), vec.end(),
				[ptr](T* currentPtr)
				{
					return ptr == currentPtr;
				});
		}

		template<typename T>
		inline bool Remove(
			const SharedPtr<T>& ptr,
			Vector<WeakPtr<T>>& vec)
		{
			const auto removeIt = std::remove_if(vec.begin(), vec.end(),
				[&ptr](const WeakPtr<T>& wkptr)
				{
					if (const SharedPtr<T> ptrLocked = wkptr.lock())
					{
						return ptrLocked == ptr;
					}
					return false;
				});
			if (removeIt != vec.end())
			{
				vec.erase(removeIt, vec.end());
				return true;
			}
			return false;
		}

		template<typename T>
		inline bool Remove(
			const SharedPtr<T>& ptr,
			Vector<SharedPtr<T>>& vec)
		{
			const auto removeIt = std::remove_if(vec.begin(), vec.end(),
				[&ptr](const SharedPtr<T>& currentPtr)
				{
					return ptr == currentPtr;
				});
			if (removeIt != vec.end())
			{
				vec.erase(removeIt, vec.end());
				return true;
			}
			return false;
		}

		template<typename T>
		inline bool Remove(
			T* ptr,
			Vector<T*>& vec)
		{
			const auto removeIt = std::remove_if(vec.begin(), vec.end(),
				[ptr](T* currentPtr)
				{
					return ptr == currentPtr;
				});
			if (removeIt != vec.end())
			{
				vec.erase(removeIt, vec.end());
				return true;
			}
			return false;
		}

		namespace detail
		{
			template<typename FloatType>
			inline FloatType DBToGain(const FloatType inDB)
			{
				static_assert(std::is_floating_point_v<FloatType>, "DBToGain(const FloatType) must take a floating point type");
				// DB = 10log10(P/P_ref)
				// DB = 20log10(V/V_ref)
				// DB/20 = log10(V/V_ref)
				// 10^(DB/20) = V/V_ref
				constexpr const FloatType div20(1 / static_cast<FloatType>(20.0L));
				return std::pow(static_cast<FloatType>(10.0L), inDB*div20);
			}

			template<typename FloatType>
			inline FloatType GainToDB(const FloatType inGain)
			{
				static_assert(std::is_floating_point_v<FloatType>, "GainToDB(const FloatType) must take a floating point type");
				// DB = 10log10(P/P_ref)
				// DB = 20log10(V/V_ref)
				constexpr const FloatType mul20(static_cast<FloatType>(20.0L));
				return mul20 * std::log10(inGain);
			}

			template<size_t sizecheck> struct FloatTypeBySizeStruct
			{
				using type =
					std::conditional_t<sizecheck <= sizeof(float), float, std::conditional_t<sizecheck <= sizeof(double), double, long double>>;
			};
			template<size_t floatsize> using FloatTypeBySize = typename FloatTypeBySizeStruct<floatsize>::type;
			template<typename T> using GetFloatType = std::conditional_t<std::is_floating_point_v<T>, T, FloatTypeBySize<sizeof(T)>>;

			/*template<bool bIsFloat, typename T>
			inline GetFloatType<T> DBToGain(const T inDB)
			{
				return DBToGain(static_cast<GetFloatType<T>>(inDB));
			}*/
		}

		template<typename T>
		inline detail::GetFloatType<T> DBToGain(const T inDB)
		{
			return detail::DBToGain(static_cast<detail::GetFloatType<T>>(inDB));
			//return detail::DBToGain<std::is_floating_point_v<T>>(inDB);
		}

		template<> inline float DBToGain<float>(const float inDB) { return detail::DBToGain(inDB); }
		template<> inline double DBToGain<double>(const double inDB) { return detail::DBToGain(inDB); }
		template<> inline long double DBToGain<long double>(const long double inDB) { return detail::DBToGain(inDB); }

		template<typename T>
		inline void DBToGain(T& outGain, const T inDB) { outGain = DBToGain(inDB); }

		template<typename T>
		inline detail::GetFloatType<T> GainToDB(const T inGain)
		{
			return detail::GainToDB(static_cast<detail::GetFloatType<T>>(inGain));
		}

		template<> inline float GainToDB<float>(const float inGain) { return detail::GainToDB(inGain); }
		template<> inline double GainToDB<double>(const double inGain) { return detail::GainToDB(inGain); }
		template<> inline long double GainToDB<long double>(const long double inGain) { return detail::GainToDB(inGain); }

		template<typename T>
		inline void GainToDB(T& outDB, const T inGain) { outDB = GainToDB(inGain); }

		namespace detail
		{
			template<size_t IntSize> struct NextPow2Impl;

			template<> struct NextPow2Impl<1>
			{
				template<typename IntType>
				static IntType Do(IntType val)
				{
					static_assert(std::is_integral_v<IntType>, "json2wav::Utility::detail::NextPow2Impl::Do() only operates on integral types");
					static_assert(sizeof(IntType) == 1, "json2wav::Utility::detail::NextPow2Impl<1>::Do() only operates on integral types of size 1");

					--val;
					val |= val >> 1;
					val |= val >> 2;
					val |= val >> 4;
					return val + 1;
				}
			};

			template<> struct NextPow2Impl<2>
			{
				template<typename IntType>
				static IntType Do(IntType val)
				{
					static_assert(std::is_integral_v<IntType>, "json2wav::Utility::detail::NextPow2Impl::Do() only operates on integral types");
					static_assert(sizeof(IntType) == 2, "json2wav::Utility::detail::NextPow2Impl<2>::Do() only operates on integral types of size 2");

					--val;
					val |= val >> 1;
					val |= val >> 2;
					val |= val >> 4;
					val |= val >> 8;
					return val + 1;
				}
			};

			template<> struct NextPow2Impl<4>
			{
				template<typename IntType>
				static IntType Do(IntType val)
				{
					static_assert(std::is_integral_v<IntType>, "json2wav::Utility::detail::NextPow2Impl::Do() only operates on integral types");
					static_assert(sizeof(IntType) == 4, "json2wav::Utility::detail::NextPow2Impl<4>::Do() only operates on integral types of size 4");

					--val;
					val |= val >> 1;
					val |= val >> 2;
					val |= val >> 4;
					val |= val >> 8;
					val |= val >> 16;
					return val + 1;
				}
			};

			template<> struct NextPow2Impl<8>
			{
				template<typename IntType>
				static IntType Do(IntType val)
				{
					static_assert(std::is_integral_v<IntType>, "json2wav::Utility::detail::NextPow2Impl::Do() only operates on integral types");
					static_assert(sizeof(IntType) == 8, "json2wav::Utility::detail::NextPow2Impl<8>::Do() only operates on integral types of size 8");

					--val;
					val |= val >> 1;
					val |= val >> 2;
					val |= val >> 4;
					val |= val >> 8;
					val |= val >> 16;
					val |= val >> 32;
					return val + 1;
				}
			};

			template<> struct NextPow2Impl<16>
			{
				template<typename IntType>
				static IntType Do(IntType val)
				{
					static_assert(std::is_integral_v<IntType>, "json2wav::Utility::detail::NextPow2Impl::Do() only operates on integral types");
					static_assert(sizeof(IntType) == 16, "json2wav::Utility::detail::NextPow2Impl<16>::Do() only operates on integral types of size 16");

					--val;
					val |= val >> 1;
					val |= val >> 2;
					val |= val >> 4;
					val |= val >> 8;
					val |= val >> 16;
					val |= val >> 32;
					val |= val >> 64;
					return val + 1;
				}
			};
		}

		template<typename IntType>
		inline IntType NextPow2(const IntType val)
		{
			static_assert(std::is_integral_v<IntType>, "json2wav::Utility::NextPow2() only operates on integral types");
			static_assert((sizeof(IntType) & (sizeof(IntType) - 1)) == 0, "json2wav::Utility::NextPow2() only operates on integral types of power-of-2 size");
			return detail::NextPow2Impl<sizeof(IntType)>::Do(val);
		}

		template<bool B, typename T, typename F> struct TypeIf;
		template<typename T, typename F> struct TypeIf<false, T, F>
		{
			using type = F;
			template<typename U, typename V>
			static constexpr inline type value(U&& u, V&& v)
			{
				type val(std::forward<V>(v));
				return val;
			}
		};
		template<typename T, typename F> struct TypeIf<true, T, F>
		{
			using type = T;
			template<typename U, typename V>
			static constexpr inline type value(U&& u, V&& v)
			{
				type val(std::forward<U>(u));
				return val;
			}
		};
		template<bool B, typename T, typename F> using TypeIf_t = typename TypeIf<B, T, F>::type;
		template<bool B, typename R, typename T, typename F>
		inline constexpr R TypeIf_v(T&& t, F&& f)
		{
			R r(TypeIf<B, T, F>::value(std::forward<T>(t), std::forward<F>(f)));
			return r;
		}

		inline bool FloatAbsEqual(float lhs, float rhs)
		{
			static constexpr const unsigned char nonsignmask(0x7f);
			reinterpret_cast<unsigned char*>(&lhs)[3] &= nonsignmask;
			reinterpret_cast<unsigned char*>(&rhs)[3] &= nonsignmask;
			return lhs == rhs;
		}

		inline bool FloatAbsLess(float lhs, float rhs)
		{
			static constexpr const unsigned char nonsignmask(0x7f);
			reinterpret_cast<unsigned char*>(&lhs)[3] &= nonsignmask;
			reinterpret_cast<unsigned char*>(&rhs)[3] &= nonsignmask;
			return lhs < rhs;
		}

		inline bool FloatAbsLessEqual(float lhs, float rhs)
		{
			static constexpr const unsigned char nonsignmask(0x7f);
			reinterpret_cast<unsigned char*>(&lhs)[3] &= nonsignmask;
			reinterpret_cast<unsigned char*>(&rhs)[3] &= nonsignmask;
			return lhs <= rhs;
		}

		inline bool FloatAbsNotEqual(float lhs, float rhs)
		{
			return !FloatAbsEqual(lhs, rhs);
		}

		inline bool FloatAbsGreater(float lhs, float rhs)
		{
			return !FloatAbsLessEqual(lhs, rhs);
		}

		inline bool FloatAbsGreaterEqual(float lhs, float rhs)
		{
			return !FloatAbsLess(lhs, rhs);
		}

		template<typename T>
		inline T ceil_log2(T v)
		{
			static_assert(std::is_unsigned_v<T>, "json2wav::ceil_log2() only works on unsigned integers");

			static const uint64_t t[6] = {
				0xffffffff00000000ull,
				0x00000000ffff0000ull,
				0x000000000000ff00ull,
				0x00000000000000f0ull,
				0x000000000000000cull,
				0x0000000000000002ull
			};

			uint64_t x = v;
			uint_fast8_t y = !((x & (x - 1)) == 0);
			uint_fast8_t j = 32;

			uint_fast8_t k = (((x & t[0]) == 0) ? 0 : j);
			y += k;
			for (uint64_t i = 1; i < 6; ++i)
			{
				x >>= k;
				j >>= 1;
				k = (((x & t[i]) == 0) ? 0 : j);
				y += k;
			}

			return static_cast<T>(y);
		}

#if 0
		template<typename T>
		inline T uintlog2(T v)
		{
			static_assert(false, "json2wav::uintlog2() hasn't been tested");
			static_assert(std::is_unsigned_v<T>, "json2wav::uintlog2() only works on unsigned integers");
			T v;
			T r;
			T shift;
			if constexpr (sizeof(T) == 8)
			{
				r = (v > 0xffffffff) << 5;
				v >>= r;
				shift = (v > 0xffff) << 4;
				v >>= shift;
				r |= shift;
				shift = (v > 0xff) << 3;
				v >>= shift;
				r |= shift;
				shift = (v > 0xf) << 2;
				v >>= shift;
				r |= shift;
			}
			else if constexpr (sizeof(T) == 4)
			{
				r = (v > 0xffff) << 4;
				v >>= r;
				shift = (v > 0xff) << 3;
				v >>= shift;
				r |= shift;
				shift = (v > 0xf) << 2;
				v >>= shift;
				r |= shift;
			}
			else if constexpr (sizeof(T) == 2)
			{
				r = (v > 0xff) << 3;
				v >>= r;
				shift = (v > 0xf) << 2;
				v >>= shift;
				r |= shift;
			}
			else
			{
				r = (v > 0xf) << 2;
				v >>= r;
			}
			shift = (v > 0x3) << 1;
			v >>= shift;
			r |= shift;
			r |= (v >> 1);
			return r;
		}
#endif

	}
}

