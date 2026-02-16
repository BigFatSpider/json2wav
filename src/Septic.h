// Copyright Dan Price 2026.

#pragma once

namespace json2wav
{
	template<typename T>
	class SepticFunctor
	{
	public:
		SepticFunctor(const T a_init, const T b_init, const T c_init, const T d_init,
			const T e_init, const T f_init, const T g_init, const T h_init) noexcept
			: a(a_init), b(b_init), c(c_init), d(d_init),
			e(e_init), f(f_init), g(g_init), h(h_init)
		{
		}

		T operator()(const T x) const
		{
			return ((((((a*x + b)*x + c)*x + d)*x + e)*x + f)*x + g)*x + h;
		}

	private:
		T a;
		T b;
		T c;
		T d;
		T e;
		T f;
		T g;
		T h;
	};

	using Septic32 = SepticFunctor<float>;
	using Septic64 = SepticFunctor<double>;
	using Septic = Septic32;
}

