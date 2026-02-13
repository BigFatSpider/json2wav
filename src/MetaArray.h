// Copyright Dan Price. All rights reserved.

#pragma once

namespace json2wav
{
	namespace Math
	{
		template<typename T> struct PreventFloat { typedef T type; };
		template<> struct PreventFloat<float> { typedef long type; };
		template<> struct PreventFloat<double> { typedef long type; };
		template<> struct PreventFloat<long double> { typedef long type; };

		template<typename T, typename PreventFloat<T>::type... args> struct ArrayHolder
		{
			static constexpr const T data[sizeof...(args)] = { static_cast<T>(args)... };
		};

		template<typename T, size_t N, template<size_t> typename F, T... args> struct generate_array_impl
		{
			typedef typename generate_array_impl<T, N - 1, F, F<N>::value, args...>::result result;
		};

		template<typename T, template<size_t> typename F, T... args> struct generate_array_impl<T, 0, F, args...>
		{
			typedef ArrayHolder<T, F<0>::value, args...> result;
		};

		template<typename T, size_t N, template<size_t> typename F> struct gen_arr
		{
			typedef typename generate_array_impl<T, N - 1, F>::result result;
		};

	}
}

