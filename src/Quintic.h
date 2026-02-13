// Copyright Dan Price. All rights reserved.

#pragma once

namespace json2wav
{
	template<typename T>
	class QuinticFunctor
	{
	public:
		QuinticFunctor(const T a_init, const T b_init, const T c_init, const T d_init
			, const T e_init, const T f_init) noexcept
			: a(a_init), b(b_init), c(c_init), d(d_init)
			, e(e_init), f(f_init)
		{
		}

		T operator()(const T x) const
		{
			return ((((a*x + b)*x + c)*x + d)*x + e)*x + f;
		}

	private:
		T a;
		T b;
		T c;
		T d;
		T e;
		T f;
	};

	using Quintic32 = QuinticFunctor<float>;
	using Quintic64 = QuinticFunctor<double>;
	using Quintic = Quintic32;
}

